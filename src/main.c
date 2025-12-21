#include "codegen.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "symbol.h"
#include "target.h"

// #define VERBOSE 1

int main(int argc, char** argv) {
    int err = 1;
    lexer_t* lexer = NULL;
    token_t* tokens = NULL;
    parser_t* parser = NULL;
    ast_node_t* ast = NULL;
    symbol_table_t* symbols = NULL;
    codegen_t* codegen = NULL;
    cc_error_t result;
    args_t args;
    reader_t* reader = NULL;

    /* Reset bump allocator so each invocation starts fresh */
    cc_reset_pool();

    /* Initialize defaults */
    g_ctx.verbose = false;
    g_ctx.optimize = false;

    /* Parse command line arguments using target-specific implementation */
    args = parse_args(argc, argv);

    if (args.show_help) {
        return 0;
    }

    if (args.error) {
        log_error("Usage: cc <input.c> <output.asm>\n");
        return 1;
    }

    g_ctx.input_file = args.input_file;
    g_ctx.output_file = args.output_file;

    /* Log compilation start */
#ifdef VERBOSE
    log_verbose("Compiling ");
    log_verbose(g_ctx.input_file);
    log_verbose(" -> ");
    log_verbose(g_ctx.output_file);
    log_verbose("\n");
#endif

    /* Open streaming reader */
    reader = reader_open(g_ctx.input_file);
    if (!reader) {
        return 1;
    }

    /* Lexical analysis */
#ifdef VERBOSE
    log_verbose("Lexing...\n");
#endif
    lexer = lexer_create(g_ctx.input_file, reader);
    if (!lexer) {
        goto cleanup_reader;
    }

    tokens = lexer_tokenize(lexer);
    if (!tokens) {
        goto cleanup_lexer;
    }

    /* Parsing */
#ifdef VERBOSE
    log_verbose("Parsing...\n");
#endif
    parser = parser_create(tokens);
    if (!parser) {
        goto cleanup_tokenlist;
    }

    ast = parser_parse(parser);
    if (!ast || parser->error_count > 0) {
        log_error("Parsing failed\n");
        goto cleanup_parser;
    }

    /* Symbol table creation */
#ifdef VERBOSE
    log_verbose("Building symbol table...\n");
#endif
    symbols = symbol_table_create(NULL);
    if (!symbols) {
        goto cleanup_ast;
    }

    /* Code generation */
#ifdef VERBOSE
    log_verbose("Generating Z80 assembly...\n");
#endif
    codegen = codegen_create(g_ctx.output_file, symbols);
    if (!codegen) {
        goto cleanup_symtab;
    }

    result = codegen_generate(codegen, ast);
    if (result != CC_OK) {
        log_error("Code generation failed\n");
        goto cleanup_codegen;
    }

    /* Write output */
#ifdef VERBOSE
    log_verbose("Writing output...\n");
#endif
    result = codegen_write_output(codegen);
    if (result != CC_OK) {
        log_error("Failed to write output\n");
        goto cleanup_codegen;
    }

    log_msg(g_ctx.input_file);
    log_msg(" -> ");
    log_msg(g_ctx.output_file);
    log_msg("\n");

    err = 0;

    /* Cleanup */
cleanup_codegen:
    codegen_destroy(codegen);
cleanup_symtab:
    symbol_table_destroy(symbols);
cleanup_ast:
    ast_node_destroy(ast); /* ast can be NULL; label kept for symmetry */
cleanup_parser:
    parser_destroy(parser);
cleanup_tokenlist:
    token_list_destroy(tokens);
cleanup_lexer:
    lexer_destroy(lexer);
cleanup_reader:
    reader_close(reader);
    return err;
}
