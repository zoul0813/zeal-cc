/* Modern/Desktop target implementation - argument parsing */
#include <stdio.h>

#include "target.h"

args_t parse_args(int argc, char** argv) {
    args_t result = {0};

    if (argc < 3) {
        result.error = 1;
        return result;
    }
    result.input_file = argv[1];
    result.output_file = argv[2];

    return result;
}
