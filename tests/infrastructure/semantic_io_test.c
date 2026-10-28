/* Drive the real stage entry point with a late reader fault. */
#include <assert.h>
#include <string.h>
#define main semantic_entry
#include "../../src/semantic/main.c"
#undef main

static bool failed;
static cc_error_t validation_result;
static char diagnostic[128];
uint16_t cc_init_pool_default(void) { return 0; }
args_t parse_args(int argc, char** argv, uint8_t mode) {
    args_t args = {"input.ast", NULL, 0};
    (void)argc; (void)argv; (void)mode;
    return args;
}
reader_t* reader_open(const char* name) { (void)name; return (reader_t*)&failed; }
bool reader_failed(reader_t* r) { (void)r; return failed; }
void reader_close(reader_t* r) { (void)r; }
int8_t ast_reader_init(void) { return 0; }
int8_t ast_reader_load_strings(void) { return 0; }
void ast_reader_destroy(void) {}
void ast_read_handler(error_handler handler, char* msg) { (void)handler; (void)msg; }
cc_error_t semantic_validate(void) { return validation_result; }
void log_error(const char* text) { strcat(diagnostic, text); }
void log_msg(const char* text) { (void)text; }
int main(void) {
    failed = true;
    validation_result = CC_OK;
    assert(semantic_entry(0, NULL) == 1);
    assert(strcmp(diagnostic, "Failed to read AST\n") == 0);
    diagnostic[0] = '\0';
    validation_result = CC_ERROR_SEMANTIC;
    assert(semantic_entry(0, NULL) == 1);
    assert(strcmp(diagnostic, "Failed to read AST\n") == 0);
    diagnostic[0] = '\0';
    failed = false;
    assert(semantic_entry(0, NULL) == 1);
    assert(strcmp(diagnostic, "Semantic validation failed\n") == 0);
    return 0;
}
