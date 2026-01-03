const DB_NAME = "ZealStorage";
const DB_VERSION = 1;
const STORE_NAME = "images";

const EEPROM_NAME="eeprom.img";
const ROMDISK_NAME="default.img";
const TF_NAME="tf.img";
const HOSTFS_NAME="hostfs";

function openDB() {
    return new Promise((resolve, reject) => {
        const request = indexedDB.open(DB_NAME, DB_VERSION);

        request.onerror = () => reject(request.error);
        request.onsuccess = () => resolve(request.result);

        request.onupgradeneeded = (event) => {
            const db = event.target.result;
            if (!db.objectStoreNames.contains(STORE_NAME)) {
                db.createObjectStore(STORE_NAME);
            }
        };
    });
}

async function dbUsage() {
    const db = await openDB();
    return new Promise((resolve, reject) => {
        let total = 0;
        const transaction = db.transaction([STORE_NAME], "readonly");
        const store = transaction.objectStore(STORE_NAME);
        const request = store.openCursor();

        request.onsuccess = (event) => {
            const cursor = event.target.result;
            if (cursor) {
                const value = cursor.value;
                if (value && value.byteLength !== undefined) {
                    total += value.byteLength;
                } else if (value && value.length !== undefined) {
                    total += value.length;
                }
                cursor.continue();
            } else {
                resolve(total);
            }
        };
        request.onerror = () => reject(request.error);
    });
}

async function dbWrite(name, data) {
    const db = await openDB();
    return new Promise((resolve, reject) => {
        const transaction = db.transaction([STORE_NAME], "readwrite");
        const store = transaction.objectStore(STORE_NAME);
        const request = store.put(data, name);

        request.onsuccess = () => resolve();
        request.onerror = () => reject(request.error);
    });
}

async function dbRead(name) {
    const db = await openDB();
    return new Promise((resolve, reject) => {
        const transaction = db.transaction([STORE_NAME], "readonly");
        const store = transaction.objectStore(STORE_NAME);
        const request = store.get(name);

        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
}

async function dbDelete(name) {
    const db = await openDB();
    return new Promise((resolve, reject) => {
        const transaction = db.transaction([STORE_NAME], "readwrite");
        const store = transaction.objectStore(STORE_NAME);
        const request = store.delete(name);

        request.onsuccess = () => resolve();
        request.onerror = () => reject(request.error);
    });
}

function getMetaKey(name) {
    return `${name}:meta`;
}

async function fetchImage(name, options = {}) {
    const { save = true, url = name } = options;
    if (!name) throw new Error("fetchImage called without a filename");

    const metaKey = getMetaKey(name);
    const cachedMeta = await dbRead(metaKey);

    if (save && cachedMeta) {
        const response = await fetch(url, { method: "HEAD" });
        const etag = response.headers.get("ETag");
        if (etag == cachedMeta) {
            const image = await dbRead(name);
            if (image) return image;
        }
    }

    return fetch(url)
        .then(async (response) => {
            if (!response.ok) throw new Error(`Failed to fetch ${url}`);
            return {
                buffer: await response.arrayBuffer(),
                etag: response.headers.get("ETag"),
            };
        })
        .then(({ buffer, etag }) => ({
            buffer: new Uint8Array(buffer), etag
        }))
        .then(async ({ buffer, etag }) => {
            if (save) {
                await dbWrite(name, buffer);
                await dbWrite(metaKey, etag);
            }
            return buffer;
        });
}

async function fetchHostFS(name, options = {}) {
    const { indexName = "index.json" } = options;
    if (!name) throw new Error("fetchHostFS called without a folder name");

    const indexUrl = `${name}/${indexName}`;
    const indexResponse = await fetch(indexUrl);
    if (!indexResponse.ok) {
        throw new Error(`Failed to fetch ${indexUrl}`);
    }
    const fileList = await indexResponse.json();
    if (!Array.isArray(fileList)) {
        throw new Error(`Invalid hostfs index at ${indexUrl}`);
    }

    const files = await Promise.all(
        fileList.map(async (path) => {
            const fileUrl = `${name}/${path}`;
            const response = await fetch(fileUrl);
            if (!response.ok) {
                throw new Error(`Failed to fetch ${fileUrl}`);
            }
            const buffer = await response.arrayBuffer();
            return { path, buffer: new Uint8Array(buffer) };
        })
    );
    return files;
}

function dispatchKeyCode(e, name, key, code, keyCode) {
    e.preventDefault();
    e.stopPropagation();
    const synthetic = new KeyboardEvent(name, {
        key,
        code,
        keyCode,
        which: keyCode,
        bubbles: true,
        cancelable: true,
    });
    e.target.dispatchEvent(synthetic);
}

function attachButton(selector, handler) {
    document.querySelector(selector).addEventListener("click", handler);
}

(() => {
    const canvas = document.getElementById("canvas");
    const viewport = document.getElementById("viewport");
    viewport.addEventListener("click", () => {
        canvas.focus();
    });

    let moduleInstance = null;
    function loadModule(romdiskImage, eepromImage, tfImage, hostFiles) {
        const defaultModule = {
            arguments: [
                "-r",
                "/roms/default.img",
                "-e",
                "/roms/eeprom.img",
                "-t",
                "/roms/tf.img",
                "-H",
                "/hostfs"
            ],
            print: function (text) {
                console.log("Log: " + text);
            },
            printErr: function (text) {
                console.log("Error: " + text);
            },
            canvas: (function () {
                return canvas;
            })(),
            onRuntimeInitialized: function () {
                if (!this.FS.analyzePath("/roms").exists) {
                    this.FS.mkdir("/roms");
                }
                if (!this.FS.analyzePath("/hostfs").exists) {
                    this.FS.mkdir("/hostfs");
                }
                this.FS.writeFile("/roms/default.img", romdiskImage);
                this.FS.writeFile("/roms/eeprom.img", eepromImage);
                this.FS.writeFile("/roms/tf.img", tfImage);
                if (Array.isArray(hostFiles)) {
                    hostFiles.forEach((file) => {
                        const target = `/hostfs/${file.path}`;
                        const parts = target.split("/").slice(1, -1);
                        let current = "";
                        parts.forEach((part) => {
                            current += `/${part}`;
                            if (!this.FS.analyzePath(current).exists) {
                                this.FS.mkdir(current);
                            }
                        });
                        this.FS.writeFile(target, file.buffer);
                    });
                }
                canvas.setAttribute("tabindex", "0");
                canvas.focus();
            },
        };
        NativeModule(defaultModule).then((mod) => (moduleInstance = mod));
    }

    async function load() {
        [romdiskImage, eepromImage, tfImage, hostFiles] = await Promise.all([
            fetchImage(ROMDISK_NAME),
            fetchImage(EEPROM_NAME),
            fetchImage(TF_NAME),
            fetchHostFS(HOSTFS_NAME),
        ]).catch((err) => console.error(err));
        dbUsage().then((size) => {
            console.log(
                "Total IndexedDB usage:",
                `${size}B`,
                `${(size / 1024).toFixed(2)}K`,
                `${(size / 1024 / 1024).toFixed(2)}M`
            );
        });
        loadModule(romdiskImage, eepromImage, tfImage, hostFiles);
    }

    async function reset() {
        moduleInstance?._zeal_exit();
        moduleInstance = null;
        setTimeout(() => {
            load();
        }, 100);
    }

    window.addEventListener("load", load);

    function resumeAudioIfNeeded() {
        if (
            NativeModule.audioContext &&
            NativeModule.audioContext.state === "suspended"
        ) {
            NativeModule.audioContext.resume().then(() => {
                console.log("Audio context resumed");
            });
        }
        canvas.focus();
    }

    attachButton("#btn-unmute", resumeAudioIfNeeded);
    attachButton("#btn-reset", reset);

    window.toggleFPS = () => {
        if (!moduleInstance) return;
        const show_fps = !!moduleInstance.getValue(
            moduleInstance._show_fps,
            "i8"
        );
        console.log("show_fps", show_fps);
        moduleInstance.setValue(
            moduleInstance._show_fps,
            !show_fps ? 1 : 0,
            "i8"
        );
        canvas.focus();
    };
    attachButton("#btn-toggle-fps", toggleFPS);

    attachButton("#btn-save", async () => {
        if (!moduleInstance) {
            console.error("no moduleInstance, can not access images");
            return;
        }
        try {
            const eeprom = moduleInstance.FS.readFile("/eeprom.img");
            const tf = moduleInstance.FS.readFile("/tf.img");
            await dbWrite("eeprom.img", eeprom);
            await dbWrite("tf.img", tf);
            console.log("Images saved");
        } catch (err) {
            console.error("Error saving images", err);
        }
        canvas.focus();
    });
    attachButton("#btn-clear", async () => {
        try {
            await dbDelete(ROMDISK_NAME);
            await dbDelete(EEPROM_NAME);
            await dbDelete(TF_NAME);
            console.log("Images deleted");
            return reset();
        } catch (err) {
            console.error("Error deleting images", err);
        }
    });
})();
