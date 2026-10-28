#ifndef ZOS_VFS_H
#define ZOS_VFS_H
#include "zos_errors.h"
typedef int8_t zos_dev_t;
#define O_RDONLY 0
#define O_RDWR 2
#define O_CREAT 4
#define O_TRUNC 8
zos_dev_t open(const char*, uint8_t);
zos_err_t close(zos_dev_t);
zos_err_t read(zos_dev_t, void*, uint16_t*);
zos_err_t write(zos_dev_t, const void*, uint16_t*);
zos_err_t seek(zos_dev_t, int32_t*, uint8_t);
zos_err_t rm(const char*);
#endif
