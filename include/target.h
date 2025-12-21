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
} args_t;

#ifdef __SDCC
typedef uint16_t output_t;
#else
typedef void* output_t;
#endif

/* Streaming reader */
typedef struct reader reader_t;

/* Parse command line arguments in a platform-specific way.
 * Returns an args_t structure with parsed arguments.
 * Caller should check show_help and error fields. */
args_t parse_args(int argc, char** argv);

/* File I/O - streaming reader */
reader_t* reader_open(const char* filename);
int reader_next(reader_t* reader); /* returns next byte or -1 on EOF/error */
int reader_peek(reader_t* reader); /* returns next byte without consuming, or -1 */
void reader_close(reader_t* reader);

/* Logging */

/* Print a message to stdout/console */
void log_msg(const char* message);

/* Print a message to stderr/console */
void log_error(const char* message);

/* Print formatted message if verbose mode is enabled */
void log_verbose(const char* message);

/* Streaming output */
output_t output_open(const char* filename);
void output_close(output_t handle);
int output_write(output_t handle, const char* data, uint16_t len);

#endif /* TARGET_H */
