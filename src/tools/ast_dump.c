#include "ast_reader.h"
#include "common.h"
#include "symbol.h"
#include "target.h"

#include <string.h>

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

static void format_type(const type_t* type, char* out, size_t out_size) {
    const type_t* cur = type;
    size_t len = 0;
    const char* base = "unknown";
    uint8_t depth = 0;

    while (cur && cur->kind == TYPE_POINTER) {
        depth++;
        cur = cur->data.pointer.base_type;
    }
    if (cur) {
        switch (cur->kind) {
            case TYPE_INT: base = "int"; break;
            case TYPE_CHAR: base = "char"; break;
            case TYPE_VOID: base = "void"; break;
            default: base = "unknown"; break;
        }
    }
    if (out_size == 0) return;
    while (base[len] && len + 1 < out_size) {
        out[len] = base[len];
        len++;
    }
    while (depth > 0 && len + 1 < out_size) {
        out[len++] = '*';
        depth--;
    }
    out[len] = '\0';
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        log_msg("  ");
    }
}

static void dump_node(const ast_node_t* node, int depth) {
    if (!node) return;
    print_indent(depth);

    char type_buf[32];
    switch (node->type) {
        case AST_PROGRAM:
            log_msg("AST_PROGRAM\n");
            for (size_t i = 0; i < node->data.program.decl_count; i++) {
                dump_node(node->data.program.declarations[i], depth + 1);
            }
            break;
        case AST_FUNCTION:
            format_type(node->data.function.return_type, type_buf, sizeof(type_buf));
            log_msg("AST_FUNCTION (name=");
            log_msg(node->data.function.name ? node->data.function.name : "null");
            log_msg(", return_type=");
            log_msg(type_buf);
            log_msg(")\n");
            for (size_t i = 0; i < node->data.function.param_count; i++) {
                dump_node(node->data.function.params[i], depth + 1);
            }
            dump_node(node->data.function.body, depth + 1);
            break;
        case AST_VAR_DECL:
            format_type(node->data.var_decl.var_type, type_buf, sizeof(type_buf));
            log_msg("AST_VAR_DECL (name=");
            log_msg(node->data.var_decl.name ? node->data.var_decl.name : "null");
            log_msg(", var_type=");
            log_msg(type_buf);
            log_msg(")\n");
            if (node->data.var_decl.initializer) {
                dump_node(node->data.var_decl.initializer, depth + 1);
            }
            break;
        case AST_COMPOUND_STMT:
            log_msg("AST_COMPOUND_STMT\n");
            for (size_t i = 0; i < node->data.compound.stmt_count; i++) {
                dump_node(node->data.compound.statements[i], depth + 1);
            }
            break;
        case AST_RETURN_STMT:
            log_msg("AST_RETURN_STMT\n");
            if (node->data.return_stmt.expr) {
                dump_node(node->data.return_stmt.expr, depth + 1);
            }
            break;
        case AST_IF_STMT:
            log_msg("AST_IF_STMT\n");
            dump_node(node->data.if_stmt.condition, depth + 1);
            dump_node(node->data.if_stmt.then_branch, depth + 1);
            if (node->data.if_stmt.else_branch) {
                dump_node(node->data.if_stmt.else_branch, depth + 1);
            }
            break;
        case AST_WHILE_STMT:
            log_msg("AST_WHILE_STMT\n");
            dump_node(node->data.while_stmt.condition, depth + 1);
            dump_node(node->data.while_stmt.body, depth + 1);
            break;
        case AST_FOR_STMT:
            log_msg("AST_FOR_STMT\n");
            if (node->data.for_stmt.init) dump_node(node->data.for_stmt.init, depth + 1);
            if (node->data.for_stmt.condition) dump_node(node->data.for_stmt.condition, depth + 1);
            if (node->data.for_stmt.increment) dump_node(node->data.for_stmt.increment, depth + 1);
            dump_node(node->data.for_stmt.body, depth + 1);
            break;
        case AST_ASSIGN:
            log_msg("AST_ASSIGN\n");
            dump_node(node->data.assign.lvalue, depth + 1);
            dump_node(node->data.assign.rvalue, depth + 1);
            break;
        case AST_CALL:
            log_msg("AST_CALL (name=");
            log_msg(node->data.call.name ? node->data.call.name : "null");
            log_msg(")\n");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                dump_node(node->data.call.args[i], depth + 1);
            }
            break;
        case AST_BINARY_OP:
            log_msg("AST_BINARY_OP (op=");
            log_msg(bin_op_name(node->data.binary_op.op));
            log_msg(")\n");
            dump_node(node->data.binary_op.left, depth + 1);
            dump_node(node->data.binary_op.right, depth + 1);
            break;
        case AST_UNARY_OP:
            log_msg("AST_UNARY_OP (op=");
            log_msg(unary_op_name(node->data.unary_op.op));
            log_msg(")\n");
            dump_node(node->data.unary_op.operand, depth + 1);
            break;
        case AST_IDENTIFIER:
            log_msg("AST_IDENTIFIER (name=");
            log_msg(node->data.identifier.name ? node->data.identifier.name : "null");
            log_msg(")\n");
            break;
        case AST_CONSTANT: {
            char buf[16];
            int value = node->data.constant.int_value;
            int i = 0;
            if (value == 0) {
                buf[i++] = '0';
            } else {
                int neg = 0;
                if (value < 0) {
                    neg = 1;
                    value = -value;
                }
                char tmp[16];
                int j = 0;
                while (value > 0 && j < (int)sizeof(tmp)) {
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
            break;
        }
        case AST_STRING_LITERAL:
            log_msg("AST_STRING_LITERAL (value=");
            log_msg(node->data.string_literal.value ? node->data.string_literal.value : "null");
            log_msg(")\n");
            break;
        case AST_ARRAY_ACCESS:
            log_msg("AST_ARRAY_ACCESS\n");
            dump_node(node->data.array_access.base, depth + 1);
            dump_node(node->data.array_access.index, depth + 1);
            break;
        default:
            log_msg("AST_UNKNOWN\n");
            break;
    }
}

static const char* parse_input_arg(int argc, char** argv) {
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

int main(int argc, char** argv) {
    int err = 1;
    const char* input = parse_input_arg(argc, argv);
    if (!input) {
        log_error("Usage: ast_dump <input.ast>\n");
        return 1;
    }

    reader_t* reader = reader_open(input);
    if (!reader) return 1;

    ast_reader_t ast = {0};
    if (ast_reader_init(&ast, reader) < 0) {
        log_error("Failed to read AST header\n");
        goto cleanup_reader;
    }
    if (ast_reader_load_strings(&ast) < 0) {
        log_error("Failed to read AST string table\n");
        goto cleanup_reader;
    }

    ast_node_t* root = ast_reader_read_root(&ast);
    if (!root) {
        log_error("Failed to parse AST node stream\n");
        goto cleanup_reader;
    }

    dump_node(root, 0);
    ast_tree_destroy(root);
    err = 0;

cleanup_reader:
    ast_reader_destroy(&ast);
    reader_close(reader);
    return err;
}
