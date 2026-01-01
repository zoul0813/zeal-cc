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
    uint8_t error;          /* 1 if error occurred, 0 otherwise */
} args_t;

#define ARG_MODE_IN_OUT 0
#define ARG_MODE_IN_ONLY 1

#ifdef __SDCC
typedef int16_t output_t;
#else
typedef void* output_t;
#endif

/* Streaming reader */
typedef struct reader reader_t;

/* Parse command line arguments in a platform-specific way.
 * Returns an args_t structure with parsed arguments.
 * Caller should check error field. */
args_t parse_args(int argc, char** argv, uint8_t mode);

/* File I/O - streaming reader */
reader_t* reader_open(const char* filename);
int16_t reader_next(reader_t* reader); /* returns next byte or -1 on EOF/error */
int16_t reader_peek(reader_t* reader); /* returns next byte without consuming, or -1 */
int8_t reader_seek(reader_t* reader, uint32_t offset); /* absolute seek from file start */
uint32_t reader_tell(reader_t* reader); /* current absolute position */
void reader_close(reader_t* reader);

/* Logging */

/* Print a message to stdout/console */
void log_msg(const char* message);

/* Print a message to stderr/console */
void log_error(const char* message);


/* Streaming output */
output_t output_open(const char* filename);
void output_close(output_t handle);
int8_t output_write(output_t handle, const char* data, uint16_t len);
uint32_t output_tell(output_t handle);

#endif /* TARGET_H */
