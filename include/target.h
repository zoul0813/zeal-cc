#ifndef TARGET_H
#define TARGET_H

#include <stddef.h>
#include <stdint.h>

/* Target abstraction layer - provides platform-specific implementations
 * for I/O, argument parsing, and logging */

/* Argument parsing */
typedef struct {
    char* input_file;
    char* output_file;
    int show_help;      /* 1 if help was shown, 0 otherwise */
    int error;          /* 1 if error occurred, 0 otherwise */
} target_args_t;

#ifdef __SDCC
typedef uint16_t target_output_t;
#else
typedef void* target_output_t;
#endif

/* Streaming reader */
typedef struct target_reader target_reader_t;

/* Parse command line arguments in a platform-specific way.
 * Returns a target_args_t structure with parsed arguments.
 * Caller should check show_help and error fields. */
target_args_t target_parse_args(int argc, char** argv);

/* File I/O - streaming reader */
target_reader_t* target_reader_open(const char* filename);
int target_reader_next(target_reader_t* reader); /* returns next byte or -1 on EOF/error */
int target_reader_peek(target_reader_t* reader); /* returns next byte without consuming, or -1 */
void target_reader_close(target_reader_t* reader);

/* Logging */

/* Print a message to stdout/console */
void target_log(const char* message);

/* Print a message to stderr/console */
void target_error(const char* message);

/* Print formatted message if verbose mode is enabled */
void target_log_verbose(const char* message);

/* Streaming output */
target_output_t target_output_open(const char* filename);
void target_output_close(target_output_t handle);
int target_output_write(target_output_t handle, const char* data, uint16_t len);

#endif /* TARGET_H */
