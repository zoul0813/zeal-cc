/* Modern/Desktop target implementation - argument parsing */
#include "target.h"
#include "common.h"
#include <stdio.h>
#include <string.h>

target_args_t target_parse_args(int argc, char** argv) {
    target_args_t result = {0};
    
    if (argc < 2) {
        result.error = 1;
        return result;
    }
    
    /* Parse options */
    int arg_idx = 1;
    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "-v") == 0 || strcmp(argv[arg_idx], "--verbose") == 0) {
            g_ctx.verbose = true;
        } else if (strcmp(argv[arg_idx], "-O") == 0 || strcmp(argv[arg_idx], "--optimize") == 0) {
            g_ctx.optimize = true;
        } else if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            printf("Zeal 8-bit C Compiler v%d.%d.%d\n", 
                   CC_VERSION_MAJOR, CC_VERSION_MINOR, CC_VERSION_PATCH);
            printf("Usage: cc [options] <input.c> <output.asm>\n");
            printf("Options:\n");
            printf("  -v, --verbose    Verbose output\n");
            printf("  -O, --optimize   Enable optimizations\n");
            printf("  -h, --help       Show this help\n");
            result.show_help = 1;
            return result;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg_idx]);
            result.error = 1;
            return result;
        }
        arg_idx++;
    }
    
    if (arg_idx >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        result.error = 1;
        return result;
    }
    
    result.input_file = argv[arg_idx];
    result.output_file = (arg_idx + 1 < argc) ? argv[arg_idx + 1] : "output.asm";
    
    return result;
}
