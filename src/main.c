#include "codegen.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "symbol.h"
#include "target.h"

int main(int argc, char** argv) {
    char* source;
    size_t source_len;
    lexer_t* lexer;
    token_t* tokens;
    parser_t* parser;
    ast_node_t* ast;
    symbol_table_t* symbols;
    codegen_t* codegen;
    cc_error_t result;
    target_args_t args;

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

    /* Read source file */
    source = target_read_file(g_ctx.input_file, &source_len);
    if (!source) {
        return 1;
    }

    /* Lexical analysis */
    target_log_verbose("Lexing...\n");
    lexer = lexer_create(g_ctx.input_file, source);
    if (!lexer) {
        target_cleanup_buffer(source);
        return 1;
    }

    tokens = lexer_tokenize(lexer);
    if (!tokens) {
        lexer_destroy(lexer);
        target_cleanup_buffer(source);
        return 1;
    }

    /* Parsing */
    target_log_verbose("Parsing...\n");
    parser = parser_create(tokens);
    if (!parser) {
        token_list_destroy(tokens);
        lexer_destroy(lexer);
        target_cleanup_buffer(source);
        return 1;
    }

    ast = parser_parse(parser);
    if (!ast || parser->error_count > 0) {
        target_error("Parsing failed\n");
        parser_destroy(parser);
        lexer_destroy(lexer);
        target_cleanup_buffer(source);
        return 1;
    }

    /* Symbol table creation */
    target_log_verbose("Building symbol table...\n");
    symbols = symbol_table_create(NULL);

    /* Code generation */
    target_log_verbose("Generating Z80 assembly...\n");
    codegen = codegen_create(g_ctx.output_file, symbols);
    if (!codegen) {
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        target_cleanup_buffer(source);
        return 1;
    }

    result = codegen_generate(codegen, ast);
    if (result != CC_OK) {
        target_error("Code generation failed\n");
        codegen_destroy(codegen);
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        target_cleanup_buffer(source);
        return 1;
    }

    /* Write output */
    target_log_verbose("Writing output...\n");
    result = codegen_write_output(codegen);
    if (result != CC_OK) {
        target_error("Failed to write output\n");
        codegen_destroy(codegen);
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        target_cleanup_buffer(source);
        return 1;
    }

    target_log("Compilation successful!\n");
    target_log_verbose("Generated: ");
    target_log_verbose(g_ctx.output_file);
    target_log_verbose("\n");

    /* Cleanup */
    codegen_destroy(codegen);
    symbol_table_destroy(symbols);
    ast_node_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    target_cleanup_buffer(source);

    return 0;
}
