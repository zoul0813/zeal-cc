#include "ast_reader.h"

#include "common.h"
#include "symbol.h"

void ast_tree_destroy(ast_node_t* node) {
    if (!node) return;
    switch (node->type) {
        case AST_PROGRAM:
            if (node->data.program.declarations) {
                for (ast_count_t i = 0; i < node->data.program.decl_count; i++) {
                    ast_tree_destroy(node->data.program.declarations[i]);
                }
                cc_free(node->data.program.declarations);
            }
            break;
        case AST_COMPOUND_STMT:
            if (node->data.compound.statements) {
                for (ast_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                    ast_tree_destroy(node->data.compound.statements[i]);
                }
                cc_free(node->data.compound.statements);
            }
            break;
        case AST_IF_STMT:
            ast_tree_destroy(node->data.if_stmt.condition);
            ast_tree_destroy(node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) {
                ast_tree_destroy(node->data.if_stmt.else_branch);
            }
            break;
        case AST_WHILE_STMT:
            ast_tree_destroy(node->data.while_stmt.condition);
            ast_tree_destroy(node->data.while_stmt.body);
            break;
        case AST_FOR_STMT:
            if (node->data.for_stmt.init) {
                ast_tree_destroy(node->data.for_stmt.init);
            }
            if (node->data.for_stmt.condition) {
                ast_tree_destroy(node->data.for_stmt.condition);
            }
            if (node->data.for_stmt.increment) {
                ast_tree_destroy(node->data.for_stmt.increment);
            }
            ast_tree_destroy(node->data.for_stmt.body);
            break;
        case AST_IDENTIFIER:
            if (node->data.identifier.name) {
                cc_free(node->data.identifier.name);
            }
            break;
        case AST_FUNCTION:
            if (node->data.function.name) {
                cc_free(node->data.function.name);
            }
            if (node->data.function.return_type) {
                type_destroy(node->data.function.return_type);
            }
            if (node->data.function.params) {
                for (ast_count_t i = 0; i < node->data.function.param_count; i++) {
                    ast_tree_destroy(node->data.function.params[i]);
                }
                cc_free(node->data.function.params);
            }
            if (node->data.function.body) {
                ast_tree_destroy(node->data.function.body);
            }
            break;
        case AST_STRING_LITERAL:
            if (node->data.string_literal.value) {
                cc_free(node->data.string_literal.value);
            }
            break;
        case AST_BINARY_OP:
            ast_tree_destroy(node->data.binary_op.left);
            ast_tree_destroy(node->data.binary_op.right);
            break;
        case AST_UNARY_OP:
            ast_tree_destroy(node->data.unary_op.operand);
            break;
        case AST_RETURN_STMT:
            if (node->data.return_stmt.expr) {
                ast_tree_destroy(node->data.return_stmt.expr);
            }
            break;
        case AST_VAR_DECL:
            if (node->data.var_decl.name) {
                cc_free(node->data.var_decl.name);
            }
            if (node->data.var_decl.var_type) {
                type_destroy(node->data.var_decl.var_type);
            }
            if (node->data.var_decl.initializer) {
                ast_tree_destroy(node->data.var_decl.initializer);
            }
            break;
        case AST_ASSIGN:
            ast_tree_destroy(node->data.assign.lvalue);
            ast_tree_destroy(node->data.assign.rvalue);
            break;
        case AST_CALL:
            if (node->data.call.name) {
                cc_free(node->data.call.name);
            }
            if (node->data.call.args) {
                for (ast_count_t i = 0; i < node->data.call.arg_count; i++) {
                    ast_tree_destroy(node->data.call.args[i]);
                }
                cc_free(node->data.call.args);
            }
            break;
        case AST_ARRAY_ACCESS:
            ast_tree_destroy(node->data.array_access.base);
            ast_tree_destroy(node->data.array_access.index);
            break;
        default:
            break;
    }
    cc_free(node);
}
