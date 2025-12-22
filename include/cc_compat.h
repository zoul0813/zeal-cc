#ifndef CC_COMPAT_H
#define CC_COMPAT_H

#ifdef __SDCC
#include <core.h>
#else
#include <string.h>
#define mem_set memset
#define str_cmp strcmp
#define str_len strlen
#define str_chr strchr
#endif

#endif /* CC_COMPAT_H */
