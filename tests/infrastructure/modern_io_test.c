#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static ssize_t injected_read(int fd, void* buffer, size_t size);
static int injected_fclose(FILE* file);
#define read injected_read
#define fclose injected_fclose
#include "../../src/target/modern/target_io.c"
#undef read
#undef fclose

static int calls, fail_read, fail_close;
void* cc_malloc(size_t size) { return malloc(size); }
void cc_free(void* ptr) { free(ptr); }
static ssize_t injected_read(int fd, void* buffer, size_t size) {
    calls++;
    if (calls == 1) { errno = EINTR; return -1; }
    if (fail_read) { errno = EIO; return -1; }
    return read(fd, buffer, size);
}
static int injected_fclose(FILE* file) {
    int result = fclose(file);
    return fail_close ? EOF : result;
}
int main(int argc, char** argv) {
    assert(argc == 3);
    reader_t* r = reader_open(argv[1]);
    assert(reader_next(r) == 'x'); /* EINTR must retry. */
    fail_read = 1;
    assert(reader_next(r) == -1 && reader_failed(r));
    assert(reader_next(r) == -1 && reader_peek(r) == -1 && calls == 3);
    assert(reader_seek(r, 0) < 0);
    reader_close(r);

    calls = 1; fail_read = 0;
    r = reader_open(argv[1]);
    assert(reader_next(r) == 'x');
    assert(reader_next(r) == -1 && !reader_failed(r));
    assert(reader_next(r) == -1 && calls == 3);
    assert(reader_seek(r, 0) == 0 && reader_next(r) == 'x');
    reader_close(r);

    output_t out = output_open(argv[2]);
    assert(OUTPUT_VALID(out));
    assert(output_write(out, "x", 1) == 0);
    fail_close = 1;
    assert(output_close(out) < 0);
    assert(output_close(OUTPUT_INVALID) == 0);
    return 0;
}
