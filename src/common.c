#include "common.h"

#ifdef __SDCC
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

void* cc_malloc(size_t size) {
#ifdef __SDCC
    /* On ZOS, we'll use a simple memory pool for now */
    static char memory_pool[8192];
    static size_t pool_offset = 0;
    
    if (pool_offset + size > sizeof(memory_pool)) {
        cc_error("Out of memory");
        return NULL;
    }
    
    void* ptr = &memory_pool[pool_offset];
    pool_offset += size;
    return ptr;
#else
    void* ptr = malloc(size);
    if (!ptr) {
        cc_error("Out of memory");
        exit(1);
    }
    return ptr;
#endif
}

void cc_free(void* ptr) {
#ifndef __SDCC
    free(ptr);
#endif
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
