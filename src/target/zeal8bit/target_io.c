/* Zeal 8-bit target implementation - I/O and logging */
#include <core.h>
#include <zos_vfs.h>

#include "common.h"
#include "target.h"


/* Global buffer for file reading - ZOS doesn't have malloc */
/* Place buffer higher to free space for DATA; stack still has headroom */
#define FILE_BUFFER_SIZE 512
static __at(0xC300) char file_buffer[FILE_BUFFER_SIZE];

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

static int8_t reader_fill(reader_t* r) {
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

int16_t reader_peek(reader_t* reader) {
    if (!reader) return -1;
    if (reader->pos >= reader->buf_len) {
        if (reader_fill(reader) < 0) {
            return -1;
        }
    }
    return (uint8_t)file_buffer[reader->pos];
}

int16_t reader_next(reader_t* reader) {
    int16_t ret = reader_peek(reader);
    if (ret != -1) reader->pos++;
    return ret;
}

int8_t reader_seek(reader_t* reader, uint32_t offset) {
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

output_t output_open(const char* filename) {
    rm(filename); // sometimes O_TRUNC doesn't work as expected?! :shrug:
    zos_dev_t dev = open(filename, O_RDWR | O_CREAT | O_TRUNC);
    if (dev < 0) {
        return (output_t)-1;
    }
    return (output_t)dev;
}

void output_close(output_t handle) {
    if (handle < 0) return;
    zos_dev_t dev = (zos_dev_t)handle;
    close(dev);
}

int8_t output_write(output_t handle, const char* data, uint16_t len) {
    if (handle < 0 || !data || len == 0) return -1;
    zos_dev_t dev = (zos_dev_t)handle;
    uint16_t write_size = len;
    zos_err_t err = write(dev, data, &write_size);
    if (err != ERR_SUCCESS || write_size != len) {
        return -1;
    }
    return 0;
}

uint32_t output_tell(output_t handle) {
    if (handle < 0) return 0;
    zos_dev_t dev = (zos_dev_t)handle;
    int32_t pos = 0;
    zos_err_t err = seek(dev, &pos, SEEK_CUR);
    return (err == ERR_SUCCESS) ? (uint32_t)pos : 0;
}
