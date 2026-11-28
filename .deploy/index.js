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
                imageStorage: 'opfs',
                hostfs,
                onImageUpdateAvailable(image) {
                    const useRemote = window.confirm(
                        `${image.name} changed on the server. Replacing it discards local image changes and guest RAM.\n\n` +
                        'Use the updated server image? Press Cancel to keep your local image.'
                    );
                    if (useRemote) emulator.applyImageUpdates([image.name]).catch(reportError);
                },
                onLoadingChange: setLoading,
            });
            await emulator.start();
            const size = await emulator.diskImageUsage();
            console.log(
                'Disk image storage:',
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
            hostfs?.reset();
            await emulator?.clearDiskImages();
            console.log('Cached disk images cleared');
        } catch (error) {
            reportError(error);
        }
    });

    window.addEventListener('load', load);
})();
