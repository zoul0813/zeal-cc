(() => {
    const canvas = document.getElementById('canvas');
    const viewport = document.getElementById('viewport');
    const buttons = [...document.querySelectorAll('header button')];
    let emulator = null;
    let hostfs = null;

    function reportError(error) {
        console.error(error);
    }

    function setLoading(loading) {
        buttons.forEach(button => {
            button.disabled = loading;
        });
    }

    function attachButton(selector, handler) {
        document.querySelector(selector).addEventListener('click', handler);
    }

    async function load() {
        setLoading(true);
        try {
            hostfs = await BundledHostFS.load('hostfs', {onError: reportError});
            emulator = new ZealNative({
                canvas,
                romdisk: 'default.img',
                eeprom: 'eeprom.img',
                tf: 'tf.img',
                imagePersistence: true,
                hostfs,
                onImageConflict(image) {
                    const useRemote = window.confirm(
                        `${image.key} changed on the server, but your local image has saved changes.\n\n` +
                        'Use the updated server image? Press Cancel to keep your local image.'
                    );
                    return useRemote ? 'remote' : 'local';
                },
                onLoadingChange: setLoading,
            });
            await emulator.start();
            const size = await emulator.diskImageUsage();
            console.log(
                'Total IndexedDB usage:',
                `${size}B`,
                `${(size / 1024).toFixed(2)}K`,
                `${(size / 1024 / 1024).toFixed(2)}M`
            );
        } catch (error) {
            reportError(error);
        } finally {
            setLoading(false);
        }
    }

    viewport.addEventListener('click', () => canvas.focus());

    async function reload() {
        hostfs?.reset();
        return emulator?.reload();
    }

    attachButton('#btn-unmute', () => {
        emulator?.resumeAudio().then(() => canvas.focus()).catch(reportError);
    });
    attachButton('#btn-reset', () => reload().catch(reportError));
    attachButton('#btn-toggle-fps', () => {
        emulator?.toggleFps();
        canvas.focus();
    });
    attachButton('#btn-save', async () => {
        try {
            const saved = await emulator?.saveDiskImages();
            console.log(`Disk images saved: ${saved?.join(', ') || 'none'}`);
        } catch (error) {
            reportError(error);
        }
        canvas.focus();
    });
    attachButton('#btn-clear', async () => {
        try {
            await emulator?.clearDiskImages();
            console.log('Cached disk images cleared');
            await reload();
        } catch (error) {
            reportError(error);
        }
    });

    window.addEventListener('load', load);
})();
