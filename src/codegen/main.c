#include "ast_reader.h"
#include "codegen.h"
#include "common.h"
#include "target.h"
#include "cc_compat.h"

#ifndef CC_POOL_SIZE
#define CC_POOL_SIZE 2048 /* 4 KB pool */
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
        log_error("Usage: cc_codegen <input.ast> <output.asm>\n");
        return 1;
    }

    reader = reader_open(args.input_file);
    if (!reader) return 1;

    if (ast_reader_init(&ast, reader) < 0) {
        log_error("Failed to read AST header\n");
        goto cleanup_reader;
    }
    if (ast_reader_load_strings(&ast) < 0) {
        log_error("Failed to read AST string table\n");
        goto cleanup_reader;
    }
    codegen = codegen_create(args.output_file);
    if (!codegen) {
        log_error("Failed to open output file\n");
        goto cleanup_reader;
    }

    result = codegen_generate_stream(codegen, &ast);
    if (result != CC_OK) {
        log_error("Code generation failed\n");
        goto cleanup_codegen;
    }

    result = codegen_write_output(codegen);
    if (result != CC_OK) {
        log_error("Failed to write output\n");
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
