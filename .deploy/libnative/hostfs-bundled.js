(() => {
    const STATUS = Object.freeze({
        SUCCESS: 0,
        FAILURE: 1,
        NO_SUCH_ENTRY: 4,
        CANNOT_REGISTER_MORE: 20,
        NO_MORE_ENTRIES: 21,
    });

    const OPERATION = Object.freeze({
        OPEN: 1,
        STAT: 2,
        READ: 3,
        WRITE: 4,
        CLOSE: 5,
        OPENDIR: 6,
        READDIR: 7,
        MKDIR: 8,
        RM: 9,
    });

    const OPEN_FLAG = Object.freeze({
        WRONLY: 1,
        RDWR: 2,
        TRUNC: 1 << 2,
        APPEND: 2 << 2,
        CREAT: 4 << 2,
    });

    class BundledHostFS {
        constructor(root, {indexName = 'index.json', onError = null} = {}) {
            this.root = root.replace(/\/+$/, '');
            this.indexName = indexName;
            this.onError = onError;
            this.entries = new Map([['', {directory: true}]]);
            this.descriptors = new Array(256).fill(null);
            this.module = null;
        }

        static async load(root, options = {}) {
            const hostfs = new BundledHostFS(root, options);
            await hostfs.loadIndex();
            return hostfs;
        }

        async loadIndex() {
            const indexUrl = `${this.root}/${this.indexName}`;
            const response = await fetch(indexUrl);
            if (!response.ok) throw new Error(`Failed to fetch ${indexUrl}`);
            const paths = await response.json();
            if (!Array.isArray(paths) || paths.some(path => typeof path !== 'string')) {
                throw new TypeError(`Invalid HostFS index at ${indexUrl}`);
            }

            await Promise.all(paths.map(async rawPath => {
                const parts = this.splitPath(rawPath);
                if (!parts.length) return;
                this.ensureDirectories(parts.slice(0, -1));
                const path = parts.join('/');
                const fileResponse = await fetch(`${this.root}/${parts.map(encodeURIComponent).join('/')}`);
                if (!fileResponse.ok) throw new Error(`Failed to fetch ${this.root}/${path}`);
                this.entries.set(path, {
                    directory: false,
                    data: new Uint8Array(await fileResponse.arrayBuffer()),
                });
            }));
            this.initialEntries = this.cloneEntries(this.entries);
        }

        cloneEntries(entries) {
            return new Map([...entries].map(([path, entry]) => [path, entry.directory
                ? {directory: true}
                : {directory: false, data: entry.data.slice()}]));
        }

        reset() {
            if (!this.initialEntries) throw new Error('BundledHostFS has not loaded');
            this.entries = this.cloneEntries(this.initialEntries);
            this.descriptors.fill(null);
        }

        bindModule(module) {
            this.module = module;
            return this;
        }

        splitPath(path) {
            if (typeof path !== 'string' || path.includes('\\')) {
                throw new DOMException('Invalid HostFS path', 'SecurityError');
            }
            let relative = path;
            const drive = relative.match(/^([A-Za-z]):(.*)$/);
            if (drive) {
                if (drive[1].toLowerCase() !== 'h' ||
                    (drive[2] !== '' && !drive[2].startsWith('/'))) {
                    throw new DOMException('Invalid HostFS drive path', 'SecurityError');
                }
                relative = drive[2];
            }
            const parts = relative.replace(/^\/+/, '').split('/')
                .filter(part => part !== '' && part !== '.');
            if (parts.some(part => part === '..')) {
                throw new DOMException('HostFS path escapes bundled root', 'SecurityError');
            }
            return parts;
        }

        ensureDirectories(parts) {
            let path = '';
            for (const part of parts) {
                path = path ? `${path}/${part}` : part;
                const entry = this.entries.get(path);
                if (entry && !entry.directory) {
                    throw new DOMException('Path component is not a directory', 'TypeMismatchError');
                }
                if (!entry) this.entries.set(path, {directory: true});
            }
        }

        pathFor(path) {
            return this.splitPath(path).join('/');
        }

        entryAt(path, directory = null) {
            const entry = this.entries.get(path);
            if (!entry) throw new DOMException('HostFS entry not found', 'NotFoundError');
            if (directory !== null && entry.directory !== directory) {
                throw new DOMException('HostFS entry has wrong type', 'TypeMismatchError');
            }
            return entry;
        }

        parentFor(path) {
            const parts = this.splitPath(path);
            if (!parts.length) throw new DOMException('Root has no parent entry', 'SecurityError');
            const name = parts.pop();
            const parentPath = parts.join('/');
            this.entryAt(parentPath, true);
            return {parentPath, name, path: [...parts, name].join('/')};
        }

        allocate(descriptor) {
            const index = this.descriptors.indexOf(null);
            if (index < 0) return -1;
            this.descriptors[index] = descriptor;
            return index;
        }

        descriptorAt(index, directory = null) {
            const descriptor = this.descriptors[index];
            if (!descriptor || (directory !== null && descriptor.directory !== directory)) {
                throw new DOMException('Invalid HostFS descriptor', 'InvalidStateError');
            }
            return descriptor;
        }

        complete(status, registers = [0, 0, 0, 0, 0, 0]) {
            this.module._hostfs_web_complete(status, ...registers);
        }

        writeGuest(address, bytes) {
            if (!bytes.length) return;
            const pointer = this.module._malloc(bytes.length);
            if (!pointer) throw new Error('Unable to allocate HostFS transfer buffer');
            try {
                this.module.HEAPU8.set(bytes, pointer);
                this.module._hostfs_web_write_guest(address, pointer, bytes.length);
            } finally {
                this.module._free(pointer);
            }
        }

        formatName(name) {
            const output = new Uint8Array(16);
            const encoded = new TextEncoder().encode(name);
            if (encoded.length <= 16) output.set(encoded);
            else {
                output.set(encoded.subarray(0, 15));
                output[15] = '~'.charCodeAt(0);
            }
            return output;
        }

        async open(request) {
            const {path, name} = this.parentFor(request.path);
            const access = request.flags & 3;
            if (access > OPEN_FLAG.RDWR) throw new DOMException('Invalid open mode', 'TypeError');
            let entry = this.entries.get(path);
            if (!entry && (request.flags & OPEN_FLAG.CREAT)) {
                entry = {directory: false, data: new Uint8Array()};
                this.entries.set(path, entry);
            }
            if (!entry) throw new DOMException('HostFS entry not found', 'NotFoundError');
            if (!entry.directory && (request.flags & OPEN_FLAG.TRUNC) && access !== 0) {
                entry.data = new Uint8Array();
            }
            const index = this.allocate({
                path,
                name,
                directory: entry.directory,
                readable: access !== OPEN_FLAG.WRONLY,
                writable: access !== 0,
                append: !!(request.flags & OPEN_FLAG.APPEND),
                entries: null,
                position: 0,
            });
            if (index < 0) return this.complete(STATUS.CANNOT_REGISTER_MORE);
            const size = entry.directory ? 0 : Math.min(entry.data.length, 0xffffffff) >>> 0;
            this.complete(STATUS.SUCCESS, [
                size & 0xff, (size >>> 8) & 0xff, (size >>> 16) & 0xff,
                (size >>> 24) & 0xff, index, entry.directory ? 1 : 0,
            ]);
        }

        async opendir(request) {
            const path = this.pathFor(request.path);
            this.entryAt(path, true);
            const index = this.allocate({
                path,
                name: path.split('/').at(-1) || '',
                directory: true,
                entries: null,
                position: 0,
            });
            if (index < 0) return this.complete(STATUS.CANNOT_REGISTER_MORE);
            this.complete(STATUS.SUCCESS, [0, 0, 0, 0, index, 1]);
        }

        async stat(request) {
            const descriptor = this.descriptorAt(request.descriptor);
            const entry = this.entryAt(descriptor.path, descriptor.directory);
            const bytes = new Uint8Array(28);
            if (!entry.directory) new DataView(bytes.buffer).setUint32(0, entry.data.length, true);
            bytes.set(this.formatName(descriptor.name), 12);
            this.writeGuest(request.guestAddress, entry.directory ? bytes : bytes.subarray(4));
            this.complete(STATUS.SUCCESS);
        }

        async read(request) {
            const descriptor = this.descriptorAt(request.descriptor, false);
            if (!descriptor.readable) throw new DOMException('Descriptor is not readable', 'NotAllowedError');
            const entry = this.entryAt(descriptor.path, false);
            const bytes = entry.data.slice(request.offset, request.offset + request.length);
            this.writeGuest(request.guestAddress, bytes);
            this.complete(STATUS.SUCCESS, [0, 0, 0, 0, bytes.length & 0xff, bytes.length >>> 8]);
        }

        async write(request) {
            const descriptor = this.descriptorAt(request.descriptor, false);
            if (!descriptor.writable) throw new DOMException('Descriptor is not writable', 'NotAllowedError');
            const entry = this.entryAt(descriptor.path, false);
            const position = descriptor.append ? entry.data.length : request.offset;
            const size = Math.max(entry.data.length, position + request.data.length);
            const data = new Uint8Array(size);
            data.set(entry.data);
            data.set(request.data, position);
            entry.data = data;
            this.complete(STATUS.SUCCESS, [
                0, 0, 0, 0, request.data.length & 0xff, request.data.length >>> 8,
            ]);
        }

        async close(request) {
            this.descriptorAt(request.descriptor);
            this.descriptors[request.descriptor] = null;
            this.complete(STATUS.SUCCESS);
        }

        async readdir(request) {
            const descriptor = this.descriptorAt(request.descriptor, true);
            if (!descriptor.entries) {
                const prefix = descriptor.path ? `${descriptor.path}/` : '';
                descriptor.entries = [...this.entries.entries()]
                    .filter(([path]) => path.startsWith(prefix) &&
                        path !== descriptor.path && !path.slice(prefix.length).includes('/'))
                    .map(([path, entry]) => ({name: path.slice(prefix.length), ...entry}))
                    .sort((a, b) => a.name.localeCompare(b.name));
            }
            const entry = descriptor.entries[descriptor.position++];
            if (!entry) return this.complete(STATUS.NO_MORE_ENTRIES);
            const bytes = new Uint8Array(17);
            bytes[0] = entry.directory ? 0 : 1;
            bytes.set(this.formatName(entry.name), 1);
            this.writeGuest(request.guestAddress, bytes);
            this.complete(STATUS.SUCCESS);
        }

        async mkdir(request) {
            const {path} = this.parentFor(request.path);
            if (this.entries.has(path)) {
                throw new DOMException('HostFS entry already exists', 'InvalidModificationError');
            }
            this.entries.set(path, {directory: true});
            this.complete(STATUS.SUCCESS);
        }

        async remove(request) {
            const {path} = this.parentFor(request.path);
            const entry = this.entryAt(path);
            if (entry.directory && [...this.entries.keys()].some(candidate => candidate.startsWith(`${path}/`))) {
                throw new DOMException('HostFS directory is not empty', 'InvalidModificationError');
            }
            this.entries.delete(path);
            this.complete(STATUS.SUCCESS);
        }

        diagnose(operation, error) {
            console.error(`[BundledHostFS] ${operation} failed`, error);
            this.onError?.(operation, error);
        }

        statusFor(error) {
            return error?.name === 'NotFoundError' ? STATUS.NO_SUCH_ENTRY : STATUS.FAILURE;
        }

        start(request) {
            const operations = {
                [OPERATION.OPEN]: this.open,
                [OPERATION.STAT]: this.stat,
                [OPERATION.READ]: this.read,
                [OPERATION.WRITE]: this.write,
                [OPERATION.CLOSE]: this.close,
                [OPERATION.OPENDIR]: this.opendir,
                [OPERATION.READDIR]: this.readdir,
                [OPERATION.MKDIR]: this.mkdir,
                [OPERATION.RM]: this.remove,
            };
            const operation = operations[request.operation];
            if (!operation) return this.complete(STATUS.FAILURE);
            Promise.resolve(operation.call(this, request)).catch(error => {
                this.diagnose(`operation ${request.operation}`, error);
                this.complete(this.statusFor(error));
            });
        }
    }

    window.BundledHostFS = BundledHostFS;
})();
