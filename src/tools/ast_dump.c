#include "cc_compat.h"
#include "ast_format.h"
#include "ast_reader.h"
#include "ast_io.h"
#include "common.h"
#include "symbol.h"
#include "target.h"

#ifndef CC_POOL_SIZE
#define CC_POOL_SIZE 2048 /* 2 KB pool */
#endif

char g_memory_pool[CC_POOL_SIZE];
reader_t* reader;
ast_reader_t* ast;

static const char* bin_op_name(binary_op_t op) {
    switch (op) {
        case OP_ADD: return "OP_ADD";
        case OP_SUB: return "OP_SUB";
        case OP_MUL: return "OP_MUL";
        case OP_DIV: return "OP_DIV";
        case OP_MOD: return "OP_MOD";
        case OP_AND: return "OP_AND";
        case OP_OR: return "OP_OR";
        case OP_XOR: return "OP_XOR";
        case OP_SHL: return "OP_SHL";
        case OP_SHR: return "OP_SHR";
        case OP_EQ: return "OP_EQ";
        case OP_NE: return "OP_NE";
        case OP_LT: return "OP_LT";
        case OP_LE: return "OP_LE";
        case OP_GT: return "OP_GT";
        case OP_GE: return "OP_GE";
        case OP_LAND: return "OP_LAND";
        case OP_LOR: return "OP_LOR";
        default: return "OP_UNKNOWN";
    }
}

static const char* unary_op_name(unary_op_t op) {
    switch (op) {
        case OP_NEG: return "OP_NEG";
        case OP_NOT: return "OP_NOT";
        case OP_LNOT: return "OP_LNOT";
        case OP_ADDR: return "OP_ADDR";
        case OP_DEREF: return "OP_DEREF";
        case OP_PREINC: return "OP_PREINC";
        case OP_PREDEC: return "OP_PREDEC";
        case OP_POSTINC: return "OP_POSTINC";
        case OP_POSTDEC: return "OP_POSTDEC";
        default: return "OP_UNKNOWN";
    }
}

static void format_type_info(uint8_t base, uint8_t depth, uint16_t array_len,
                             char* out, size_t out_size) {
    size_t len = 0;
    bool is_unsigned = (base & AST_BASE_FLAG_UNSIGNED) != 0;
    uint8_t base_kind = (uint8_t)(base & AST_BASE_MASK);
    const char* base_name = "unknown";
    switch (base_kind) {
        case AST_BASE_INT: base_name = "int"; break;
        case AST_BASE_CHAR: base_name = "char"; break;
        case AST_BASE_VOID: base_name = "void"; break;
        default: base_name = "unknown"; break;
    }
    if (out_size == 0) return;
    if (is_unsigned && base_kind != AST_BASE_VOID) {
        const char* prefix = "unsigned ";
        size_t i = 0;
        while (prefix[i] && len + 1 < out_size) {
            out[len++] = prefix[i++];
        }
    }
    {
        size_t i = 0;
        while (base_name[i] && len + 1 < out_size) {
            out[len++] = base_name[i++];
        }
    }
    while (depth > 0 && len + 1 < out_size) {
        out[len++] = '*';
        depth--;
    }
    if (array_len > 0) {
        char buf[8];
        size_t i = 0;
        size_t j = 0;
        uint16_t n = array_len;
        if (len + 1 < out_size) {
            out[len++] = '[';
        }
        if (n == 0) {
            buf[i++] = '0';
        } else {
            while (n > 0 && i < sizeof(buf)) {
                buf[i++] = (char)('0' + (n % 10));
                n /= 10;
            }
        }
        while (j < i && len + 1 < out_size) {
            out[len++] = buf[i - 1 - j];
            j++;
        }
        if (len + 1 < out_size) {
            out[len++] = ']';
        }
    }
    out[len] = '\0';
}

static void print_indent(uint16_t depth) {
    for (uint16_t i = 0; i < depth; i++) {
        log_msg("  ");
    }
}

static int8_t dump_node_stream(ast_reader_t* ast, uint16_t depth);

static int8_t dump_constant(int16_t value) {
    char buf[16];
    uint16_t i = 0;
    if (value == 0) {
        buf[i++] = '0';
    } else {
        uint8_t neg = 0;
        if (value < 0) {
            neg = 1;
            value = -value;
        }
        char tmp[16];
        uint16_t j = 0;
        while (value > 0 && j < (uint16_t)sizeof(tmp)) {
            tmp[j++] = '0' + (value % 10);
            value /= 10;
        }
        if (neg) buf[i++] = '-';
        while (j > 0) {
            buf[i++] = tmp[--j];
        }
    }
    buf[i] = '\0';
    log_msg("AST_CONSTANT (value=");
    log_msg(buf);
    log_msg(")\n");
    return 0;
}

static int8_t dump_node_stream(ast_reader_t* ast, uint16_t depth) {
    uint8_t tag = 0;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    int16_t i16 = 0;
    uint8_t base = 0;
    uint8_t ptr_depth = 0;
    char type_buf[32];

    ast_read_u8(ast->reader, &tag);
    print_indent(depth);

    switch (tag) {
        case AST_TAG_PROGRAM:
            log_msg("AST_PROGRAM\n");
            ast_read_u16(ast->reader, &u16);
            for (uint16_t i = 0; i < u16; i++) {
                if (dump_node_stream(ast, depth + 1) < 0) return -1;
            }
            return 0;
        case AST_TAG_FUNCTION: {
            uint16_t array_len = 0;
            ast_read_u16(ast->reader, &u16);
            if (ast_reader_read_type_info(ast, &base, &ptr_depth, &array_len) < 0) return -1;
            ast_read_u8(ast->reader, &u8);
            format_type_info(base, ptr_depth, array_len, type_buf, sizeof(type_buf));
            log_msg("AST_FUNCTION (name=");
            log_msg(ast_reader_string(ast, u16) ? ast_reader_string(ast, u16) : "null");
            log_msg(", return_type=");
            log_msg(type_buf);
            log_msg(")\n");
            for (uint8_t i = 0; i < u8; i++) {
                if (dump_node_stream(ast, depth + 1) < 0) return -1;
            }
            return dump_node_stream(ast, depth + 1);
        }
        case AST_TAG_VAR_DECL: {
            uint8_t has_init = 0;
            uint16_t array_len = 0;
            ast_read_u16(ast->reader, &u16);
            if (ast_reader_read_type_info(ast, &base, &ptr_depth, &array_len) < 0) return -1;
            ast_read_u8(ast->reader, &has_init);
            format_type_info(base, ptr_depth, array_len, type_buf, sizeof(type_buf));
            log_msg("AST_VAR_DECL (name=");
            log_msg(ast_reader_string(ast, u16) ? ast_reader_string(ast, u16) : "null");
            log_msg(", var_type=");
            log_msg(type_buf);
            log_msg(")\n");
            if (has_init) return dump_node_stream(ast, depth + 1);
            return 0;
        }
        case AST_TAG_COMPOUND_STMT:
            log_msg("AST_COMPOUND_STMT\n");
            ast_read_u16(ast->reader, &u16);
            for (uint16_t i = 0; i < u16; i++) {
                if (dump_node_stream(ast, depth + 1) < 0) return -1;
            }
            return 0;
        case AST_TAG_RETURN_STMT:
            log_msg("AST_RETURN_STMT\n");
            ast_read_u8(ast->reader, &u8);
            if (u8) return dump_node_stream(ast, depth + 1);
            return 0;
        case AST_TAG_IF_STMT:
            log_msg("AST_IF_STMT\n");
            ast_read_u8(ast->reader, &u8);
            if (dump_node_stream(ast, depth + 1) < 0) return -1;
            if (dump_node_stream(ast, depth + 1) < 0) return -1;
            if (u8) return dump_node_stream(ast, depth + 1);
            return 0;
        case AST_TAG_WHILE_STMT:
            log_msg("AST_WHILE_STMT\n");
            if (dump_node_stream(ast, depth + 1) < 0) return -1;
            return dump_node_stream(ast, depth + 1);
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = 0;
            uint8_t has_cond = 0;
            uint8_t has_inc = 0;
            log_msg("AST_FOR_STMT\n");
            ast_read_u8(ast->reader, &has_init);
            ast_read_u8(ast->reader, &has_cond);
            ast_read_u8(ast->reader, &has_inc);
            if (has_init && dump_node_stream(ast, depth + 1) < 0) return -1;
            if (has_cond && dump_node_stream(ast, depth + 1) < 0) return -1;
            if (has_inc && dump_node_stream(ast, depth + 1) < 0) return -1;
            return dump_node_stream(ast, depth + 1);
        }
        case AST_TAG_ASSIGN:
            log_msg("AST_ASSIGN\n");
            if (dump_node_stream(ast, depth + 1) < 0) return -1;
            return dump_node_stream(ast, depth + 1);
        case AST_TAG_CALL:
            ast_read_u16(ast->reader, &u16);
            ast_read_u8(ast->reader, &u8);
            log_msg("AST_CALL (name=");
            log_msg(ast_reader_string(ast, u16) ? ast_reader_string(ast, u16) : "null");
            log_msg(")\n");
            for (uint8_t i = 0; i < u8; i++) {
                if (dump_node_stream(ast, depth + 1) < 0) return -1;
            }
            return 0;
        case AST_TAG_BINARY_OP:
            ast_read_u8(ast->reader, &u8);
            log_msg("AST_BINARY_OP (op=");
            log_msg(bin_op_name((binary_op_t)u8));
            log_msg(")\n");
            if (dump_node_stream(ast, depth + 1) < 0) return -1;
            return dump_node_stream(ast, depth + 1);
        case AST_TAG_UNARY_OP:
            ast_read_u8(ast->reader, &u8);
            log_msg("AST_UNARY_OP (op=");
            log_msg(unary_op_name((unary_op_t)u8));
            log_msg(")\n");
            return dump_node_stream(ast, depth + 1);
        case AST_TAG_IDENTIFIER:
            ast_read_u16(ast->reader, &u16);
            log_msg("AST_IDENTIFIER (name=");
            log_msg(ast_reader_string(ast, u16) ? ast_reader_string(ast, u16) : "null");
            log_msg(")\n");
            return 0;
        case AST_TAG_CONSTANT:
            ast_read_i16(ast->reader, &i16);
            return dump_constant(i16);
        case AST_TAG_STRING_LITERAL:
            ast_read_u16(ast->reader, &u16);
            log_msg("AST_STRING_LITERAL (value=");
            log_msg(ast_reader_string(ast, u16) ? ast_reader_string(ast, u16) : "null");
            log_msg(")\n");
            return 0;
        case AST_TAG_ARRAY_ACCESS:
            log_msg("AST_ARRAY_ACCESS\n");
            if (dump_node_stream(ast, depth + 1) < 0) return -1;
            return dump_node_stream(ast, depth + 1);
        default:
            log_msg("AST_UNKNOWN\n");
            return -1;
    }
}

static const char* parse_input_arg(int16_t argc, char** argv) {
#ifdef __SDCC
    if (argc == 0 || !argv || !argv[0]) return NULL;
    char* p = argv[0];
    while (*p == ' ') p++;
    if (*p == '\0') return NULL;
    char* end = p;
    while (*end && *end != ' ') end++;
    if (*end) *end = '\0';
    return p;
#else
    if (argc < 2) return NULL;
    return argv[1];
#endif
}

void cleanup(void) {
    ast_reader_destroy(ast);
    reader_close(reader);
}

void handle_error(char* msg) {
    log_error(msg);
    cleanup();
    exit(1);
}

int main(int argc, char** argv) {
    int8_t err = 1;
    const char* input = parse_input_arg((int16_t)argc, argv);
    if (!input) {
        log_error("Usage: ast_dump <input.ast>\n");
        return 1;
    }

    cc_init_pool(g_memory_pool, sizeof(g_memory_pool));

    reader = reader_open(input);
    if (!reader) return 1;

    ast = (ast_reader_t*)cc_malloc(sizeof(ast_reader_t));
    mem_set(ast, 0, sizeof(ast_reader_t));
    uint16_t decl_count = 0;

    ast_read_handler(handle_error, "Failed to read AST\n");

    if (ast_reader_init(ast, reader) < 0) {
        log_error("Failed to read AST header\n");
        goto cleanup;
    }
    if (ast_reader_load_strings(ast) < 0) {
        log_error("Failed to read AST string table\n");
        goto cleanup;
    }

    if (ast_reader_begin_program(ast, &decl_count) < 0) {
        log_error("Failed to read AST program header\n");
        goto cleanup;
    }
    log_msg("AST_PROGRAM\n");
    for (uint16_t i = 0; i < decl_count; i++) {
        if (dump_node_stream(ast, 1) < 0) {
            log_error("Failed to parse AST node stream\n");
            goto cleanup;
        }
    }
    err = 0;

cleanup:
    cleanup();
    return err;
}
