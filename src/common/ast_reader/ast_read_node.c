#include "ast_reader_internal.h"

#include "ast_format.h"
#include "ast_io.h"
#include "cc_compat.h"
#include "common.h"
#include "symbol.h"

static ast_node_t* ast_node_alloc(ast_node_type_t type) {
    ast_node_t* node = (ast_node_t*)cc_malloc(sizeof(ast_node_t));
    if (!node) return NULL;
    mem_set(node, 0, sizeof(*node));
    node->type = type;
    return node;
}

static char* ast_strdup_index(ast_reader_t* ast, uint16_t index) {
    if (!ast || !ast->strings || index >= ast->string_count) return NULL;
    return cc_strdup(ast->strings[index]);
}

static int8_t ast_read_type(ast_reader_t* ast, type_t** out) {
    uint8_t base = 0;
    uint8_t depth = 0;
    if (!out) return -1;
    if (ast_read_u8(ast->reader, &base) < 0) return -1;
    if (ast_read_u8(ast->reader, &depth) < 0) return -1;

    type_t* type = NULL;
    switch (base) {
        case AST_BASE_INT:
            type = type_create(TYPE_INT);
            break;
        case AST_BASE_CHAR:
            type = type_create(TYPE_CHAR);
            break;
        case AST_BASE_VOID:
            type = type_create(TYPE_VOID);
            break;
        default:
            return -1;
    }
    if (!type) return -1;
    while (depth-- > 0) {
        type_t* next = type_create_pointer(type);
        if (!next) {
            type_destroy(type);
            return -1;
        }
        type = next;
    }
    *out = type;
    return 0;
}

ast_node_t* ast_read_node(ast_reader_t* ast) {
    uint8_t tag = 0;
    if (ast_read_u8(ast->reader, &tag) < 0) return NULL;

    switch (tag) {
        case AST_TAG_PROGRAM: {
            uint16_t decl_count = 0;
            if (ast_read_u16(ast->reader, &decl_count) < 0) return NULL;
            ast_node_t* node = ast_node_alloc(AST_PROGRAM);
            if (!node) return NULL;
            node->data.program.decl_count = decl_count;
            if (decl_count > 0) {
                node->data.program.declarations = (ast_node_t**)cc_malloc(
                    sizeof(ast_node_t*) * decl_count);
                if (!node->data.program.declarations) {
                    ast_tree_destroy(node);
                    return NULL;
                }
                for (uint16_t i = 0; i < decl_count; i++) {
                    node->data.program.declarations[i] = ast_read_node(ast);
                    if (!node->data.program.declarations[i]) {
                        ast_tree_destroy(node);
                        return NULL;
                    }
                }
            }
            return node;
        }
        case AST_TAG_FUNCTION: {
            uint16_t name_index = 0;
            uint8_t param_count = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return NULL;
            ast_node_t* node = ast_node_alloc(AST_FUNCTION);
            if (!node) return NULL;
            node->data.function.name = ast_strdup_index(ast, name_index);
            if (!node->data.function.name) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (ast_read_type(ast, &node->data.function.return_type) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (ast_read_u8(ast->reader, &param_count) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.function.param_count = param_count;
            if (param_count > 0) {
                node->data.function.params = (ast_node_t**)cc_malloc(
                    sizeof(ast_node_t*) * param_count);
                if (!node->data.function.params) {
                    ast_tree_destroy(node);
                    return NULL;
                }
                for (uint8_t i = 0; i < param_count; i++) {
                    node->data.function.params[i] = ast_read_node(ast);
                    if (!node->data.function.params[i]) {
                        ast_tree_destroy(node);
                        return NULL;
                    }
                }
            }
            node->data.function.body = ast_read_node(ast);
            if (!node->data.function.body) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_VAR_DECL: {
            uint16_t name_index = 0;
            uint8_t has_init = 0;
            ast_node_t* node = ast_node_alloc(AST_VAR_DECL);
            if (!node) return NULL;
            if (ast_read_u16(ast->reader, &name_index) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.var_decl.name = ast_strdup_index(ast, name_index);
            if (!node->data.var_decl.name) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (ast_read_type(ast, &node->data.var_decl.var_type) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (ast_read_u8(ast->reader, &has_init) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (has_init) {
                node->data.var_decl.initializer = ast_read_node(ast);
                if (!node->data.var_decl.initializer) {
                    ast_tree_destroy(node);
                    return NULL;
                }
            }
            return node;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = 0;
            if (ast_read_u16(ast->reader, &stmt_count) < 0) return NULL;
            ast_node_t* node = ast_node_alloc(AST_COMPOUND_STMT);
            if (!node) return NULL;
            node->data.compound.stmt_count = stmt_count;
            if (stmt_count > 0) {
                node->data.compound.statements = (ast_node_t**)cc_malloc(
                    sizeof(ast_node_t*) * stmt_count);
                if (!node->data.compound.statements) {
                    ast_tree_destroy(node);
                    return NULL;
                }
                for (uint16_t i = 0; i < stmt_count; i++) {
                    node->data.compound.statements[i] = ast_read_node(ast);
                    if (!node->data.compound.statements[i]) {
                        ast_tree_destroy(node);
                        return NULL;
                    }
                }
            }
            return node;
        }
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = 0;
            ast_node_t* node = ast_node_alloc(AST_RETURN_STMT);
            if (!node) return NULL;
            if (ast_read_u8(ast->reader, &has_expr) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (has_expr) {
                node->data.return_stmt.expr = ast_read_node(ast);
                if (!node->data.return_stmt.expr) {
                    ast_tree_destroy(node);
                    return NULL;
                }
            }
            return node;
        }
        case AST_TAG_IF_STMT: {
            uint8_t has_else = 0;
            ast_node_t* node = ast_node_alloc(AST_IF_STMT);
            if (!node) return NULL;
            if (ast_read_u8(ast->reader, &has_else) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.if_stmt.condition = ast_read_node(ast);
            if (!node->data.if_stmt.condition) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.if_stmt.then_branch = ast_read_node(ast);
            if (!node->data.if_stmt.then_branch) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (has_else) {
                node->data.if_stmt.else_branch = ast_read_node(ast);
                if (!node->data.if_stmt.else_branch) {
                    ast_tree_destroy(node);
                    return NULL;
                }
            }
            return node;
        }
        case AST_TAG_WHILE_STMT: {
            ast_node_t* node = ast_node_alloc(AST_WHILE_STMT);
            if (!node) return NULL;
            node->data.while_stmt.condition = ast_read_node(ast);
            if (!node->data.while_stmt.condition) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.while_stmt.body = ast_read_node(ast);
            if (!node->data.while_stmt.body) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = 0;
            uint8_t has_cond = 0;
            uint8_t has_inc = 0;
            ast_node_t* node = ast_node_alloc(AST_FOR_STMT);
            if (!node) return NULL;
            if (ast_read_u8(ast->reader, &has_init) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (ast_read_u8(ast->reader, &has_cond) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (ast_read_u8(ast->reader, &has_inc) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (has_init) {
                node->data.for_stmt.init = ast_read_node(ast);
                if (!node->data.for_stmt.init) {
                    ast_tree_destroy(node);
                    return NULL;
                }
            }
            if (has_cond) {
                node->data.for_stmt.condition = ast_read_node(ast);
                if (!node->data.for_stmt.condition) {
                    ast_tree_destroy(node);
                    return NULL;
                }
            }
            if (has_inc) {
                node->data.for_stmt.increment = ast_read_node(ast);
                if (!node->data.for_stmt.increment) {
                    ast_tree_destroy(node);
                    return NULL;
                }
            }
            node->data.for_stmt.body = ast_read_node(ast);
            if (!node->data.for_stmt.body) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_ASSIGN: {
            ast_node_t* node = ast_node_alloc(AST_ASSIGN);
            if (!node) return NULL;
            node->data.assign.lvalue = ast_read_node(ast);
            if (!node->data.assign.lvalue) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.assign.rvalue = ast_read_node(ast);
            if (!node->data.assign.rvalue) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_CALL: {
            uint16_t name_index = 0;
            uint8_t arg_count = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return NULL;
            ast_node_t* node = ast_node_alloc(AST_CALL);
            if (!node) return NULL;
            node->data.call.name = ast_strdup_index(ast, name_index);
            if (!node->data.call.name) {
                ast_tree_destroy(node);
                return NULL;
            }
            if (ast_read_u8(ast->reader, &arg_count) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.call.arg_count = arg_count;
            if (arg_count > 0) {
                node->data.call.args = (ast_node_t**)cc_malloc(
                    sizeof(ast_node_t*) * arg_count);
                if (!node->data.call.args) {
                    ast_tree_destroy(node);
                    return NULL;
                }
                for (uint8_t i = 0; i < arg_count; i++) {
                    node->data.call.args[i] = ast_read_node(ast);
                    if (!node->data.call.args[i]) {
                        ast_tree_destroy(node);
                        return NULL;
                    }
                }
            }
            return node;
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = 0;
            ast_node_t* node = ast_node_alloc(AST_BINARY_OP);
            if (!node) return NULL;
            if (ast_read_u8(ast->reader, &op) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.binary_op.op = (binary_op_t)op;
            node->data.binary_op.left = ast_read_node(ast);
            if (!node->data.binary_op.left) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.binary_op.right = ast_read_node(ast);
            if (!node->data.binary_op.right) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_UNARY_OP: {
            uint8_t op = 0;
            ast_node_t* node = ast_node_alloc(AST_UNARY_OP);
            if (!node) return NULL;
            if (ast_read_u8(ast->reader, &op) < 0) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.unary_op.op = (unary_op_t)op;
            node->data.unary_op.operand = ast_read_node(ast);
            if (!node->data.unary_op.operand) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_IDENTIFIER: {
            uint16_t name_index = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return NULL;
            ast_node_t* node = ast_node_alloc(AST_IDENTIFIER);
            if (!node) return NULL;
            node->data.identifier.name = ast_strdup_index(ast, name_index);
            if (!node->data.identifier.name) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_CONSTANT: {
            int16_t value = 0;
            if (ast_read_i16(ast->reader, &value) < 0) return NULL;
            ast_node_t* node = ast_node_alloc(AST_CONSTANT);
            if (!node) return NULL;
            node->data.constant.int_value = value;
            return node;
        }
        case AST_TAG_STRING_LITERAL: {
            uint16_t value_index = 0;
            if (ast_read_u16(ast->reader, &value_index) < 0) return NULL;
            ast_node_t* node = ast_node_alloc(AST_STRING_LITERAL);
            if (!node) return NULL;
            node->data.string_literal.value = ast_strdup_index(ast, value_index);
            if (!node->data.string_literal.value) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        case AST_TAG_ARRAY_ACCESS: {
            ast_node_t* node = ast_node_alloc(AST_ARRAY_ACCESS);
            if (!node) return NULL;
            node->data.array_access.base = ast_read_node(ast);
            if (!node->data.array_access.base) {
                ast_tree_destroy(node);
                return NULL;
            }
            node->data.array_access.index = ast_read_node(ast);
            if (!node->data.array_access.index) {
                ast_tree_destroy(node);
                return NULL;
            }
            return node;
        }
        default:
            return NULL;
    }
}
