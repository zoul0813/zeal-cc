#include "codegen.h"

#include "common.h"
#include "target.h"

#include <string.h>

#ifdef __SDCC
#include <core.h>
#else
#include <stdarg.h>
#include <stdio.h>
#endif

#define INITIAL_OUTPUT_CAPACITY 1024

/* Helpers */
static void codegen_emit_mangled_var(codegen_t* gen, const char* name) {
    codegen_emit(gen, "_v_");
    codegen_emit(gen, name);
}

static int codegen_names_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static void codegen_emit_int(codegen_t* gen, int value) {
    char buf[16];
    int i = 0;
    if (value == 0) {
        buf[i++] = '0';
    } else {
        if (value < 0) {
            buf[i++] = '-';
            value = -value;
        }
        char temp[16];
        int j = 0;
        while (value > 0 && j < (int)sizeof(temp)) {
            temp[j++] = '0' + (value % 10);
            value /= 10;
        }
        while (j > 0) {
            buf[i++] = temp[--j];
        }
    }
    buf[i] = '\0';
    codegen_emit(gen, buf);
}

static void codegen_record_local(codegen_t* gen, const char* name) {
    if (!gen || !name) return;
    for (size_t i = 0; i < gen->local_var_count; i++) {
        if (gen->local_vars[i] == name || codegen_names_equal(gen->local_vars[i], name)) {
            return;
        }
    }
    if (gen->local_var_count < (sizeof(gen->local_vars) / sizeof(gen->local_vars[0]))) {
        gen->local_vars[gen->local_var_count] = name;
        gen->local_offsets[gen->local_var_count] = gen->stack_offset;
        gen->stack_offset += 1;
        gen->local_var_count++;
    }
}

static int codegen_param_offset(codegen_t* gen, const char* name, int* out_offset) {
    if (!gen || !name || !out_offset) return 0;
    for (size_t i = 0; i < gen->param_count; i++) {
        if (gen->param_names[i] == name || codegen_names_equal(gen->param_names[i], name)) {
            *out_offset = gen->param_offsets[i];
            return 1;
        }
    }
    return 0;
}

static int codegen_local_offset(codegen_t* gen, const char* name, int* out_offset) {
    if (!gen || !name || !out_offset) return 0;
    for (size_t i = 0; i < gen->local_var_count; i++) {
        if (gen->local_vars[i] == name || codegen_names_equal(gen->local_vars[i], name)) {
            *out_offset = gen->local_offsets[i];
            return 1;
        }
    }
    return 0;
}

static void codegen_emit_ix_offset(codegen_t* gen, int offset) {
    (void)gen;
    codegen_emit(gen, "ix+");
    codegen_emit_int(gen, offset);
}

static void codegen_collect_locals(codegen_t* gen, ast_node_t* node) {
    if (!gen || !node) return;
    if (node->type == AST_VAR_DECL) {
        codegen_record_local(gen, node->data.var_decl.name);
        return;
    }
    switch (node->type) {
        case AST_COMPOUND_STMT:
            for (size_t i = 0; i < node->data.compound.stmt_count; i++) {
                codegen_collect_locals(gen, node->data.compound.statements[i]);
            }
            break;
        case AST_IF_STMT:
            codegen_collect_locals(gen, node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) {
                codegen_collect_locals(gen, node->data.if_stmt.else_branch);
            }
            break;
        case AST_WHILE_STMT:
            codegen_collect_locals(gen, node->data.while_stmt.body);
            break;
        case AST_FOR_STMT:
            if (node->data.for_stmt.init) codegen_collect_locals(gen, node->data.for_stmt.init);
            if (node->data.for_stmt.body) codegen_collect_locals(gen, node->data.for_stmt.body);
            break;
        default:
            break;
    }
}

static char* codegen_new_label_persist(codegen_t* gen) {
    char* tmp = codegen_new_label(gen);
    size_t len = 0;
    while (tmp[len]) len++;
    char* out = (char*)cc_malloc(len + 1);
    if (!out) return tmp;
    for (size_t i = 0; i <= len; i++) {
        out[i] = tmp[i];
    }
    return out;
}

static const char* codegen_get_string_label(codegen_t* gen, const char* value) {
    if (!gen || !value) return NULL;
    for (size_t i = 0; i < gen->string_count; i++) {
        if (gen->string_literals[i] && strcmp(gen->string_literals[i], value) == 0) {
            return gen->string_labels[i];
        }
    }
    if (gen->string_count >= (sizeof(gen->string_labels) / sizeof(gen->string_labels[0]))) {
        return NULL;
    }
    char* label = codegen_new_string_label(gen);
    if (label) {
        label = cc_strdup(label);
    }
    char* copy = cc_strdup(value);
    if (!label || !copy) {
        if (label) cc_free(label);
        if (copy) cc_free(copy);
        return NULL;
    }
    gen->string_labels[gen->string_count] = label;
    gen->string_literals[gen->string_count] = copy;
    gen->string_count++;
    return label;
}

codegen_t* codegen_create(const char* output_file, symbol_table_t* symbols) {
    codegen_t* gen = (codegen_t*)cc_malloc(sizeof(codegen_t));
    if (!gen) return NULL;

    gen->output_file = output_file;
    gen->output_handle = output_open(output_file);
    if (!gen->output_handle) {
        cc_free(gen);
        return NULL;
    }

    gen->global_symbols = symbols;
    gen->current_scope = symbols;
    gen->label_counter = 0;
    gen->string_counter = 0;
    gen->temp_counter = 0;
    gen->stack_offset = 0;

    gen->reg_a_used = false;
    gen->reg_hl_used = false;
    gen->reg_de_used = false;
    gen->reg_bc_used = false;
    gen->local_var_count = 0;
    gen->defer_var_storage = false;
    gen->param_count = 0;
    gen->function_end_label = NULL;
    gen->return_direct = false;
    gen->use_function_end_label = false;
    gen->string_count = 0;

    return gen;
}

void codegen_destroy(codegen_t* gen) {
    if (!gen) return;
    if (gen->output_handle) {
        output_close(gen->output_handle);
    }
    for (size_t i = 0; i < gen->string_count; i++) {
        if (gen->string_labels[i]) {
            cc_free((void*)gen->string_labels[i]);
        }
        if (gen->string_literals[i]) {
            cc_free(gen->string_literals[i]);
        }
    }
    cc_free(gen);
}

void codegen_emit(codegen_t* gen, const char* fmt, ...) {
    if (!gen || !gen->output_handle || !fmt) return;
    size_t len = 0;
    const char* p = fmt;
    while (p[len]) len++;
    if (len > 0) {
        output_write(gen->output_handle, fmt, (uint16_t)len);
    }
}

char* codegen_new_label(codegen_t* gen) {
    static char labels[8][16];
    static int slot = 0;
    char* label = labels[slot++ & 7];
    int n = gen->label_counter++;
    int i = 0;
    label[i++] = '_';
    label[i++] = 'l';
    if (n == 0) {
        label[i++] = '0';
    } else {
        char temp[8];
        int j = 0;
        while (n > 0 && j < (int)sizeof(temp)) {
            temp[j++] = '0' + (n % 10);
            n /= 10;
        }
        while (j > 0) {
            label[i++] = temp[--j];
        }
    }
    label[i] = '\0';
    return label;
}

char* codegen_new_string_label(codegen_t* gen) {
    static char labels[8][16];
    static int slot = 0;
    char* label = labels[slot++ & 7];
    int n = gen->string_counter++;
    int i = 0;
    label[i++] = '_';
    label[i++] = 's';
    if (n == 0) {
        label[i++] = '0';
    } else {
        char temp[8];
        int j = 0;
        while (n > 0 && j < (int)sizeof(temp)) {
            temp[j++] = '0' + (n % 10);
            n /= 10;
        }
        while (j > 0) {
            label[i++] = temp[--j];
        }
    }
    label[i] = '\0';
    return label;
}

void codegen_emit_prologue(codegen_t* gen, const char* func_name) {
    codegen_emit(gen, func_name);
    codegen_emit(gen,
        ":\n"
        "  push bc\n"
        "  push de\n"
        "  push hl\n");
}

void codegen_emit_epilogue(codegen_t* gen) {
    codegen_emit(gen,
        "  pop hl\n"
        "  pop de\n"
        "  pop bc\n"
        "  ret\n");
}

/* Forward declarations */
static cc_error_t codegen_statement(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_expression(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_global_var(codegen_t* gen, ast_node_t* node);

static cc_error_t codegen_expression(codegen_t* gen, ast_node_t* node) {
    if (!node) return CC_ERROR_INTERNAL;

    switch (node->type) {
        case AST_CONSTANT:
            /* Load constant into A register */
            codegen_emit(gen, "  ld a, ");
            {
                /* Convert int to string */
                int val = node->data.constant.int_value;
                char buf[16];
                int i = 0;
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    int neg = 0;
                    if (val < 0) {
                        neg = 1;
                        val = -val;
                    }
                    char temp[16];
                    int j = 0;
                    while (val > 0) {
                        temp[j++] = '0' + (val % 10);
                        val /= 10;
                    }
                    if (neg) buf[i++] = '-';
                    while (j > 0) {
                        buf[i++] = temp[--j];
                    }
                }
                buf[i] = '\0';
                codegen_emit(gen, buf);
            }
            codegen_emit(gen, "\n");
            return CC_OK;

        case AST_IDENTIFIER:
            /* Load variable from stack/memory */
            {
                int offset = 0;
                if (codegen_local_offset(gen, node->data.identifier.name, &offset)) {
                    codegen_emit(gen, "  ld a, (");
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, ")  ; Load local: ");
                } else if (codegen_param_offset(gen, node->data.identifier.name, &offset)) {
                    codegen_emit(gen, "  ld a, (ix+");
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, ")  ; Load param: ");
                } else {
                    codegen_emit(gen, "  ld a, (");
                    codegen_emit_mangled_var(gen, node->data.identifier.name);
                    codegen_emit(gen, ")  ; Load variable: ");
                }
                codegen_emit(gen, node->data.identifier.name);
                codegen_emit(gen, "\n");
            }
            return CC_OK;

        case AST_BINARY_OP: {
            /* Evaluate left operand (result in A) */
            cc_error_t err = codegen_expression(gen, node->data.binary_op.left);
            if (err != CC_OK) return err;

            /* For right operand, we need to save A and evaluate right */
            /* Push left result onto stack */
            codegen_emit(gen, "  push af\n");

            /* Evaluate right operand (result in A) */
            err = codegen_expression(gen, node->data.binary_op.right);
            if (err != CC_OK) return err;

            /* Pop left operand into L (for arithmetic) or B (for comparison) */
            codegen_emit(gen,
                "  ld l, a\n"
                "  pop af\n");

            /* Perform operation: A op L, result in A */
            switch (node->data.binary_op.op) {
                case OP_ADD:
                    codegen_emit(gen, "  add a, l\n");
                    break;
                case OP_SUB:
                    codegen_emit(gen, "  sub l\n");
                    break;
                case OP_MUL:
                    /* Z80 doesn't have mul, need to call helper */
                    codegen_emit(gen,
                        "; Multiplication (A * L)\n"
                        "  call __mul_a_l\n");
                    break;
                case OP_DIV:
                    /* Z80 doesn't have div, need to call helper */
                    codegen_emit(gen,
                        "; Division (A / L)\n"
                        "  call __div_a_l\n");
                    break;
                case OP_MOD:
                    /* Modulo operation */
                    codegen_emit(gen,
                        "; Modulo (A % L)\n"
                        "  call __mod_a_l\n");
                    break;

                /* Comparison operators: result is 1 (true) or 0 (false) */
                case OP_EQ:
                    codegen_emit(gen,
                        "; Equality test (A == L)\n"
                        "  cp l\n"
                        "  ld a, 0\n");
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit(gen, "  jr nz, ");
                        codegen_emit(gen, end);
                        codegen_emit(gen,
                            "\n"
                            "  ld a, 1\n");
                        codegen_emit(gen, end);
                        codegen_emit(gen, ":\n");
                    }
                    break;
                case OP_NE:
                    codegen_emit(gen,
                        "; Inequality test (A != L)\n"
                        "  cp l\n"
                        "  ld a, 0\n");
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit(gen, "  jr z, ");
                        codegen_emit(gen, end);
                        codegen_emit(gen,
                            "\n"
                            "  ld a, 1\n");
                        codegen_emit(gen, end);
                        codegen_emit(gen, ":\n");
                    }
                    break;
                case OP_LT:
                    codegen_emit(gen,
                        "; Less than test (A < L)\n"
                        "  cp l\n"
                        "  ld a, 0\n");
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit(gen, "  jr nc,");
                        codegen_emit(gen, end);
                        codegen_emit(gen,
                            "\n"
                            "  ld a, 1\n");
                        codegen_emit(gen, end);
                        codegen_emit(gen, ":\n");
                    }
                    break;
                case OP_GT:
                    codegen_emit(gen,
                        "; Greater than test (A > L)\n"
                        "  sub l\n"
                        "  ld a, 0\n");
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit(gen, "  jr z, ");
                        codegen_emit(gen, end);
                        codegen_emit(gen,
                            "\n"
                            "  jr c, ");
                        codegen_emit(gen, end);
                        codegen_emit(gen,
                            "\n"
                            "  ld a, 1\n");
                        codegen_emit(gen, end);
                        codegen_emit(gen, ":\n");
                    }
                    break;
                case OP_LE:
                    codegen_emit(gen,
                        "; Less or equal test (A <= L)\n"
                        "  sub l\n"
                        "  ld a, 0\n");
                    {
                        char* end = codegen_new_label(gen);
                        char* set = codegen_new_label(gen);
                        codegen_emit(gen, "  jr z, ");
                        codegen_emit(gen, set);
                        codegen_emit(gen,
                            "\n"
                            "  jr c, ");
                        codegen_emit(gen, set);
                        codegen_emit(gen,
                            "\n"
                            "  jr ");
                        codegen_emit(gen, end);
                        codegen_emit(gen,
                            "\n");
                        codegen_emit(gen, set);
                        codegen_emit(gen,
                            ":\n"
                            "  ld a, 1\n");
                        codegen_emit(gen, end);
                        codegen_emit(gen, ":\n");
                    }
                    break;
                case OP_GE:
                    codegen_emit(gen,
                        "; Greater or equal test (A >= L)\n"
                        "  cp l\n"
                        "  ld a, 0\n");
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit(gen, "  jr c, ");
                        codegen_emit(gen, end);
                        codegen_emit(gen,
                            "\n"
                            "  ld a, 1\n");
                        codegen_emit(gen, end);
                        codegen_emit(gen, ":\n");
                    }
                    break;

                default:
                    return CC_ERROR_CODEGEN;
            }
            return CC_OK;
        }

        case AST_CALL: {
            /* Function call - for now, simple calling convention */
            codegen_emit(gen, "; Call function: ");
            codegen_emit(gen, node->data.call.name);
            codegen_emit(gen, "\n");
            if (node->data.call.arg_count > 0) {
                for (size_t i = node->data.call.arg_count; i-- > 0;) {
                    cc_error_t err = codegen_expression(gen, node->data.call.args[i]);
                    if (err != CC_OK) return err;
                    codegen_emit(gen,
                        "  ld l, a\n"
                        "  ld h, 0\n"
                        "  push hl\n");
                }
            }
            codegen_emit(gen, "  call ");
            codegen_emit(gen, node->data.call.name);
            codegen_emit(gen, "\n");
            if (node->data.call.arg_count > 0) {
                for (size_t i = 0; i < node->data.call.arg_count; i++) {
                    codegen_emit(gen, "  pop bc\n");
                }
            }
            return CC_OK;
        }

        case AST_STRING_LITERAL:
            cc_error("String literal used without index");
            return CC_ERROR_CODEGEN;

        case AST_ARRAY_ACCESS: {
            ast_node_t* base = node->data.array_access.base;
            ast_node_t* index = node->data.array_access.index;
            if (base && base->type == AST_STRING_LITERAL &&
                index && index->type == AST_CONSTANT) {
                const char* label = codegen_get_string_label(gen, base->data.string_literal.value);
                if (!label) return CC_ERROR_CODEGEN;
                int offset = index->data.constant.int_value;
                codegen_emit(gen, "  ld hl, ");
                codegen_emit(gen, label);
                codegen_emit(gen, "\n");
                if (offset != 0) {
                    codegen_emit(gen, "  ld de, ");
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, "\n");
                    codegen_emit(gen, "  add hl, de\n");
                }
                codegen_emit(gen, "  ld a, (hl)\n");
                return CC_OK;
            }
            cc_error("Unsupported array access");
            return CC_ERROR_CODEGEN;
        }

        case AST_ASSIGN: {
            /* Assignment: evaluate RHS, store to LHS */
            cc_error_t err = codegen_expression(gen, node->data.assign.rvalue);
            if (err != CC_OK) return err;

            /* Store A to variable */
            if (node->data.assign.lvalue->type == AST_IDENTIFIER) {
                int offset = 0;
                if (codegen_local_offset(gen, node->data.assign.lvalue->data.identifier.name, &offset)) {
                    codegen_emit(gen, "  ld (");
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, "), a\n");
                } else if (codegen_param_offset(gen, node->data.assign.lvalue->data.identifier.name, &offset)) {
                    codegen_emit(gen, "  ld (ix+");
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, "), a\n");
                } else {
                    codegen_emit(gen, "  ld (");
                    codegen_emit_mangled_var(gen, node->data.assign.lvalue->data.identifier.name);
                    codegen_emit(gen, "), a\n");
                }
            }
            return CC_OK;
        }

        default:
            return CC_ERROR_CODEGEN;
    }
}

static cc_error_t codegen_statement(codegen_t* gen, ast_node_t* node) {
    if (!node) return CC_OK;

    switch (node->type) {
        case AST_RETURN_STMT:
            if (node->data.return_stmt.expr) {
                /* Evaluate return expression into A */
                cc_error_t err = codegen_expression(gen, node->data.return_stmt.expr);
                if (err != CC_OK) return err;
            } else {
                codegen_emit(gen, "  ld a, 0\n");
            }
            if (gen->return_direct || !gen->function_end_label) {
                if (gen->stack_offset > 0) {
                    codegen_emit(gen, "  ld hl, 0\n");
                    codegen_emit(gen, "  add hl, sp\n");
                    codegen_emit(gen, "  ld de, ");
                    codegen_emit_int(gen, gen->stack_offset);
                    codegen_emit(gen, "\n  add hl, de\n  ld sp, hl\n");
                }
                codegen_emit(gen,
                    "  pop ix\n"
                    "  ret\n");
            } else {
                gen->use_function_end_label = true;
                codegen_emit(gen, "  jp ");
                codegen_emit(gen, gen->function_end_label);
                codegen_emit(gen, "\n");
            }
            return CC_OK;

        case AST_VAR_DECL:
            /* Reserve space for variable */
            codegen_emit(gen, "; Variable: ");
            codegen_emit(gen, node->data.var_decl.name);
            codegen_emit(gen, "\n");

            /* If there's an initializer, generate assignment */
            if (node->data.var_decl.initializer) {
                cc_error_t err = codegen_expression(gen, node->data.var_decl.initializer);
                if (err != CC_OK) return err;
                {
                    int offset = 0;
                    if (codegen_local_offset(gen, node->data.var_decl.name, &offset)) {
                        codegen_emit(gen, "  ld (");
                        codegen_emit_ix_offset(gen, offset);
                        codegen_emit(gen, "), a\n");
                    } else {
                        codegen_emit(gen, "  ld (");
                        codegen_emit_mangled_var(gen, node->data.var_decl.name);
                        codegen_emit(gen, "), a\n");
                    }
                }
            }
            return CC_OK;

        case AST_COMPOUND_STMT:
            /* Process statements in compound block */
            for (size_t i = 0; i < node->data.compound.stmt_count; i++) {
                cc_error_t err = codegen_statement(gen, node->data.compound.statements[i]);
                if (err != CC_OK) return err;
            }
            return CC_OK;

        case AST_IF_STMT: {
            /* Evaluate condition */
            cc_error_t err = codegen_expression(gen, node->data.if_stmt.condition);
            if (err != CC_OK) return err;

            if (node->data.if_stmt.else_branch) {
                /* if-else: jump to else on false, fall through to then */
                char* else_label = codegen_new_label(gen);
                char* end_label = codegen_new_label(gen);

                codegen_emit(gen, "  or a\n  jp z, "); /* Test if A is zero */
                codegen_emit(gen, else_label);
                codegen_emit(gen, "\n");

                /* Then branch */
                err = codegen_statement(gen, node->data.if_stmt.then_branch);
                if (err != CC_OK) {
                    cc_free(else_label);
                    cc_free(end_label);
                    return err;
                }

                codegen_emit(gen, "  jp ");
                codegen_emit(gen, end_label);
                codegen_emit(gen, "\n");

                /* Else branch */
                codegen_emit(gen, else_label);
                codegen_emit(gen, ":\n");
                err = codegen_statement(gen, node->data.if_stmt.else_branch);
                if (err != CC_OK) {
                    cc_free(else_label);
                    cc_free(end_label);
                    return err;
                }

                /* End label */
                codegen_emit(gen, end_label);
                codegen_emit(gen, ":\n");

                cc_free(else_label);
                cc_free(end_label);
            } else {
                /* Simple if: jump over then branch on false */
                char* end_label = codegen_new_label(gen);

                codegen_emit(gen, "  or a\n  jp z, "); /* Test if A is zero */
                codegen_emit(gen, end_label);
                codegen_emit(gen, "\n");

                /* Then branch */
                err = codegen_statement(gen, node->data.if_stmt.then_branch);
                if (err != CC_OK) {
                    cc_free(end_label);
                    return err;
                }

                /* End label */
                codegen_emit(gen, end_label);
                codegen_emit(gen, ":\n");

                cc_free(end_label);
            }
            return CC_OK;
        }

        case AST_WHILE_STMT: {
            /* While loop: test at top, jump to end if false, jump back to top at end of body */
            char* loop_label = codegen_new_label(gen);
            char* end_label = codegen_new_label(gen);

            /* Loop start label */
            codegen_emit(gen, loop_label);
            codegen_emit(gen, ":\n");

            /* Evaluate condition */
            cc_error_t err = codegen_expression(gen, node->data.while_stmt.condition);
            if (err != CC_OK) {
                cc_free(loop_label);
                cc_free(end_label);
                return err;
            }

            /* Test condition (A register) */
            codegen_emit(gen, "  or a\n  jp z,");
            codegen_emit(gen, end_label);
            codegen_emit(gen, "\n");

            /* Loop body */
            err = codegen_statement(gen, node->data.while_stmt.body);
            if (err != CC_OK) {
                cc_free(loop_label);
                cc_free(end_label);
                return err;
            }

            /* Jump back to loop start */
            codegen_emit(gen, "  jp ");
            codegen_emit(gen, loop_label);
            codegen_emit(gen, "\n");

            /* End label */
            codegen_emit(gen, end_label);
            codegen_emit(gen, ":\n");

            cc_free(loop_label);
            cc_free(end_label);
            return CC_OK;
        }

        case AST_FOR_STMT: {
            /* For loop: init, test condition, body, increment, repeat */
            char* loop_label = codegen_new_label(gen);
            char* end_label = codegen_new_label(gen);
            cc_error_t err;

            /* Init statement */
            if (node->data.for_stmt.init) {
                err = codegen_statement(gen, node->data.for_stmt.init);
                if (err != CC_OK) {
                    cc_free(loop_label);
                    cc_free(end_label);
                    return err;
                }
            }

            /* Loop start label */
            codegen_emit(gen, loop_label);
            codegen_emit(gen, ":\n");

            /* Evaluate condition (if present) */
            if (node->data.for_stmt.condition) {
                err = codegen_expression(gen, node->data.for_stmt.condition);
                if (err != CC_OK) {
                    cc_free(loop_label);
                    cc_free(end_label);
                    return err;
                }

                /* Test condition */
                codegen_emit(gen, "  or a\n  jp z,");
                codegen_emit(gen, end_label);
                codegen_emit(gen, "\n");
            }

            /* Loop body */
            err = codegen_statement(gen, node->data.for_stmt.body);
            if (err != CC_OK) {
                cc_free(loop_label);
                cc_free(end_label);
                return err;
            }

            /* Increment expression */
            if (node->data.for_stmt.increment) {
                err = codegen_expression(gen, node->data.for_stmt.increment);
                if (err != CC_OK) {
                    cc_free(loop_label);
                    cc_free(end_label);
                    return err;
                }
            }

            /* Jump back to loop start */
            codegen_emit(gen, "  jp ");
            codegen_emit(gen, loop_label);
            codegen_emit(gen, "\n");

            /* End label */
            codegen_emit(gen, end_label);
            codegen_emit(gen, ":\n");

            cc_free(loop_label);
            cc_free(end_label);
            return CC_OK;
        }

        case AST_ASSIGN:
        case AST_CALL:
            /* Expression statements */
            return codegen_expression(gen, node);

        default:
            return CC_OK;
    }
}

static cc_error_t codegen_function(codegen_t* gen, ast_node_t* node) {
    if (!node || node->type != AST_FUNCTION) {
        return CC_ERROR_INTERNAL;
    }

    bool last_was_return = false;

    gen->local_var_count = 0;
    gen->defer_var_storage = false;
    gen->param_count = 0;
    gen->function_end_label = NULL;
    gen->return_direct = false;
    gen->use_function_end_label = false;
    gen->stack_offset = 0;

    codegen_emit(gen, node->data.function.name);
    codegen_emit(gen, ":\n");

    /* Collect locals for stack allocation */
    if (node->data.function.body) {
        codegen_collect_locals(gen, node->data.function.body);
    }

    if (node->data.function.param_count > 0) {
        for (size_t i = 0; i < node->data.function.param_count && i < 8; i++) {
            ast_node_t* param = node->data.function.params[i];
            if (!param || param->type != AST_VAR_DECL) continue;
            gen->param_names[gen->param_count] = param->data.var_decl.name;
            gen->param_offsets[gen->param_count] = gen->stack_offset + 4 + (int)(2 * gen->param_count);
            gen->param_count++;
        }
    }

    gen->function_end_label = codegen_new_label_persist(gen);
    codegen_emit(gen,
        "  push ix\n"
        "  ld ix, 0\n"
        "  add ix, sp\n");
    if (gen->stack_offset > 0) {
        codegen_emit(gen, "  ld hl, 0\n");
        codegen_emit(gen, "  add hl, sp\n");
        codegen_emit(gen, "  ld de, ");
        codegen_emit_int(gen, gen->stack_offset);
        codegen_emit(gen, "\n  or a\n  sbc hl, de\n  ld sp, hl\n");
        codegen_emit(gen, "  ld ix, 0\n");
        codegen_emit(gen, "  add ix, sp\n");
    }

    /* Generate function body */
    if (node->data.function.body) {
        if (node->data.function.body->type == AST_COMPOUND_STMT &&
            node->data.function.body->data.compound.stmt_count > 0) {
            size_t count = node->data.function.body->data.compound.stmt_count;
            for (size_t i = 0; i + 1 < count; i++) {
                cc_error_t err = codegen_statement(gen, node->data.function.body->data.compound.statements[i]);
                if (err != CC_OK) return err;
            }
            ast_node_t* last = node->data.function.body->data.compound.statements[count - 1];
            if (last && last->type == AST_RETURN_STMT) {
                last_was_return = true;
                gen->return_direct = true;
            }
            {
                cc_error_t err = codegen_statement(gen, last);
                gen->return_direct = false;
                if (err != CC_OK) return err;
            }
        } else if (node->data.function.body->type == AST_RETURN_STMT) {
            gen->return_direct = true;
            last_was_return = true;
            cc_error_t err = codegen_statement(gen, node->data.function.body);
            gen->return_direct = false;
            if (err != CC_OK) return err;
        } else {
            cc_error_t err = codegen_statement(gen, node->data.function.body);
            if (err != CC_OK) return err;
        }
    } else {
                codegen_emit(gen, "  ld a, 0\n");
            }

    if (gen->use_function_end_label) {
        codegen_emit(gen, gen->function_end_label);
        codegen_emit(gen, ":\n");
        if (gen->stack_offset > 0) {
            codegen_emit(gen, "  ld hl, 0\n");
            codegen_emit(gen, "  add hl, sp\n");
            codegen_emit(gen, "  ld de, ");
            codegen_emit_int(gen, gen->stack_offset);
            codegen_emit(gen, "\n  add hl, de\n  ld sp, hl\n");
        }
        codegen_emit(gen,
            "  pop ix\n"
            "  ret\n");
    } else if (!last_was_return) {
        if (gen->stack_offset > 0) {
            codegen_emit(gen, "  ld hl, 0\n");
            codegen_emit(gen, "  add hl, sp\n");
            codegen_emit(gen, "  ld de, ");
            codegen_emit_int(gen, gen->stack_offset);
            codegen_emit(gen, "\n  add hl, de\n  ld sp, hl\n");
        }
        codegen_emit(gen,
            "  pop ix\n"
            "  ret\n");
    }

    gen->defer_var_storage = false;
    if (gen->function_end_label) {
        cc_free(gen->function_end_label);
        gen->function_end_label = NULL;
    }
    codegen_emit(gen, "\n");

    return CC_OK;
}

static cc_error_t codegen_global_var(codegen_t* gen, ast_node_t* node) {
    if (!node || node->type != AST_VAR_DECL) {
        return CC_ERROR_INTERNAL;
    }
    codegen_emit(gen, "; Global variable: ");
    codegen_emit(gen, node->data.var_decl.name);
    codegen_emit(gen, "\n");
    codegen_emit_mangled_var(gen, node->data.var_decl.name);
    if (node->data.var_decl.initializer &&
        node->data.var_decl.initializer->type == AST_CONSTANT) {
        int value = node->data.var_decl.initializer->data.constant.int_value;
        codegen_emit(gen, ":\n  .db ");
        codegen_emit_int(gen, value);
        codegen_emit(gen, "\n");
    } else {
        codegen_emit(gen, ":\n  .db 0\n");
    }
    return CC_OK;
}

cc_error_t codegen_generate_global(codegen_t* gen, ast_node_t* decl) {
    return codegen_global_var(gen, decl);
}

void codegen_emit_runtime(codegen_t* gen) {
    codegen_emit(gen, "\n; Runtime library functions\n\n");

    codegen_emit(gen,
        "; Multiply A by L\n"
        "__mul_a_l:\n"
        "  ld b, l      ; multiplier (loop counter)\n"
        "  ld c, a      ; multiplicand\n"
        "  ld a, b\n"
        "  or c\n"
        "  ret z        ; short-circuit if either operand is zero\n"
        "  ld a, 0      ; result accumulator\n"
        "__mul_loop:\n"
        "  add a, c\n"
        "  djnz __mul_loop\n"
        "  ret\n"
        "\n"
        "; Divide A by L\n"
        "__div_a_l:\n"
        "  ld c, a      ; dividend\n"
        "  ld a, l\n"
        "  or a\n"
        "  ret z        ; divide by zero -> 0\n"
        "  ld b, 0      ; quotient\n"
        "__div_loop:\n"
        "  ld a, c\n"
        "  cp l\n"
        "  jr c, __div_done\n"
        "  sub l\n"
        "  ld c, a\n"
        "  inc b\n"
        "  jr __div_loop\n"
        "__div_done:\n"
        "  ld a, b      ; result = quotient\n"
        "  ret\n"
        "\n"
        "; Modulo A by L\n"
        "__mod_a_l:\n"
        "  ld b, l      ; divisor\n"
        "  ld c, a      ; remainder working copy\n"
        "  ld a, b\n"
        "  or a\n"
        "  ret z        ; divide by zero -> 0\n"
        "__mod_loop:\n"
        "  ld a, c\n"
        "  cp b\n"
        "  jr c, __mod_done\n"
        "  sub b\n"
        "  ld c, a\n"
        "  jr __mod_loop\n"
        "__mod_done:\n"
        "  ld a, c\n"
        "  ret\n"
    );
}

void codegen_emit_strings(codegen_t* gen) {
    if (!gen || gen->string_count == 0) return;
    codegen_emit(gen, "\n; String literals\n");
    for (size_t i = 0; i < gen->string_count; i++) {
        const char* label = gen->string_labels[i];
        const char* value = gen->string_literals[i];
        if (!label || !value) continue;
        codegen_emit(gen, label);
        codegen_emit(gen, ":\n");
        size_t idx = 0;
        while (value[idx]) {
            codegen_emit(gen, "  .db ");
            codegen_emit_int(gen, (unsigned char)value[idx]);
            codegen_emit(gen, "\n");
            idx++;
        }
        codegen_emit(gen, "  .db 0\n");
    }
}

// Helper: emit contents of a file to the output buffer
static void codegen_emit_crt0(codegen_t* gen) {
    codegen_emit(gen,
        "; Zeal 8-bit OS crt0.asm - minimal startup for user programs\n"
        "; Provides _start entry, calls _main, then exits via syscall\n"
        "\n"
        "; Start of crt0.asm\n"
        "_start:\n"
        "  call main    ; Call user main()\n"
        "  ld h, a      ; Move return value from A to H (compiler ABI)\n"
        "  ld l, 15     ; EXIT syscall\n"
        "  rst 0x8      ; ZOS exit syscall (returns to shell)\n"
        "  halt\n"
        "\n"
        "; End of crt0.asm\n"
    );
}

void codegen_emit_preamble(codegen_t* gen) {
    if (!gen) return;
    codegen_emit(gen,
        "; Generated by Zeal 8-bit C Compiler\n"
        "; Target: Z80\n\n"
        "  org 0x4000\n"
        "\n");
    codegen_emit_crt0(gen);
    codegen_emit(gen, "\n; Program code\n");
}

cc_error_t codegen_generate_function(codegen_t* gen, ast_node_t* func) {
    if (!gen || !func) {
        return CC_ERROR_INTERNAL;
    }
    if (func->type != AST_FUNCTION) {
        return CC_OK;
    }
    return codegen_function(gen, func);
}

cc_error_t codegen_generate(codegen_t* gen, ast_node_t* ast) {
    if (!gen || !ast) {
        return CC_ERROR_INTERNAL;
    }

    codegen_emit_preamble(gen);

    /* Process AST - handle both PROGRAM node and direct FUNCTION node */
    if (ast->type == AST_PROGRAM) {
        /* Generate code for all top-level declarations */
        for (size_t i = 0; i < ast->data.program.decl_count; i++) {
            ast_node_t* decl = ast->data.program.declarations[i];
            if (decl->type == AST_FUNCTION) {
                cc_error_t err = codegen_function(gen, decl);
                if (err != CC_OK) return err;
            }
        }
        for (size_t i = 0; i < ast->data.program.decl_count; i++) {
            ast_node_t* decl = ast->data.program.declarations[i];
            if (decl->type == AST_VAR_DECL) {
                cc_error_t err = codegen_global_var(gen, decl);
                if (err != CC_OK) return err;
            }
        }

        codegen_emit_strings(gen);
        /* Emit runtime library */
        codegen_emit_runtime(gen);

        return CC_OK;
    } else if (ast->type == AST_FUNCTION) {
        /* Direct function node */
        cc_error_t err = codegen_function(gen, ast);
        if (err != CC_OK) return err;

        codegen_emit_strings(gen);
        /* Emit runtime library */
        codegen_emit_runtime(gen);

        return err;
    }

    return CC_OK;
}

cc_error_t codegen_write_output(codegen_t* gen) {
    /* Streaming write already performed; nothing to do */
    (void)gen;
    return CC_OK;
}
