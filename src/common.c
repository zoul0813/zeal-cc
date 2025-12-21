#include "common.h"

#ifdef __SDCC
#include <zos_sys.h>
#include <zos_vfs.h>
#include <core.h>
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

typedef struct cc_block_header {
    size_t size;
    bool free;
    struct cc_block_header* next;
} cc_block_header_t;

#ifndef CC_POOL_SIZE
#define CC_POOL_SIZE 12288 /* 12 KB pool; keep DATA below file_buffer */
#endif
char g_memory_pool[CC_POOL_SIZE];
size_t g_pool_offset = 0;
size_t g_pool_max = 0;
static cc_block_header_t* g_pool_head = NULL;

static size_t cc_align_size(size_t size) {
    const size_t align = 4;
    return (size + (align - 1)) & ~(align - 1);
}

void cc_reset_pool(void) {
    g_pool_offset = 0;
    g_pool_max = 0;
    g_pool_head = (cc_block_header_t*)g_memory_pool;
    g_pool_head->size = CC_POOL_SIZE - sizeof(cc_block_header_t);
    g_pool_head->free = true;
    g_pool_head->next = NULL;
}

static void cc_coalesce_free_blocks(void) {
    cc_block_header_t* cur = g_pool_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(cc_block_header_t) + cur->next->size;
            cur->next = cur->next->next;
            continue;
        }
        cur = cur->next;
    }
}

void* cc_malloc(size_t size) {
    if (size == 0) return NULL;

    size = cc_align_size(size);
    if (!g_pool_head) {
        cc_reset_pool();
    }
    cc_block_header_t* cur = g_pool_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            size_t remaining = cur->size - size;
            if (remaining > sizeof(cc_block_header_t) + 4) {
                cc_block_header_t* split = (cc_block_header_t*)((char*)cur + sizeof(cc_block_header_t) + size);
                split->size = remaining - sizeof(cc_block_header_t);
                split->free = true;
                split->next = cur->next;
                cur->next = split;
                cur->size = size;
            }
            cur->free = false;
            g_pool_offset += cur->size;
            if (g_pool_offset > g_pool_max) {
                g_pool_max = g_pool_offset;
            }
            return (char*)cur + sizeof(cc_block_header_t);
        }
        cur = cur->next;
    }

    cc_error("Out of memory");
    exit(1);
}

void cc_free(void* ptr) {
    if (!ptr) return;
    if (!g_pool_head) {
        return;
    }
    cc_block_header_t* header = (cc_block_header_t*)((char*)ptr - sizeof(cc_block_header_t));
    if (header->free) return;
    header->free = true;
    if (g_pool_offset >= header->size) {
        g_pool_offset -= header->size;
    } else {
        g_pool_offset = 0;
    }
    cc_coalesce_free_blocks();
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
