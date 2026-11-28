#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../../src/common/common.c"

int main(int argc, char** argv) {
    unsigned char storage[8192 + 1];
    memset(storage, 0xa5, sizeof(storage));
    cc_init_pool(storage + 1, sizeof(storage) - 1);
    if (argc > 1 && strcmp(argv[1], "overflow") == 0) {
        cc_malloc((size_t)-1);
        return 9;
    }
    if (argc > 1 && strcmp(argv[1], "oversize") == 0) {
        cc_malloc(sizeof(storage));
        return 9;
    }
    void* pointers[64];
    for (size_t i = 0; i < 64; i++) {
        size_t bytes = i + sizeof(cc_alignment_t);
        pointers[i] = cc_malloc(bytes);
        assert((uintptr_t)pointers[i] % CC_ALIGNMENT == 0);
        memset(pointers[i], 0x3c, bytes);
        *(long double*)pointers[i] = 1.25L;
    }
    for (size_t i = 0; i < 64; i += 2) cc_free(pointers[i]);
    for (size_t i = 1; i < 64; i += 2) cc_free(pointers[i]);
    assert(g_pool_offset == 0);
    assert(g_pool_head->next == NULL);
    assert(storage[0] == 0xa5);
    void* large = cc_malloc(7000);
    memset(large, 0x55, 7000);
    cc_free(large);
    return 0;
}
