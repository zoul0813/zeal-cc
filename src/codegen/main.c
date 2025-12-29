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

int main(int argc, char** argv) {
    int8_t err = 1;
    args_t args;
    reader_t* reader = NULL;
    ast_reader_t ast;
    codegen_t* codegen = NULL;
    cc_error_t result;

    mem_set(&ast, 0, sizeof(ast));
    cc_init_pool(g_memory_pool, sizeof(g_memory_pool));


    args = parse_args(argc, argv);
    if (args.error) {
        log_error(CG_MSG_USAGE_CODEGEN);
        return 1;
    }

    reader = reader_open(args.input_file);
    if (!reader) return 1;

    if (ast_reader_init(&ast, reader) < 0) {
        log_error(CG_MSG_FAILED_READ_AST_HEADER);
        goto cleanup_reader;
    }
    if (ast_reader_load_strings(&ast) < 0) {
        log_error(CG_MSG_FAILED_READ_AST_STRING_TABLE);
        goto cleanup_reader;
    }
    codegen = codegen_create(args.output_file);
    if (!codegen) {
        log_error(CG_MSG_FAILED_OPEN_OUTPUT);
        goto cleanup_reader;
    }

    result = codegen_generate_stream(codegen, &ast);
    if (result != CC_OK) {
        log_error(CG_MSG_CODEGEN_FAILED);
        goto cleanup_codegen;
    }

    log_msg(args.input_file);
    log_msg(" -> ");
    log_msg(args.output_file);
    log_msg("\n");

    err = 0;

cleanup_codegen:
    codegen_destroy(codegen);
cleanup_reader:
    ast_reader_destroy(&ast);
    reader_close(reader);
    return err;
}
