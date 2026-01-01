/* Zeal 8-bit target implementation - argument parsing */
#include <core.h>
#include <zos_vfs.h>

#include "common.h"
#include "target.h"

args_t parse_args(int argc, char** argv, uint8_t mode) {
    args_t result = {0};

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
            *p = 0; /* Null-terminate the first argument */
            result.output_file = p + 1;
            break;
        }
        p++;
    }

    if (mode == ARG_MODE_IN_ONLY) {
        if (!result.input_file || *result.input_file == '\0') {
            result.error = 1;
            return result;
        }
        result.output_file = NULL;
    } else {
        /* Validate both arguments are present */
        if (!result.input_file || !result.output_file ||
            *result.input_file == '\0' || *result.output_file == '\0') {
            result.error = 1;
            return result;
        }
    }

    return result;
}
