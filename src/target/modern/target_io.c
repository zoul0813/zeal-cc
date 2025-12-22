/* Modern/Desktop target implementation - streaming I/O and logging */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "target.h"

#define FILE_BUFFER_SIZE 512

struct reader {
    int fd;
    char buffer[FILE_BUFFER_SIZE];
    ssize_t buf_len;
    ssize_t pos;
    uint32_t buffer_start;
    uint32_t file_pos;
};

reader_t* reader_open(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    reader_t* r = (reader_t*)cc_malloc(sizeof(reader_t));
    if (!r) {
        close(fd);
        return NULL;
    }
    r->fd = fd;
    r->buf_len = 0;
    r->pos = 0;
    r->buffer_start = 0;
    r->file_pos = 0;
    return r;
}

static int reader_fill(reader_t* r) {
    r->buffer_start = r->file_pos;
    r->buf_len = read(r->fd, r->buffer, FILE_BUFFER_SIZE);
    r->pos = 0;
    if (r->buf_len <= 0) {
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
    return (unsigned char)reader->buffer[reader->pos++];
}

int reader_peek(reader_t* reader) {
    if (!reader) return -1;
    if (reader->pos >= reader->buf_len) {
        if (reader_fill(reader) < 0) {
            return -1;
        }
    }
    return (unsigned char)reader->buffer[reader->pos];
}

int reader_seek(reader_t* reader, uint32_t offset) {
    if (!reader) return -1;
    off_t res = lseek(reader->fd, (off_t)offset, SEEK_SET);
    if (res < 0) {
        return -1;
    }
    reader->buf_len = 0;
    reader->pos = 0;
    reader->buffer_start = offset;
    reader->file_pos = offset;
    return 0;
}

uint32_t reader_tell(reader_t* reader) {
    if (!reader) return 0;
    return reader->buffer_start + (uint32_t)reader->pos;
}

void reader_close(reader_t* reader) {
    if (!reader) return;
    if (reader->fd >= 0) {
        close(reader->fd);
    }
    cc_free(reader);
}

void log_msg(const char* message) {
    printf("%s", message);
}

void log_error(const char* message) {
    fprintf(stderr, "%s", message);
}

void log_verbose(const char* message) {
    if (g_ctx.verbose) {
        printf("%s", message);
    }
}

output_t output_open(const char* filename) {
    return fopen(filename, "wb");
}

void output_close(output_t handle) {
    if (handle) {
        fclose((FILE*)handle);
    }
}

int output_write(output_t handle, const char* data, uint16_t len) {
    if (!handle || !data || len == 0) return -1;
    size_t written = fwrite(data, 1, (size_t)len, (FILE*)handle);
    return (written == (size_t)len) ? 0 : -1;
}

int output_seek(output_t handle, uint32_t offset) {
    if (!handle) return -1;
    return (fseek((FILE*)handle, (long)offset, SEEK_SET) == 0) ? 0 : -1;
}

uint32_t output_tell(output_t handle) {
    if (!handle) return 0;
    long pos = ftell((FILE*)handle);
    return pos < 0 ? 0 : (uint32_t)pos;
}
