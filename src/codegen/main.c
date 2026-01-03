#include "ast_io.h"
#include "ast_reader.h"
#include "codegen.h"
#include "codegen_strings.h"
#include "common.h"
#include "target.h"
#include "cc_compat.h"

#ifndef CC_POOL_SIZE
#define CC_POOL_SIZE 1024 /* 1 KB pool to avoid file_buffer overlap */
#endif

char g_memory_pool[CC_POOL_SIZE];

reader_t* reader;
ast_reader_t* ast;
ast_reader_t ast_ctx;
codegen_t codegen;
codegen_t* codegen_ctx;

void cleanup(void) {
    codegen_destroy(codegen_ctx);
    ast_reader_destroy();
    reader_close(reader);
}

void handle_error(char* msg) {
    log_error(msg);
    cleanup();
    exit(1);
}

int main(int argc, char** argv) {
    int8_t err = 1;
    args_t args;
    cc_error_t result;

    ast = &ast_ctx;
    mem_set(ast, 0, sizeof(ast_ctx));
    cc_init_pool(g_memory_pool, sizeof(g_memory_pool));


    args = parse_args(argc, argv, ARG_MODE_IN_OUT);
    if (args.error) {
        log_error(CG_MSG_USAGE_CODEGEN);
        return 1;
    }

    reader = reader_open(args.input_file);
    if (!reader) return 1;

    ast_read_handler(handle_error, "Failed to read AST\n");
    if (ast_reader_init() < 0) {
        log_error(CG_MSG_FAILED_READ_AST_HEADER);
        goto cleanup;
    }
    if (ast_reader_load_strings() < 0) {
        log_error(CG_MSG_FAILED_READ_AST_STRING_TABLE);
        goto cleanup;
    }
    codegen_ctx = codegen_create(args.output_file);
    if (!codegen_ctx) {
        log_error(CG_MSG_FAILED_OPEN_OUTPUT);
        goto cleanup;
    }

    result = codegen_generate_stream();
    if (result != CC_OK) {
        log_error(CG_MSG_CODEGEN_FAILED);
        goto cleanup;
    }

    log_msg(args.input_file);
    log_msg(" -> ");
    log_msg(args.output_file);
    log_msg("\n");

    err = 0;

cleanup:
    cleanup();
    return err;
}
