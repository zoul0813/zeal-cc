/* Run the driver's real copy and rollback logic against files in a private cwd. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/cc/publish.c"

static FILE *streams[2];
static int destination_writes, fail_restore;

void put_s(const char *text) { fputs(text, stdout); }

zos_err_t stat(const char *path, zos_stat_t *result) {
    FILE *file = fopen(path, "rb");
    if (!file) return ERR_NO_SUCH_ENTRY;
    fclose(file);
    result->s_flags = 1;
    return 0;
}

zos_dev_t open(const char *path, uint8_t flags) {
    int output = flags != O_RDONLY;
    if (output && strcmp(path, "final") == 0) destination_writes++;
    streams[output] = fopen(path, output ? "wb" : "rb");
    return streams[output] ? output : -ERR_FAILURE;
}

zos_err_t close(zos_dev_t fd) { return fclose(streams[fd]) ? ERR_FAILURE : 0; }
zos_err_t rm(const char *path) { return remove(path) ? ERR_FAILURE : 0; }

zos_err_t read(zos_dev_t fd, void *buf, uint16_t *len) {
    *len = fread(buf, 1, *len, streams[fd]);
    return ferror(streams[fd]) ? ERR_FAILURE : 0;
}

zos_err_t write(zos_dev_t fd, const void *buf, uint16_t *len) {
    /* Backup completes. The new file and optionally its replacement fail
       after creating a visible prefix. */
    if (destination_writes == 1 || (destination_writes == 2 && fail_restore)) {
        assert(fwrite(buf, 1, 1, streams[fd]) == 1);
        return ERR_FAILURE;
    }
    *len = fwrite(buf, 1, *len, streams[fd]);
    return ferror(streams[fd]) ? ERR_FAILURE : 0;
}

static void seed(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb");
    assert(file && fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}

static void check_original(const char *path) {
    char contents[32];
    FILE *file = fopen(path, "rb");
    assert(file && fread(contents, 1, sizeof(contents), file) == strlen("original"));
    assert(memcmp(contents, "original", strlen("original")) == 0);
    assert(fclose(file) == 0);
}

int main(void) {
    seed("new", "replacement");
    for (fail_restore = 0; fail_restore <= 1; fail_restore++) {
        int keep_backup = 0;
        seed("final", "original");
        destination_writes = 0;
        assert(publish("new", "final", &keep_backup) == ERR_FAILURE);
        assert(destination_writes == 2 && keep_backup == fail_restore);
        check_original(SAVE);
        if (fail_restore) {
            zos_stat_t result;
            assert(stat("final", &result) == ERR_NO_SUCH_ENTRY);
        } else {
            check_original("final");
        }
    }
    return 0;
}
