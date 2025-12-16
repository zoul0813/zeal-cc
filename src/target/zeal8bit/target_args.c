/* Zeal 8-bit target implementation - argument parsing */
#include "target.h"
#include "common.h"
#include <zos_vfs.h>
#include <core.h>

target_args_t target_parse_args(int argc, char** argv) {
    target_args_t result = {0};
    
    /* On ZOS, argv[0] contains all arguments as a single string separated by spaces */
    if (argc == 0) {
        result.error = 1;
        return result;
    }
    
    result.input_file = argv[0];
    result.output_file = NULL;
    
    /* Find the space separator between arguments */
    char* p = argv[0];
    while (*p) {
        if (*p == ' ') {
            *p = 0;  /* Null-terminate the first argument */
            result.output_file = p + 1;
            break;
        }
        p++;
    }
    
    /* Validate both arguments are present */
    if (!result.input_file || !result.output_file || 
        *result.input_file == '\0' || *result.output_file == '\0') {
        result.error = 1;
        return result;
    }
    
    return result;
}
