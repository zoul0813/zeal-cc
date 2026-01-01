#include "ast_io.h"
#include "ast_reader.h"
#include "cc_compat.h"
#include "common.h"
#include "semantic.h"
#include "target.h"

#ifndef CC_POOL_SIZE
#define CC_POOL_SIZE 1024 /* 1 KB pool to avoid file_buffer overlap */
#endif

static const char SEM_MSG_USAGE[] = "Usage: cc_semantic <input.ast>\n";
static const char SEM_MSG_FAILED_READ_AST_HEADER[] = "Failed to read AST header\n";
static const char SEM_MSG_FAILED_READ_AST_STRING_TABLE[] = "Failed to read AST string table\n";
static const char SEM_MSG_FAILED_SEMANTIC[] = "Semantic validation failed\n";
static const char SEM_MSG_FAILED_OPEN_INPUT[] = "Failed to open input file\n";

char g_memory_pool[CC_POOL_SIZE];

static reader_t* reader;
static ast_reader_t ast;

static void cleanup(void) {
    ast_reader_destroy(&ast);
    reader_close(reader);
}

static void handle_error(char* msg) {
    log_error(msg);
    cleanup();
    exit(1);
}

int main(int argc, char** argv) {
    int8_t err = 1;
    args_t args;
    cc_error_t result;

    mem_set(&ast, 0, sizeof(ast));
    cc_init_pool(g_memory_pool, sizeof(g_memory_pool));

    args = parse_args(argc, argv, ARG_MODE_IN_ONLY);
    if (args.error) {
        log_error(SEM_MSG_USAGE);
        return 1;
    }

    reader = reader_open(args.input_file);
    if (!reader) {
        log_error(SEM_MSG_FAILED_OPEN_INPUT);
        return 1;
    }

    ast_read_handler(handle_error, "Failed to read AST\n");
    if (ast_reader_init(&ast, reader) < 0) {
        log_error(SEM_MSG_FAILED_READ_AST_HEADER);
        goto cleanup;
    }
    if (ast_reader_load_strings(&ast) < 0) {
        log_error(SEM_MSG_FAILED_READ_AST_STRING_TABLE);
        goto cleanup;
    }

    result = semantic_validate(&ast);
    if (result != CC_OK) {
        log_error(SEM_MSG_FAILED_SEMANTIC);
        goto cleanup;
    }

    cleanup();

    log_msg(args.input_file);
    log_msg(" OK\n");

    err = 0;
    return err;

cleanup:
    cleanup();
    return err;
}
