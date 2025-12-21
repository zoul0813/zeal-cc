/* Zeal 8-bit target implementation - I/O and logging */
#include <core.h>
#include <zos_vfs.h>

#include "common.h"
#include "target.h"

/* Global buffer for file reading - ZOS doesn't have malloc */
/* Place buffer higher to free space for DATA; stack still has headroom */
#define FILE_BUFFER_SIZE 512
static __at(0xE000) char file_buffer[FILE_BUFFER_SIZE];

struct reader {
    zos_dev_t dev;
    uint16_t buf_len;
    uint16_t pos;
};

reader_t* reader_open(const char* filename) {
    zos_dev_t dev = open(filename, O_RDONLY);
    if (dev < 0) {
        return NULL;
    }
    reader_t* r = (reader_t*)cc_malloc(sizeof(reader_t));
    if (!r) {
        close(dev);
        return NULL;
    }
    r->dev = dev;
    r->buf_len = 0;
    r->pos = 0;
    return r;
}

static int reader_fill(reader_t* r) {
    r->buf_len = FILE_BUFFER_SIZE;
    zos_err_t err = read(r->dev, file_buffer, &r->buf_len);
    r->pos = 0;
    if (err != ERR_SUCCESS || r->buf_len == 0) {
        return -1;
    }
    return 0;
}

int reader_next(reader_t* reader) {
    if (!reader) return -1;
    if (reader->pos >= reader->buf_len) {
        if (reader_fill(reader) < 0) {
            return -1;
        }
    }
    return (uint8_t)file_buffer[reader->pos++];
}

int reader_peek(reader_t* reader) {
    if (!reader) return -1;
    if (reader->pos >= reader->buf_len) {
        if (reader_fill(reader) < 0) {
            return -1;
        }
    }
    return (uint8_t)file_buffer[reader->pos];
}

void reader_close(reader_t* reader) {
    if (!reader) return;
    if (reader->dev >= 0) {
        close(reader->dev);
    }
    cc_free(reader);
}

void log_msg(const char* message) {
    put_s(message);
}

void log_error(const char* message) {
    put_s(message);
}

void log_verbose(const char* message) {
    /* ZOS doesn't support verbose mode, always log */
    put_s(message);
}

output_t output_open(const char* filename) {
    zos_dev_t dev = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (dev < 0) {
        return NULL;
    }
    return (output_t)dev;
}

void output_close(output_t handle) {
    if (handle == 0) return;
    zos_dev_t dev = (zos_dev_t)handle;
    close(dev);
}

int output_write(output_t handle, const char* data, uint16_t len) {
    if (handle == 0 || !data || len == 0) return -1;
    zos_dev_t dev = (zos_dev_t)handle;
    uint16_t size = len;
    zos_err_t err = write(dev, data, &size);
    return (err == ERR_SUCCESS && size == len) ? 0 : -1;
}
