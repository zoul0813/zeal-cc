#include "common.h"

#ifdef __SDCC
#include <zos_sys.h>
#include <zos_vfs.h>

void put_s(const char* str) {
    uint16_t len = 0;
    const char* p = str;
    while (*p++) len++;
    write(DEV_STDOUT, (void*)str, &len);
}

void put_c(char c) {
    uint16_t len = 1;
    write(DEV_STDOUT, &c, &len);
}
#endif

/* Global compiler context */
compiler_ctx_t g_ctx = {0};

void cc_error(const char* msg) {
    g_ctx.error_count++;
#ifdef __SDCC
    put_s("ERROR: ");
    put_s(msg);
    put_c('\n');
#else
    fprintf(stderr, "ERROR: %s\n", msg);
#endif
}

void cc_warning(const char* msg) {
    g_ctx.warning_count++;
#ifdef __SDCC
    put_s("WARNING: ");
    put_s(msg);
    put_c('\n');
#else
    fprintf(stderr, "WARNING: %s\n", msg);
#endif
}

/* Static pool allocator shared by host and target */
#ifndef CC_POOL_SIZE
#define CC_POOL_SIZE 16384 /* 16 KB pool: fits in 0x4000-0xC000 user space alongside file_buffer/stack */
#endif
char g_memory_pool[CC_POOL_SIZE];
size_t g_pool_offset = 0;
size_t g_pool_max = 0;

void cc_reset_pool(void) {
    g_pool_offset = 0;
    g_pool_max = 0;
}

void* cc_malloc(size_t size) {
    if (g_pool_offset + size > sizeof(g_memory_pool)) {
        cc_error("Out of memory");
#ifndef __SDCC
        fprintf(stderr, "Pool usage %zu / %zu, request %zu bytes\n",
                g_pool_offset, (size_t)sizeof(g_memory_pool), size);
#endif
        exit(1);
    }

    void* ptr = &g_memory_pool[g_pool_offset];
    g_pool_offset += size;
    if (g_pool_offset > g_pool_max) {
        g_pool_max = g_pool_offset;
    }
    return ptr;
}

void cc_free(void* ptr) {
    (void)ptr;
}

char* cc_strdup(const char* str) {
    if (!str) return NULL;

    size_t len = 0;
    const char* p = str;
    while (*p++) len++;

    char* copy = (char*)cc_malloc(len + 1);
    if (!copy) return NULL;

    for (size_t i = 0; i <= len; i++) {
        copy[i] = str[i];
    }

    return copy;
}
