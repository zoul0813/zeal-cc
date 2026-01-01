#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __SDCC
#include <zos_errors.h>
#include <zos_vfs.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ERR_SUCCESS 0
#define ERR_FAILURE 1
#define ERR_INVALID_PARAMETER 2
#define ERR_NO_MORE_ENTRIES 3
#endif

/* Version information */
#define CC_VERSION_MAJOR 0
#define CC_VERSION_MINOR 1
#define CC_VERSION_PATCH 0

/* Buffer sizes (kept small to match target constraints even on host) */
#define MAX_LINE_LENGTH 128
#define MAX_IDENTIFIER_LENGTH 32
#define MAX_STRING_LENGTH 96
#define MAX_TOKEN_LENGTH 48

/* Error codes */
typedef enum {
    CC_OK = 0,
    CC_ERROR_FILE_NOT_FOUND,
    CC_ERROR_MEMORY,
    CC_ERROR_SYNTAX,
    CC_ERROR_SEMANTIC,
    CC_ERROR_CODEGEN,
    CC_ERROR_INTERNAL,
    CC_ERROR_INVALID_ARG
} cc_error_t;

/* Forward declarations */
typedef struct token token_t;
typedef struct ast_node ast_node_t;
typedef struct symbol symbol_t;
typedef struct type type_t;

/* Compiler context */
typedef struct {
    uint16_t error_count;
    uint16_t warning_count;
} compiler_ctx_t;

/* Global context */
extern compiler_ctx_t g_ctx;

/* Utility functions */
void cc_error(const char* msg);
void* cc_malloc(size_t size);
void cc_free(void* ptr);
char* cc_strdup(const char* str);
void cc_reset_pool(void);
void cc_init_pool(void* pool, size_t size);
extern size_t g_pool_offset;
extern size_t g_pool_max;

#endif /* COMMON_H */
