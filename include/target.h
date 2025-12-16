#ifndef TARGET_H
#define TARGET_H

#include <stddef.h>

/* Target abstraction layer - provides platform-specific implementations
 * for I/O, argument parsing, and logging */

/* Argument parsing */
typedef struct {
    char* input_file;
    char* output_file;
    int show_help;      /* 1 if help was shown, 0 otherwise */
    int error;          /* 1 if error occurred, 0 otherwise */
} target_args_t;

/* Parse command line arguments in a platform-specific way.
 * Returns a target_args_t structure with parsed arguments.
 * Caller should check show_help and error fields. */
target_args_t target_parse_args(int argc, char** argv);

/* File I/O */

/* Read entire file into memory. Returns pointer to buffer.
 * For ZOS, this may be a static buffer. For desktop, this is malloc'd.
 * out_size will be set to the size of the file.
 * Returns NULL on error. */
char* target_read_file(const char* filename, size_t* out_size);

/* Clean up a buffer returned by target_read_file.
 * For ZOS, this is a no-op. For desktop, this calls free(). */
void target_cleanup_buffer(char* buffer);

/* Logging */

/* Print a message to stdout/console */
void target_log(const char* message);

/* Print a message to stderr/console */
void target_error(const char* message);

/* Print formatted message if verbose mode is enabled */
void target_log_verbose(const char* message);

#endif /* TARGET_H */
