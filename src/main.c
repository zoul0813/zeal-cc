#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "symbol.h"
#include "codegen.h"

#ifdef __SDCC
#include <zos_vfs.h>

extern void put_s(const char* str);
extern void put_c(char c);

static void usage(void) {
    put_s("Zeal 8-bit C Compiler v");
    put_s("0.1.0");
    put_s("\n");
    put_s("Usage: cc <input.c> <output.asm>\n");
}

static char* read_file(const char* filename, size_t* out_size) {
    zos_dev_t dev;
    zos_err_t err;
    uint16_t size;
    char* buffer;
    static char file_buffer[4096];
    
    dev = open(filename, O_RDONLY);
    if (dev < 0) {
        put_s("Error: Could not open file ");
        put_s(filename);
        put_c('\n');
        return NULL;
    }
    
    size = sizeof(file_buffer) - 1;
    err = read(dev, file_buffer, &size);
    close(dev);
    
    if (err != ERR_SUCCESS) {
        put_s("Error: Could not read file\n");
        return NULL;
    }
    
    file_buffer[size] = '\0';
    *out_size = size;
    
    return file_buffer;
}

static int write_file(const char* filename, const char* data, size_t size) {
    zos_dev_t dev;
    zos_err_t err;
    uint16_t write_size;
    
    dev = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (dev < 0) {
        put_s("Error: Could not create file ");
        put_s(filename);
        put_c('\n');
        return -dev;
    }
    
    write_size = (uint16_t)size;
    err = write(dev, (void*)data, &write_size);
    close(dev);
    
    if (err != ERR_SUCCESS) {
        put_s("Error: Could not write file\n");
        return err;
    }
    
    return ERR_SUCCESS;
}

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
    
    if (argc < 2) {
        usage();
        return ERR_INVALID_PARAMETER;
    }
    
    g_ctx.input_file = argv[0];
    g_ctx.output_file = (argc > 1) ? argv[1] : "output.asm";
    g_ctx.verbose = false;
    g_ctx.optimize = false;
    
    put_s("Compiling ");
    put_s(g_ctx.input_file);
    put_s(" -> ");
    put_s(g_ctx.output_file);
    put_c('\n');
    
    /* Read source file */
    source = read_file(g_ctx.input_file, &source_len);
    if (!source) {
        return ERR_FAILURE;
    }
    
    /* Lexical analysis */
    put_s("Lexing...\n");
    lexer = lexer_create(g_ctx.input_file, source);
    if (!lexer) {
        return ERR_FAILURE;
    }
    
    tokens = lexer_tokenize(lexer);
    if (!tokens) {
        lexer_destroy(lexer);
        return ERR_FAILURE;
    }
    
    /* Parsing */
    put_s("Parsing...\n");
    parser = parser_create(tokens);
    if (!parser) {
        token_list_destroy(tokens);
        lexer_destroy(lexer);
        return ERR_FAILURE;
    }
    
    ast = parser_parse(parser);
    if (!ast || parser->error_count > 0) {
        parser_destroy(parser);
        lexer_destroy(lexer);
        return ERR_FAILURE;
    }
    
    /* Symbol table creation */
    put_s("Building symbol table...\n");
    symbols = symbol_table_create(NULL);
    
    /* Code generation */
    put_s("Generating Z80 assembly...\n");
    codegen = codegen_create(g_ctx.output_file, symbols);
    if (!codegen) {
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return ERR_FAILURE;
    }
    
    result = codegen_generate(codegen, ast);
    if (result != CC_OK) {
        put_s("Code generation failed\n");
        codegen_destroy(codegen);
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return ERR_FAILURE;
    }
    
    /* Write output */
    put_s("Writing output...\n");
    result = codegen_write_output(codegen);
    if (result != CC_OK) {
        put_s("Failed to write output\n");
        codegen_destroy(codegen);
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return ERR_FAILURE;
    }
    
    put_s("Compilation successful!\n");
    
    /* Cleanup */
    codegen_destroy(codegen);
    symbol_table_destroy(symbols);
    ast_node_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    
    return ERR_SUCCESS;
}

#else /* Desktop version */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    printf("Zeal 8-bit C Compiler v%d.%d.%d\n", 
           CC_VERSION_MAJOR, CC_VERSION_MINOR, CC_VERSION_PATCH);
    printf("Usage: cc [options] <input.c> <output.asm>\n");
    printf("Options:\n");
    printf("  -v, --verbose    Verbose output\n");
    printf("  -O, --optimize   Enable optimizations\n");
    printf("  -h, --help       Show this help\n");
}

static char* read_file(const char* filename, size_t* out_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    fclose(file);
    
    *out_size = read_size;
    return buffer;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    
    /* Parse command line arguments */
    int arg_idx = 1;
    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "-v") == 0 || strcmp(argv[arg_idx], "--verbose") == 0) {
            g_ctx.verbose = true;
        } else if (strcmp(argv[arg_idx], "-O") == 0 || strcmp(argv[arg_idx], "--optimize") == 0) {
            g_ctx.optimize = true;
        } else if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg_idx]);
            usage();
            return 1;
        }
        arg_idx++;
    }
    
    if (arg_idx >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        usage();
        return 1;
    }
    
    g_ctx.input_file = argv[arg_idx];
    g_ctx.output_file = (arg_idx + 1 < argc) ? argv[arg_idx + 1] : "output.asm";
    
    if (g_ctx.verbose) {
        printf("Compiling %s -> %s\n", g_ctx.input_file, g_ctx.output_file);
    }
    
    /* Read source file */
    size_t source_len;
    char* source = read_file(g_ctx.input_file, &source_len);
    if (!source) {
        return 1;
    }
    
    /* Lexical analysis */
    if (g_ctx.verbose) printf("Lexing...\n");
    lexer_t* lexer = lexer_create(g_ctx.input_file, source);
    if (!lexer) {
        free(source);
        return 1;
    }
    
    token_t* tokens = lexer_tokenize(lexer);
    if (!tokens) {
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    
    /* Parsing */
    if (g_ctx.verbose) printf("Parsing...\n");
    parser_t* parser = parser_create(tokens);
    if (!parser) {
        token_list_destroy(tokens);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    
    ast_node_t* ast = parser_parse(parser);
    if (!ast || parser->error_count > 0) {
        printf("Parsing failed with %d errors\n", parser->error_count);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    
    /* Symbol table creation */
    if (g_ctx.verbose) printf("Building symbol table...\n");
    symbol_table_t* symbols = symbol_table_create(NULL);
    
    /* Code generation */
    if (g_ctx.verbose) printf("Generating Z80 assembly...\n");
    codegen_t* codegen = codegen_create(g_ctx.output_file, symbols);
    if (!codegen) {
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    
    cc_error_t result = codegen_generate(codegen, ast);
    if (result != CC_OK) {
        fprintf(stderr, "Code generation failed\n");
        codegen_destroy(codegen);
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    
    /* Write output */
    if (g_ctx.verbose) printf("Writing output...\n");
    result = codegen_write_output(codegen);
    if (result != CC_OK) {
        fprintf(stderr, "Failed to write output\n");
        codegen_destroy(codegen);
        symbol_table_destroy(symbols);
        ast_node_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    
    printf("Compilation successful!\n");
    printf("Generated: %s\n", g_ctx.output_file);
    
    /* Cleanup */
    codegen_destroy(codegen);
    symbol_table_destroy(symbols);
    ast_node_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);
    
    return 0;
}

#endif
