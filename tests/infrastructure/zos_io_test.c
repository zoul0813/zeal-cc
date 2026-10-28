/* Host-compiled syscall fault injection for the real ZOS adapter logic. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define __SDCC 1
#define __at(address)
#include "../../src/target/zeal8bit/target_io.c"

static int read_calls, close_calls, fail_read, fail_seek, fail_close, short_write;
void* cc_malloc(size_t size) { return malloc(size); }
void cc_free(void* ptr) { free(ptr); }
void put_s(const char* text) { (void)text; }
zos_dev_t open(const char* path, uint8_t flags) { (void)path; (void)flags; return 0; }
zos_err_t rm(const char* path) { (void)path; return ERR_SUCCESS; }
zos_err_t close(zos_dev_t dev) { assert(dev == 0); close_calls++; return fail_close; }
zos_err_t seek(zos_dev_t dev, int32_t* pos, uint8_t whence) {
    (void)dev; (void)pos; (void)whence; return fail_seek;
}
zos_err_t read(zos_dev_t dev, void* buf, uint16_t* len) {
    assert(dev == 0);
    read_calls++;
    if (fail_read) return ERR_FAILURE; /* Deliberately leave requested len untouched. */
    if (read_calls == 1) { ((char*)buf)[0] = 'x'; *len = 1; }
    else *len = 0;
    return ERR_SUCCESS;
}
zos_err_t write(zos_dev_t dev, const void* buf, uint16_t* len) {
    assert(dev == 0); (void)buf;
    if (short_write) (*len)--;
    return ERR_SUCCESS;
}
int main(void) {
    reader_t* r = reader_open("input");
    assert(reader_next(r) == 'x');
    fail_read = 1;
    assert(reader_peek(r) == -1 && reader_failed(r));
    assert(reader_next(r) == -1 && reader_peek(r) == -1);
    assert(read_calls == 2); /* No stale bytes or retry after failure. */
    assert(reader_seek(r, 0) < 0);
    reader_close(r);

    fail_read = 0; read_calls = 0;
    r = reader_open("input");
    assert(reader_next(r) == 'x');
    assert(reader_next(r) == -1 && !reader_failed(r));
    assert(reader_next(r) == -1 && read_calls == 2);
    assert(reader_seek(r, 0) == 0); /* Explicit seek resets normal EOF. */
    fail_seek = 1;
    assert(reader_seek(r, 0) < 0 && reader_failed(r));
    assert(reader_next(r) == -1);
    reader_close(r);

    output_t out = output_open("output");
    assert(out == 0 && OUTPUT_VALID(out));
    assert(output_write(out, "x", 1) == 0);
    short_write = 1;
    assert(output_write(out, "x", 1) < 0);
    fail_close = 1;
    assert(output_close(out) < 0);
    assert(close_calls == 3);
    assert(output_close(OUTPUT_INVALID) == 0 && close_calls == 3);
    return 0;
}
