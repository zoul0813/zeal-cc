/* ZDE-only wrapper: same sources and allocator, plus stack canaries and peak reporting. */
#define main stage_main
#if defined(PROBE_PARSE)
#include "../../src/parser/main.c"
#elif defined(PROBE_SEMANTIC)
#include "../../src/semantic/main.c"
#elif defined(PROBE_CODEGEN)
#include "../../src/codegen/main.c"
#elif defined(PROBE_DUMP)
#include "../../src/tools/ast_dump.c"
#endif
#undef main
#include "../../src/common/common.c"

static uint16_t probe_sp;
int main(int argc, char** argv) {
    __asm__("ld hl, #0\n add hl, sp\n ld (_probe_sp), hl\n");
    if (probe_sp < 0xf100) return 1;
    /* ZOS maps a private user stack page at C000 before entering a program. */
    volatile uint8_t* mark = (volatile uint8_t*)0xc500;
    while (mark < (volatile uint8_t*)0xf000) *mark++ = 0xa5;
    int result = stage_main(argc, argv);
    mark = (volatile uint8_t*)0xc500;
    while (mark < (volatile uint8_t*)0xf000 && *mark == 0xa5) mark++;
    put_s("MEMORY pool="); put_hex(g_pool_size);
    put_s(" allocated_peak="); put_hex(g_pool_max);
    put_s(" stack_guard_to="); put_hex((uint16_t)mark); put_s("\n");
    /* Even deepest stress cases must leave at least 8 KB between buffer and stack. */
    if (mark < (volatile uint8_t*)0xe500 || g_pool_max > g_pool_size) return 1;
    return result;
}
