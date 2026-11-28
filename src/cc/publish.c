#include <core.h>
#include <zos_vfs.h>
#include "publish.h"

static unsigned char buffer[256];
static zos_stat_t info;

/* ZOS has no rename syscall. Back up an existing destination before publishing. */
static int copy_file(const char *src, const char *dst) {
    zos_dev_t in = open(src, O_RDONLY);
    if (in < 0) return -in;
    zos_dev_t out = open(dst, O_WRONLY | O_CREAT | O_TRUNC);
    int err = out < 0 ? -out : 0;
    while (!err) {
        uint16_t n = sizeof(buffer);
        err = read(in, buffer, &n);
        if (err || !n) break;
        uint16_t written = n;
        err = write(out, buffer, &written);
        if (!err && written != n) err = ERR_FAILURE;
    }
    if (out >= 0 && close(out) && !err) err = ERR_FAILURE;
    if (close(in) && !err) err = ERR_FAILURE;
    return err;
}

int publish(const char *src, const char *dst, int *keep_backup) {
    int err = 0;
    int lookup = stat(dst, &info);
    int existing = lookup == 0;
    if (lookup && lookup != ERR_NO_SUCH_ENTRY) err = lookup;
    if (existing && D_ISDIR(info.s_flags)) err = ERR_INVALID_PARAMETER;
    if (!err && existing) {
        err = copy_file(dst, SAVE);
        /* HostFS can fail a write to an existing truncated file. Publish
           into a fresh file only after the backup has closed successfully. */
        if (!err) err = rm(dst);
    }
    if (!err) {
        err = copy_file(src, dst);
        if (err) {
            rm(dst);
            if (existing && copy_file(SAVE, dst)) {
                int cleanup = rm(dst);
                if (cleanup && cleanup != ERR_NO_SUCH_ENTRY)
                    put_s("cc: could not remove incomplete output\n");
                *keep_backup = 1;
                put_s("cc: recover previous output from " SAVE "\n");
            }
        }
    }
    return err;
}
