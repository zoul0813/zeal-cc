/* Modern/Desktop target implementation - streaming I/O and logging */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "target.h"

#define FILE_BUFFER_SIZE 512

struct target_reader {
    int fd;
    char buffer[FILE_BUFFER_SIZE];
    ssize_t buf_len;
    ssize_t pos;
};

target_reader_t* target_reader_open(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    target_reader_t* r = (target_reader_t*)cc_malloc(sizeof(target_reader_t));
    if (!r) {
        close(fd);
        return NULL;
    }
    r->fd = fd;
    r->buf_len = 0;
    r->pos = 0;
    return r;
}

static int target_reader_fill(target_reader_t* r) {
    r->buf_len = read(r->fd, r->buffer, FILE_BUFFER_SIZE);
    r->pos = 0;
    if (r->buf_len <= 0) {
        return -1;
    }
    return 0;
}

int target_reader_next(target_reader_t* reader) {
    if (!reader) return -1;
    if (reader->pos >= reader->buf_len) {
        if (target_reader_fill(reader) < 0) {
            return -1;
        }
    }
    return (unsigned char)reader->buffer[reader->pos++];
}

int target_reader_peek(target_reader_t* reader) {
    if (!reader) return -1;
    if (reader->pos >= reader->buf_len) {
        if (target_reader_fill(reader) < 0) {
            return -1;
        }
    }
    return (unsigned char)reader->buffer[reader->pos];
}

void target_reader_close(target_reader_t* reader) {
    if (!reader) return;
    if (reader->fd >= 0) {
        close(reader->fd);
    }
    cc_free(reader);
}

void target_log(const char* message) {
    printf("%s", message);
}

void target_error(const char* message) {
    fprintf(stderr, "%s", message);
}

void target_log_verbose(const char* message) {
    if (g_ctx.verbose) {
        printf("%s", message);
    }
}

target_output_t target_output_open(const char* filename) {
    return fopen(filename, "wb");
}

void target_output_close(target_output_t handle) {
    if (handle) {
        fclose((FILE*)handle);
    }
}

int target_output_write(target_output_t handle, const char* data, uint16_t len) {
    if (!handle || !data || len == 0) return -1;
    size_t written = fwrite(data, 1, (size_t)len, (FILE*)handle);
    return (written == (size_t)len) ? 0 : -1;
}
