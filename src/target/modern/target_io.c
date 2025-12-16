/* Modern/Desktop target implementation - I/O and logging */
#include "target.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

char* target_read_file(const char* filename, size_t* out_size) {
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

void target_cleanup_buffer(char* buffer) {
    free(buffer);
}

void target_log(const char* message) {
    printf("%s", message);
}

void target_error(const char* message) {
    fprintf(stderr, "%s", message);
}

void target_log_verbose(const char* message) {
    if (g_ctx.verbose) {
        printf("%s", message);
    }
}
