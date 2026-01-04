#ifndef CC_COMPAT_H
#define CC_COMPAT_H

#ifdef __SDCC
#include <core.h>
typedef void* unused;
#define CC_ASSERT(expr, msg) _Static_assert((expr), msg)
#else
#include <string.h>
typedef void unused;
#define mem_set memset
#define mem_cpy memcpy
#define mem_cmp memcmp
#define str_cmp strcmp
#define str_len strlen
#define str_chr strchr
#define put_c(c) printf("%c", c);
#define put_s(str) printf("%s", str)
#define put_hex(i) printf("%04x", i)
#ifndef CC_ASSERT
#define CC_ASSERT(expr, msg) \
    typedef char cc_static_assert_##__LINE__[(expr) ? 1 : -1]; \
    enum { cc_static_assert_##__LINE__##_value = sizeof(cc_static_assert_##__LINE__) }
#endif
#endif


#endif /* CC_COMPAT_H */
