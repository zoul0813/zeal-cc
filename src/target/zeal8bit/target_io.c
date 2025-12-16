/* Zeal 8-bit target implementation - I/O and logging */
#include "target.h"
#include "common.h"
#include <zos_vfs.h>
#include <core.h>

/* Global buffer for file reading - ZOS doesn't have malloc */
/* Use smaller buffer size like other ZOS programs */
static char file_buffer[1024];

char* target_read_file(const char* filename, size_t* out_size) {
    zos_dev_t dev;
    zos_err_t err;
    uint16_t size;
    uint16_t total_read = 0;
    
    /* Debug: show what file we're trying to open */
    put_s("Opening: ");
    put_s(filename);
    put_c('\n');
    
    dev = open(filename, O_RDONLY);
    if (dev < 0) {
        put_s("Error: Could not open file (dev=");
        put_u8((uint8_t)(-dev));
        put_s(")\n");
        return NULL;
    }
    
    put_s("File opened, reading...\n");
    
    /* Read in chunks - ZOS read() supports up to 1024 bytes */
    size = sizeof(file_buffer) - 1;
    err = read(dev, file_buffer, &size);
    
    if (err != ERR_SUCCESS) {
        close(dev);
        put_s("Error: Could not read file (err=");
        put_u8(err);
        put_s(", size=");
        put_u16(size);
        put_s(")\n");
        return NULL;
    }
    
    close(dev);
    
    put_s("Read successful, size=");
    put_u16(size);
    put_c('\n');
    
    file_buffer[size] = '\0';
    *out_size = size;
    
    return file_buffer;
}

void target_cleanup_buffer(char* buffer) {
    /* ZOS uses static buffer, no cleanup needed */
    (void)buffer;
}

void target_log(const char* message) {
    put_s(message);
}

void target_error(const char* message) {
    put_s(message);
}

void target_log_verbose(const char* message) {
    /* ZOS doesn't support verbose mode, always log */
    put_s(message);
}
