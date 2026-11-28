#define main parser_entry
#include "../../src/parser/main.c"
#undef main
extern char memory_pool[];
int main(int argc, char** argv) {
    mem_set(memory_pool, 0xa5, CC_POOL_SIZE);
    return parser_entry(argc, argv);
}
