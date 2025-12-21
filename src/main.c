#include "codegen.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "symbol.h"
#include "target.h"

int main(int argc, char** argv) {
    int err = 1;
    lexer_t* lexer;
    token_t* tokens;
    parser_t* parser;
    ast_node_t* ast;
    symbol_table_t* symbols;
    codegen_t* codegen;
    cc_error_t result;
    target_args_t args;
    target_reader_t* reader;

    /* Reset bump allocator so each invocation starts fresh */
    cc_reset_pool();

    /* Initialize defaults */
    g_ctx.verbose = false;
    g_ctx.optimize = false;

    /* Parse command line arguments using target-specific implementation */
    args = target_parse_args(argc, argv);

    if (args.show_help) {
        return 0;
    }

    if (args.error) {
        target_error("Usage: cc <input.c> <output.asm>\n");
        return 1;
    }

    g_ctx.input_file = args.input_file;
    g_ctx.output_file = args.output_file;

    /* Log compilation start */
    target_log_verbose("Compiling ");
    target_log_verbose(g_ctx.input_file);
    target_log_verbose(" -> ");
    target_log_verbose(g_ctx.output_file);
    target_log_verbose("\n");

    /* Open streaming reader */
    reader = target_reader_open(g_ctx.input_file);
    if (!reader) {
        return 1;
    }

    /* Lexical analysis */
    target_log_verbose("Lexing...\n");
    lexer = lexer_create(g_ctx.input_file, reader);
    if (!lexer) {
        goto cleanup_target;
    }

    tokens = lexer_tokenize(lexer);
    if (!tokens) {
        goto cleanup_lexer;
    }

    /* Parsing */
    target_log_verbose("Parsing...\n");
    parser = parser_create(tokens);
    if (!parser) {
        goto cleanup_tokenlist;
    }

    ast = parser_parse(parser);
    if (!ast || parser->error_count > 0) {
        target_error("Parsing failed\n");
        goto cleanup_parser;
    }

    /* Symbol table creation */
    target_log_verbose("Building symbol table...\n");
    symbols = symbol_table_create(NULL);

    /* Code generation */
    target_log_verbose("Generating Z80 assembly...\n");
    codegen = codegen_create(g_ctx.output_file, symbols);
    if (!codegen) {
        goto cleanup_symtab;
    }

    result = codegen_generate(codegen, ast);
    if (result != CC_OK) {
        target_error("Code generation failed\n");
        goto cleanup_codegen;
    }

    /* Write output */
    target_log_verbose("Writing output...\n");
    result = codegen_write_output(codegen);
    if (result != CC_OK) {
        target_error("Failed to write output\n");
        goto cleanup_codegen;
    }

    target_log(g_ctx.input_file);
    target_log(" -> ");
    target_log(g_ctx.output_file);
    target_log("\n");

    err = 0;
    /* Cleanup */
cleanup_codegen:
    codegen_destroy(codegen);
cleanup_symtab:
    symbol_table_destroy(symbols);
cleanup_ast:
    ast_node_destroy(ast);
cleanup_parser:
    parser_destroy(parser);
cleanup_tokenlist:
    token_list_destroy(tokens);
cleanup_lexer:
    lexer_destroy(lexer);
cleanup_target:
    target_reader_close(reader);
    return err;
}
