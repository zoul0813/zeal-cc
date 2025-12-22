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
    uint32_t buffer_start;
    uint32_t file_pos;
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
    r->buffer_start = 0;
    r->file_pos = 0;
    return r;
}

static int reader_fill(reader_t* r) {
    r->buffer_start = r->file_pos;
    r->buf_len = FILE_BUFFER_SIZE;
    zos_err_t err = read(r->dev, file_buffer, &r->buf_len);
    r->pos = 0;
    if (err != ERR_SUCCESS || r->buf_len == 0) {
        return -1;
    }
    r->file_pos += (uint32_t)r->buf_len;
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

int reader_seek(reader_t* reader, uint32_t offset) {
    if (!reader) return -1;
    int32_t pos = (int32_t)offset;
    zos_err_t err = seek(reader->dev, &pos, SEEK_SET);
    if (err != ERR_SUCCESS) {
        return -1;
    }
    reader->buf_len = 0;
    reader->pos = 0;
    reader->buffer_start = (uint32_t)pos;
    reader->file_pos = (uint32_t)pos;
    return 0;
}

uint32_t reader_tell(reader_t* reader) {
    if (!reader) return 0;
    return reader->buffer_start + (uint32_t)reader->pos;
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

int output_seek(output_t handle, uint32_t offset) {
    if (handle == 0) return -1;
    zos_dev_t dev = (zos_dev_t)handle;
    int32_t pos = (int32_t)offset;
    zos_err_t err = seek(dev, &pos, SEEK_SET);
    return (err == ERR_SUCCESS) ? 0 : -1;
}

uint32_t output_tell(output_t handle) {
    if (handle == 0) return 0;
    zos_dev_t dev = (zos_dev_t)handle;
    int32_t pos = 0;
    zos_err_t err = seek(dev, &pos, SEEK_CUR);
    return (err == ERR_SUCCESS) ? (uint32_t)pos : 0;
}
