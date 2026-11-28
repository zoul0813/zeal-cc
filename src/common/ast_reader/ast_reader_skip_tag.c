#include "ast_reader.h"
#include "ast_format.h"
#include "ast_io.h"

/* One bounded traversal owns the wire layout for validation and skipping. */
enum { NODE_ANY, NODE_DECL, NODE_STMT, NODE_EXPR, NODE_PARAM, NODE_BODY, NODE_INIT };
static int8_t walk(uint8_t tag, uint8_t kind);

static bool expression_tag(uint8_t tag) {
    return tag >= AST_TAG_ASSIGN && tag <= AST_TAG_ARRAY_ACCESS;
}

static int8_t node(uint8_t kind) {
    if (reader_tell(reader) >= ast->string_table_offset) return -1;
    return walk(ast_read_u8(), kind);
}

static int8_t nodes(uint16_t count, uint8_t kind) {
    for (uint16_t i = 0; i < count; i++) {
        if (node(kind) < 0) return -1;
    }
    return 0;
}

static int8_t reference(bool name) {
    const char* text = ast_reader_string(ast_read_u16());
    if (!text) return -1;
    if (!name) return 0;
    uint8_t length = 0;
    while (*text) {
        char c = *text++;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
              (length > 0 && c >= '0' && c <= '9'))) return -1;
        if (++length >= MAX_IDENTIFIER_LENGTH) return -1;
    }
    return length ? 0 : -1;
}

static int8_t optional(uint8_t present, uint8_t kind) {
    if (present > 1) return -1;
    return present ? node(kind) : 0;
}

static int8_t payload(uint8_t tag, uint8_t kind) {
    uint8_t base, depth, count, present;
    uint16_t length, total;
    switch (tag) {
        case AST_TAG_PROGRAM:
            total = ast_read_u16();
            return nodes(total, NODE_DECL);
        case AST_TAG_FUNCTION:
            if (reference(true) < 0 || ast_reader_read_type_info(&base, &depth, &length) < 0 || length) return -1;
            count = ast_read_u8();
            if (count > 8 || nodes(count, NODE_PARAM) < 0) return -1;
            return node(NODE_BODY);
        case AST_TAG_VAR_DECL:
            if (reference(true) < 0 || ast_reader_read_type_info(&base, &depth, &length) < 0) return -1;
            if ((base & AST_BASE_MASK) == AST_BASE_VOID && depth == 0) return -1;
            present = ast_read_u8();
            if (kind == NODE_PARAM && (present || length)) return -1;
            return optional(present, NODE_EXPR);
        case AST_TAG_COMPOUND_STMT:
            total = ast_read_u16();
            if (total > 64) return -1;
            return nodes(total, NODE_STMT);
        case AST_TAG_RETURN_STMT:
            return optional(ast_read_u8(), NODE_EXPR);
        case AST_TAG_BREAK_STMT:
        case AST_TAG_CONTINUE_STMT:
            return 0;
        case AST_TAG_GOTO_STMT:
        case AST_TAG_LABEL_STMT:
        case AST_TAG_IDENTIFIER:
            return reference(true);
        case AST_TAG_IF_STMT:
            present = ast_read_u8();
            if (present > 1 || node(NODE_EXPR) < 0 || node(NODE_STMT) < 0) return -1;
            return optional(present, NODE_STMT);
        case AST_TAG_WHILE_STMT:
            if (node(NODE_EXPR) < 0) return -1;
            return node(NODE_STMT);
        case AST_TAG_FOR_STMT: {
            uint8_t init = ast_read_u8(), cond = ast_read_u8(), inc = ast_read_u8();
            if (optional(init, NODE_INIT) < 0 || optional(cond, NODE_EXPR) < 0 || optional(inc, NODE_EXPR) < 0) return -1;
            return node(NODE_STMT);
        }
        case AST_TAG_CALL:
            if (reference(true) < 0) return -1;
            count = ast_read_u8();
            if (count > 8) return -1;
            return nodes(count, NODE_EXPR);
        case AST_TAG_BINARY_OP:
            if (ast_read_u8() > OP_LOR) return -1;
            if (node(NODE_EXPR) < 0) return -1;
            return node(NODE_EXPR);
        case AST_TAG_UNARY_OP:
            if (ast_read_u8() > OP_POSTDEC) return -1;
            return node(NODE_EXPR);
        case AST_TAG_CONSTANT:
            (void)ast_read_i16();
            return 0;
        case AST_TAG_STRING_LITERAL:
            return reference(false);
        case AST_TAG_ASSIGN:
        case AST_TAG_ARRAY_ACCESS:
            if (node(NODE_EXPR) < 0) return -1;
            return node(NODE_EXPR);
        default:
            return -1;
    }
}

static int8_t walk(uint8_t tag, uint8_t kind) {
    if (!ast || tag < AST_TAG_PROGRAM || tag > AST_TAG_ARRAY_ACCESS ||
        ast->walk_depth >= AST_MAX_DEPTH || reader_tell(reader) > ast->string_table_offset) return -1;
    if (tag == AST_TAG_PROGRAM && ast->walk_depth != 0) return -1;
    if (kind == NODE_DECL && tag != AST_TAG_FUNCTION && tag != AST_TAG_VAR_DECL) return -1;
    if (kind == NODE_STMT && (tag < AST_TAG_VAR_DECL || tag == AST_TAG_PROGRAM)) return -1;
    if (kind == NODE_EXPR && !expression_tag(tag)) return -1;
    if (kind == NODE_PARAM && tag != AST_TAG_VAR_DECL) return -1;
    if (kind == NODE_BODY && tag != AST_TAG_COMPOUND_STMT) return -1;
    if (kind == NODE_INIT && tag != AST_TAG_VAR_DECL && !expression_tag(tag)) return -1;
    if (ast->validating) {
        if (!ast->nodes_left) return -1;
        ast->nodes_left--;
    }
    ast->walk_depth++;
    int8_t result = payload(tag, kind);
    ast->walk_depth--;
    if (reader_tell(reader) > ast->string_table_offset) return -1;
    return result;
}

int8_t ast_reader_skip_tag(uint8_t tag) {
    return walk(tag, NODE_ANY);
}

int8_t ast_reader_validate(void) {
    if (reader_seek(reader, AST_HEADER_SIZE) < 0 || ast_read_u8() != AST_TAG_PROGRAM) return -1;
    ast->nodes_left = ast->node_count;
    ast->walk_depth = 0;
    ast->validating = true;
    int8_t result = walk(AST_TAG_PROGRAM, NODE_ANY);
    ast->validating = false;
    if (result < 0 || ast->nodes_left || reader_tell(reader) != ast->string_table_offset) return -1;
    /* The string loader already consumed exactly the string section through EOF. */
    return reader_seek(reader, AST_HEADER_SIZE);
}
