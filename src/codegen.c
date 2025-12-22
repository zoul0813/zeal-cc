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

static void codegen_record_local(codegen_t* gen, const char* name, size_t size) {
    if (!gen || !name) return;
    for (size_t i = 0; i < gen->local_var_count; i++) {
        if (gen->local_var_names[i] == name || codegen_names_equal(gen->local_var_names[i], name)) {
            gen->local_var_sizes[i] = size;
            return;
        }
    }
    if (gen->local_var_count < (sizeof(gen->local_var_names) / sizeof(gen->local_var_names[0]))) {
        gen->local_var_names[gen->local_var_count] = name;
        gen->local_var_sizes[gen->local_var_count] = size;
        gen->local_var_count++;
    }
}

static int codegen_lookup_param(codegen_t* gen, const char* name, int* out_offset, size_t* out_size) {
    if (!gen || !name || !out_offset || !out_size) return 0;
    for (size_t i = 0; i < gen->param_count; i++) {
        if (gen->param_names[i] == name || codegen_names_equal(gen->param_names[i], name)) {
            *out_offset = gen->param_offsets[i];
            *out_size = gen->param_sizes[i];
            return 1;
        }
    }
    return 0;
}

static int codegen_lookup_local(codegen_t* gen, const char* name, size_t* out_size) {
    if (!gen || !name || !out_size) return 0;
    for (size_t i = 0; i < gen->local_var_count; i++) {
        if (gen->local_var_names[i] == name || codegen_names_equal(gen->local_var_names[i], name)) {
            *out_size = gen->local_var_sizes[i];
            return 1;
        }
    }
    return 0;
}

static void codegen_emit_addr(codegen_t* gen, const char* name, bool is_var) {
    if (is_var) {
        codegen_emit_mangled_var(gen, name);
        return;
    }
    codegen_emit(gen, name);
}

static void codegen_emit_addr_offset(codegen_t* gen, const char* name, bool is_var, int offset) {
    codegen_emit_addr(gen, name, is_var);
    if (offset > 0) {
        codegen_emit(gen, "+");
        codegen_emit_int(gen, offset);
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

static char* codegen_new_temp_label(codegen_t* gen) {
    char buf[16];
    int n = gen->temp_counter++;
    int i = 0;
    buf[i++] = '_';
    buf[i++] = 't';
    if (n == 0) {
        buf[i++] = '0';
    } else {
        char temp[8];
        int j = 0;
        while (n > 0 && j < (int)sizeof(temp)) {
            temp[j++] = '0' + (n % 10);
            n /= 10;
        }
        while (j > 0) {
            buf[i++] = temp[--j];
        }
    }
    buf[i] = '\0';
    char* out = (char*)cc_malloc((size_t)i + 1);
    if (!out) return NULL;
    for (int k = 0; k <= i; k++) {
        out[k] = buf[k];
    }
    return out;
}

static const char* codegen_record_temp(codegen_t* gen, size_t size) {
    if (!gen || gen->temp_count >= (sizeof(gen->temp_names) / sizeof(gen->temp_names[0]))) {
        return NULL;
    }
    char* label = codegen_new_temp_label(gen);
    if (!label) return NULL;
    gen->temp_names[gen->temp_count] = label;
    gen->temp_sizes[gen->temp_count] = size;
    gen->temp_count++;
    return label;
}

static size_t codegen_type_storage_size(const type_t* type) {
    if (!type) return 1;
    if (type->kind == TYPE_LONG) return 4;
    if (type->kind == TYPE_CHAR) return 1;
    if (type->kind == TYPE_VOID) return 0;
    return 1;
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
    gen->temp_count = 0;
    gen->current_return_size = 1;
    gen->func_count = 0;
    gen->function_end_label = NULL;
    gen->return_direct = false;
    gen->use_function_end_label = false;

    return gen;
}

void codegen_destroy(codegen_t* gen) {
    if (!gen) return;
    if (gen->output_handle) {
        output_close(gen->output_handle);
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
static size_t codegen_expr_size(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_expression_u8(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_expression_u32(codegen_t* gen, ast_node_t* node, const char* dest,
    const char** out_addr);

static int codegen_find_function(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (size_t i = 0; i < gen->func_count; i++) {
        if (gen->func_names[i] == name || codegen_names_equal(gen->func_names[i], name)) {
            return (int)i;
        }
    }
    return -1;
}

static void codegen_register_function_signature(codegen_t* gen, ast_node_t* func) {
    if (!gen || !func || func->type != AST_FUNCTION) return;
    if (gen->func_count >= (sizeof(gen->func_names) / sizeof(gen->func_names[0]))) return;
    size_t idx = gen->func_count++;
    gen->func_names[idx] = func->data.function.name;
    gen->func_return_sizes[idx] = codegen_type_storage_size(func->data.function.return_type);
    gen->func_param_counts[idx] = func->data.function.param_count;
    for (size_t i = 0; i < func->data.function.param_count && i < 8; i++) {
        ast_node_t* param = func->data.function.params[i];
        size_t size = 1;
        if (param && param->type == AST_VAR_DECL) {
            size = codegen_type_storage_size(param->data.var_decl.var_type);
        }
        gen->func_param_sizes[idx][i] = size;
    }
}

static void codegen_collect_function_signatures(codegen_t* gen, ast_node_t* ast) {
    if (!gen || !ast) return;
    gen->func_count = 0;
    if (ast->type == AST_PROGRAM) {
        for (size_t i = 0; i < ast->data.program.decl_count; i++) {
            ast_node_t* decl = ast->data.program.declarations[i];
            if (decl && decl->type == AST_FUNCTION) {
                codegen_register_function_signature(gen, decl);
            }
        }
        return;
    }
    if (ast->type == AST_FUNCTION) {
        codegen_register_function_signature(gen, ast);
    }
}

static int codegen_is_comparison(binary_op_t op) {
    return op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_LE || op == OP_GT || op == OP_GE;
}

static size_t codegen_expr_size(codegen_t* gen, ast_node_t* node) {
    if (!node) return 1;
    switch (node->type) {
        case AST_IDENTIFIER: {
            size_t size = 1;
            size_t local_size = 0;
            int offset = 0;
            size_t param_size = 0;
            if (codegen_lookup_local(gen, node->data.identifier.name, &local_size)) {
                size = local_size;
            } else if (codegen_lookup_param(gen, node->data.identifier.name, &offset, &param_size)) {
                size = param_size;
            }
            return size == 4 ? 4 : 1;
        }
        case AST_ASSIGN:
            if (node->data.assign.lvalue && node->data.assign.lvalue->type == AST_IDENTIFIER) {
                size_t size = 1;
                size_t local_size = 0;
                int offset = 0;
                size_t param_size = 0;
                if (codegen_lookup_local(gen, node->data.assign.lvalue->data.identifier.name, &local_size)) {
                    size = local_size;
                } else if (codegen_lookup_param(gen, node->data.assign.lvalue->data.identifier.name,
                               &offset, &param_size)) {
                    size = param_size;
                }
                return size == 4 ? 4 : 1;
            }
            return 1;
        case AST_CALL: {
            int idx = codegen_find_function(gen, node->data.call.name);
            if (idx >= 0) {
                return gen->func_return_sizes[idx] == 4 ? 4 : 1;
            }
            return 1;
        }
        case AST_BINARY_OP: {
            if (codegen_is_comparison(node->data.binary_op.op)) {
                return 1;
            }
            size_t left = codegen_expr_size(gen, node->data.binary_op.left);
            size_t right = codegen_expr_size(gen, node->data.binary_op.right);
            if (left == 4 || right == 4) return 4;
            return 1;
        }
        default:
            return 1;
    }
}

static void codegen_emit_store_u32_immediate(codegen_t* gen, const char* dest, int32_t value) {
    unsigned int v = (unsigned int)value;
    for (int i = 0; i < 4; i++) {
        codegen_emit(gen, "  ld a, ");
        codegen_emit_int(gen, (int)(v & 0xFF));
        codegen_emit(gen, "\n  ld (");
        codegen_emit_addr_offset(gen, dest, false, i);
        codegen_emit(gen, "), a\n");
        v >>= 8;
    }
}

static void codegen_emit_store_u32_zero_extend_a(codegen_t* gen, const char* dest) {
    codegen_emit(gen, "  ld (");
    codegen_emit_addr(gen, dest, false);
    codegen_emit(gen, "), a\n");
    for (int i = 1; i < 4; i++) {
        codegen_emit(gen, "  ld a, 0\n  ld (");
        codegen_emit_addr_offset(gen, dest, false, i);
        codegen_emit(gen, "), a\n");
    }
}

static void codegen_emit_copy_u32_from_ix(codegen_t* gen, const char* dest, int offset) {
    for (int i = 0; i < 4; i++) {
        codegen_emit(gen, "  ld a, (ix+");
        codegen_emit_int(gen, offset + i);
        codegen_emit(gen, ")\n  ld (");
        codegen_emit_addr_offset(gen, dest, false, i);
        codegen_emit(gen, "), a\n");
    }
}

static void codegen_emit_call_u32_op(codegen_t* gen,
    const char* op,
    const char* dest, bool dest_is_var,
    const char* left, bool left_is_var,
    const char* right, bool right_is_var) {
    codegen_emit(gen, "  ld hl, ");
    codegen_emit_addr(gen, left, left_is_var);
    codegen_emit(gen, "\n  ld de, ");
    codegen_emit_addr(gen, right, right_is_var);
    codegen_emit(gen, "\n  ld bc, ");
    codegen_emit_addr(gen, dest, dest_is_var);
    codegen_emit(gen, "\n  call ");
    codegen_emit(gen, op);
    codegen_emit(gen, "\n");
}

static void codegen_emit_cmp32(codegen_t* gen, binary_op_t op,
    const char* left, bool left_is_var,
    const char* right, bool right_is_var) {
    char* set_true = codegen_new_label(gen);
    char* set_false = codegen_new_label(gen);
    char* done = codegen_new_label(gen);

    for (int i = 3; i >= 0; i--) {
        codegen_emit(gen, "  ld a, (");
        codegen_emit_addr_offset(gen, left, left_is_var, i);
        codegen_emit(gen, ")\n  cp (");
        codegen_emit_addr_offset(gen, right, right_is_var, i);
        codegen_emit(gen, ")\n");
        if (op == OP_EQ || op == OP_NE) {
            codegen_emit(gen, "  jr nz, ");
            codegen_emit(gen, op == OP_EQ ? set_false : set_true);
            codegen_emit(gen, "\n");
        } else if (op == OP_LT || op == OP_LE) {
            codegen_emit(gen, "  jr c, ");
            codegen_emit(gen, set_true);
            codegen_emit(gen, "\n  jr nz, ");
            codegen_emit(gen, set_false);
            codegen_emit(gen, "\n");
        } else if (op == OP_GT || op == OP_GE) {
            codegen_emit(gen, "  jr c, ");
            codegen_emit(gen, set_false);
            codegen_emit(gen, "\n  jr nz, ");
            codegen_emit(gen, set_true);
            codegen_emit(gen, "\n");
        }
    }

    if (op == OP_EQ) {
        codegen_emit(gen, "  jr ");
        codegen_emit(gen, set_true);
        codegen_emit(gen, "\n");
    } else if (op == OP_NE) {
        codegen_emit(gen, "  jr ");
        codegen_emit(gen, set_false);
        codegen_emit(gen, "\n");
    } else if (op == OP_LE || op == OP_GE) {
        codegen_emit(gen, "  jr ");
        codegen_emit(gen, set_true);
        codegen_emit(gen, "\n");
    } else {
        codegen_emit(gen, "  jr ");
        codegen_emit(gen, set_false);
        codegen_emit(gen, "\n");
    }

    codegen_emit(gen, set_true);
    codegen_emit(gen, ":\n  ld a, 1\n  jr ");
    codegen_emit(gen, done);
    codegen_emit(gen, "\n");

    codegen_emit(gen, set_false);
    codegen_emit(gen, ":\n  ld a, 0\n");
    codegen_emit(gen, done);
    codegen_emit(gen, ":\n");
}

static cc_error_t codegen_expression(codegen_t* gen, ast_node_t* node) {
    size_t size = codegen_expr_size(gen, node);
    if (size == 4) {
        return codegen_expression_u32(gen, node, NULL, NULL);
    }
    return codegen_expression_u8(gen, node);
}

static cc_error_t codegen_expression_u8(codegen_t* gen, ast_node_t* node) {
    if (!node) return CC_ERROR_INTERNAL;

    switch (node->type) {
        case AST_CONSTANT: {
            int32_t val = node->data.constant.int_value;
            int value = (int)(val & 0xFF);
            codegen_emit(gen, "  ld a, ");
            codegen_emit_int(gen, value);
            codegen_emit(gen, "\n");
            return CC_OK;
        }

        case AST_IDENTIFIER: {
            size_t size = 1;
            size_t local_size = 0;
            int offset = 0;
            size_t param_size = 0;
            if (codegen_lookup_local(gen, node->data.identifier.name, &local_size)) {
                size = local_size;
            } else if (codegen_lookup_param(gen, node->data.identifier.name, &offset, &param_size)) {
                size = param_size;
            }

            if (param_size > 0) {
                codegen_emit(gen, "  ld a, (ix+");
                codegen_emit_int(gen, offset);
                codegen_emit(gen, ")  ; Load param: ");
            } else {
                codegen_emit(gen, "  ld a, (");
                codegen_emit_mangled_var(gen, node->data.identifier.name);
                codegen_emit(gen, ")  ; Load variable: ");
            }
            codegen_emit(gen, node->data.identifier.name);
            if (size == 4) {
                codegen_emit(gen, " (low byte)");
            }
            codegen_emit(gen, "\n");
            return CC_OK;
        }

        case AST_BINARY_OP: {
            if (codegen_is_comparison(node->data.binary_op.op)) {
                size_t left_size = codegen_expr_size(gen, node->data.binary_op.left);
                size_t right_size = codegen_expr_size(gen, node->data.binary_op.right);
                if (left_size == 4 || right_size == 4) {
                    const char* left_addr = NULL;
                    const char* right_addr = NULL;
                    bool left_is_var = false;
                    bool right_is_var = false;
                    cc_error_t err = codegen_expression_u32(gen, node->data.binary_op.left, NULL, &left_addr);
                    if (err != CC_OK) return err;
                    err = codegen_expression_u32(gen, node->data.binary_op.right, NULL, &right_addr);
                    if (err != CC_OK) return err;
                    codegen_emit_cmp32(gen, node->data.binary_op.op, left_addr, left_is_var,
                        right_addr, right_is_var);
                    return CC_OK;
                }
            }

            cc_error_t err = codegen_expression_u8(gen, node->data.binary_op.left);
            if (err != CC_OK) return err;
            codegen_emit(gen, "  push af\n");
            err = codegen_expression_u8(gen, node->data.binary_op.right);
            if (err != CC_OK) return err;
            codegen_emit(gen,
                "  ld l, a\n"
                "  pop af\n");

            switch (node->data.binary_op.op) {
                case OP_ADD:
                    codegen_emit(gen, "  add a, l\n");
                    break;
                case OP_SUB:
                    codegen_emit(gen, "  sub l\n");
                    break;
                case OP_MUL:
                    codegen_emit(gen,
                        "; Multiplication (A * L)\n"
                        "  call __mul_a_l\n");
                    break;
                case OP_DIV:
                    codegen_emit(gen,
                        "; Division (A / L)\n"
                        "  call __div_a_l\n");
                    break;
                case OP_MOD:
                    codegen_emit(gen,
                        "; Modulo (A % L)\n"
                        "  call __mod_a_l\n");
                    break;
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
            int idx = codegen_find_function(gen, node->data.call.name);
            size_t expected_args = node->data.call.arg_count;
            const size_t* param_sizes = NULL;
            if (idx >= 0) {
                param_sizes = gen->func_param_sizes[idx];
            }

            codegen_emit(gen, "; Call function: ");
            codegen_emit(gen, node->data.call.name);
            codegen_emit(gen, "\n");
            if (node->data.call.arg_count > 0) {
                for (size_t i = node->data.call.arg_count; i-- > 0;) {
                    size_t arg_size = 1;
                    if (param_sizes && i < expected_args) {
                        arg_size = param_sizes[i];
                    } else {
                        arg_size = codegen_expr_size(gen, node->data.call.args[i]);
                    }
                    if (arg_size == 4) {
                        const char* temp = NULL;
                        cc_error_t err = codegen_expression_u32(gen, node->data.call.args[i], NULL, &temp);
                        if (err != CC_OK) return err;
                        codegen_emit(gen, "  ld hl, (");
                        codegen_emit_addr_offset(gen, temp, false, 0);
                        codegen_emit(gen, ")\n  ld de, (");
                        codegen_emit_addr_offset(gen, temp, false, 2);
                        codegen_emit(gen, "\n  push de\n  push hl\n");
                    } else {
                        cc_error_t err = codegen_expression_u8(gen, node->data.call.args[i]);
                        if (err != CC_OK) return err;
                        codegen_emit(gen,
                            "  ld l, a\n"
                            "  ld h, 0\n"
                            "  push hl\n");
                    }
                }
            }
            codegen_emit(gen, "  call ");
            codegen_emit(gen, node->data.call.name);
            codegen_emit(gen, "\n");
            if (idx >= 0 && gen->func_return_sizes[idx] == 4) {
                codegen_emit(gen, "  ld a, l\n");
            }
            if (node->data.call.arg_count > 0) {
                for (size_t i = 0; i < node->data.call.arg_count; i++) {
                    size_t arg_size = 2;
                    if (param_sizes && i < expected_args) {
                        arg_size = param_sizes[i] == 4 ? 4 : 2;
                    } else {
                        arg_size = codegen_expr_size(gen, node->data.call.args[i]) == 4 ? 4 : 2;
                    }
                    if (arg_size == 4) {
                        codegen_emit(gen, "  pop bc\n  pop bc\n");
                    } else {
                        codegen_emit(gen, "  pop bc\n");
                    }
                }
            }
            return CC_OK;
        }

        case AST_ASSIGN: {
            size_t size = codegen_expr_size(gen, node);
            if (size == 4) {
                const char* temp = NULL;
                cc_error_t err = codegen_expression_u32(gen, node->data.assign.rvalue, NULL, &temp);
                if (err != CC_OK) return err;
                if (node->data.assign.lvalue->type == AST_IDENTIFIER) {
                    int offset = 0;
                    size_t param_size = 0;
                    if (codegen_lookup_param(gen, node->data.assign.lvalue->data.identifier.name,
                            &offset, &param_size)) {
                        for (int i = 0; i < 4; i++) {
                            codegen_emit(gen, "  ld a, (");
                            codegen_emit_addr_offset(gen, temp, false, i);
                            codegen_emit(gen, ")\n  ld (ix+");
                            codegen_emit_int(gen, offset + i);
                            codegen_emit(gen, "), a\n");
                        }
                    } else {
                        for (int i = 0; i < 4; i++) {
                            codegen_emit(gen, "  ld a, (");
                            codegen_emit_addr_offset(gen, temp, false, i);
                            codegen_emit(gen, ")\n  ld (");
                            codegen_emit_mangled_var(gen, node->data.assign.lvalue->data.identifier.name);
                            codegen_emit(gen, "+");
                            codegen_emit_int(gen, i);
                            codegen_emit(gen, "), a\n");
                        }
                    }
                }
                return CC_OK;
            }

            cc_error_t err = codegen_expression_u8(gen, node->data.assign.rvalue);
            if (err != CC_OK) return err;
            if (node->data.assign.lvalue->type == AST_IDENTIFIER) {
                int offset = 0;
                size_t param_size = 0;
                if (codegen_lookup_param(gen, node->data.assign.lvalue->data.identifier.name,
                        &offset, &param_size)) {
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

static cc_error_t codegen_expression_u32(codegen_t* gen, ast_node_t* node, const char* dest,
    const char** out_addr) {
    if (!node) return CC_ERROR_INTERNAL;

    const char* out = dest ? dest : codegen_record_temp(gen, 4);
    if (!out) return CC_ERROR_CODEGEN;
    if (out_addr) {
        *out_addr = out;
    }

    switch (node->type) {
        case AST_CONSTANT:
            codegen_emit_store_u32_immediate(gen, out, node->data.constant.int_value);
            return CC_OK;
        case AST_IDENTIFIER: {
            size_t local_size = 0;
            int offset = 0;
            size_t param_size = 0;
            if (codegen_lookup_param(gen, node->data.identifier.name, &offset, &param_size)) {
                if (param_size == 4) {
                    codegen_emit_copy_u32_from_ix(gen, out, offset);
                } else {
                    cc_error_t err = codegen_expression_u8(gen, node);
                    if (err != CC_OK) return err;
                    codegen_emit_store_u32_zero_extend_a(gen, out);
                }
                return CC_OK;
            }
            if (codegen_lookup_local(gen, node->data.identifier.name, &local_size) && local_size == 4) {
                if (dest && codegen_names_equal(dest, node->data.identifier.name)) {
                    return CC_OK;
                }
                codegen_emit(gen, "  ld hl, ");
                codegen_emit_mangled_var(gen, node->data.identifier.name);
                codegen_emit(gen, "\n  ld de, ");
                codegen_emit_addr(gen, out, false);
                codegen_emit(gen, "\n  ld bc, 4\n  ldir\n");
                return CC_OK;
            }
            {
                cc_error_t err = codegen_expression_u8(gen, node);
                if (err != CC_OK) return err;
                codegen_emit_store_u32_zero_extend_a(gen, out);
            }
            return CC_OK;
        }
        case AST_BINARY_OP: {
            if (codegen_is_comparison(node->data.binary_op.op)) {
                cc_error_t err = codegen_expression_u8(gen, node);
                if (err != CC_OK) return err;
                codegen_emit_store_u32_zero_extend_a(gen, out);
                return CC_OK;
            }
            const char* left = NULL;
            const char* right = NULL;
            cc_error_t err = codegen_expression_u32(gen, node->data.binary_op.left, NULL, &left);
            if (err != CC_OK) return err;
            err = codegen_expression_u32(gen, node->data.binary_op.right, NULL, &right);
            if (err != CC_OK) return err;
            switch (node->data.binary_op.op) {
                case OP_ADD:
                    codegen_emit_call_u32_op(gen, "__add32", out, false, left, false, right, false);
                    break;
                case OP_SUB:
                    codegen_emit_call_u32_op(gen, "__sub32", out, false, left, false, right, false);
                    break;
                case OP_MUL:
                    codegen_emit_call_u32_op(gen, "__mul32", out, false, left, false, right, false);
                    break;
                case OP_DIV:
                    codegen_emit_call_u32_op(gen, "__div32", out, false, left, false, right, false);
                    break;
                case OP_MOD:
                    codegen_emit_call_u32_op(gen, "__mod32", out, false, left, false, right, false);
                    break;
                default:
                    return CC_ERROR_CODEGEN;
            }
            return CC_OK;
        }
        case AST_ASSIGN: {
            cc_error_t err = codegen_expression_u32(gen, node->data.assign.rvalue, out, NULL);
            if (err != CC_OK) return err;
            if (node->data.assign.lvalue->type == AST_IDENTIFIER) {
                int offset = 0;
                size_t param_size = 0;
                if (codegen_lookup_param(gen, node->data.assign.lvalue->data.identifier.name,
                        &offset, &param_size)) {
                    for (int i = 0; i < 4; i++) {
                        codegen_emit(gen, "  ld a, (");
                        codegen_emit_addr_offset(gen, out, false, i);
                        codegen_emit(gen, ")\n  ld (ix+");
                        codegen_emit_int(gen, offset + i);
                        codegen_emit(gen, "), a\n");
                    }
                } else {
                    for (int i = 0; i < 4; i++) {
                        codegen_emit(gen, "  ld a, (");
                        codegen_emit_addr_offset(gen, out, false, i);
                        codegen_emit(gen, ")\n  ld (");
                        codegen_emit_mangled_var(gen, node->data.assign.lvalue->data.identifier.name);
                        codegen_emit(gen, "+");
                        codegen_emit_int(gen, i);
                        codegen_emit(gen, "), a\n");
                    }
                }
            }
            return CC_OK;
        }
        case AST_CALL: {
            size_t ret_size = codegen_expr_size(gen, node);
            if (ret_size == 4) {
                cc_error_t err = codegen_expression_u8(gen, node);
                if (err != CC_OK) return err;
                codegen_emit(gen, "  ld (");
                codegen_emit_addr_offset(gen, out, false, 0);
                codegen_emit(gen, "), l\n  ld (");
                codegen_emit_addr_offset(gen, out, false, 1);
                codegen_emit(gen, "), h\n  ld (");
                codegen_emit_addr_offset(gen, out, false, 2);
                codegen_emit(gen, "), e\n  ld (");
                codegen_emit_addr_offset(gen, out, false, 3);
                codegen_emit(gen, "), d\n");
                return CC_OK;
            }
            {
                cc_error_t err = codegen_expression_u8(gen, node);
                if (err != CC_OK) return err;
                codegen_emit_store_u32_zero_extend_a(gen, out);
                return CC_OK;
            }
        }
        default:
            {
                cc_error_t err = codegen_expression_u8(gen, node);
                if (err != CC_OK) return err;
                codegen_emit_store_u32_zero_extend_a(gen, out);
                return CC_OK;
            }
    }
}

static cc_error_t codegen_statement(codegen_t* gen, ast_node_t* node) {
    if (!node) return CC_OK;

    switch (node->type) {
        case AST_RETURN_STMT:
            if (gen->current_return_size == 4) {
                if (node->data.return_stmt.expr) {
                    const char* temp = NULL;
                    cc_error_t err = codegen_expression_u32(gen, node->data.return_stmt.expr, NULL, &temp);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, "  ld hl, (");
                    codegen_emit_addr_offset(gen, temp, false, 0);
                    codegen_emit(gen, ")\n  ld de, (");
                    codegen_emit_addr_offset(gen, temp, false, 2);
                    codegen_emit(gen, ")\n  ld a, l\n");
                } else {
                    codegen_emit(gen,
                        "  ld hl, 0\n"
                        "  ld de, 0\n"
                        "  ld a, 0\n");
                }
            } else {
                if (node->data.return_stmt.expr) {
                    cc_error_t err = codegen_expression_u8(gen, node->data.return_stmt.expr);
                    if (err != CC_OK) return err;
                } else {
                    codegen_emit(gen, "  ld a, 0\n");
                }
            }
            if (gen->return_direct || !gen->function_end_label) {
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
            if (gen->defer_var_storage) {
                size_t size = codegen_type_storage_size(node->data.var_decl.var_type);
                codegen_record_local(gen, node->data.var_decl.name, size);
            } else {
                size_t size = codegen_type_storage_size(node->data.var_decl.var_type);
                codegen_emit_mangled_var(gen, node->data.var_decl.name);
                if (size == 4) {
                    codegen_emit(gen,
                        ":\n"
                        "  .db 0, 0, 0, 0\n");
                } else {
                    codegen_emit(gen,
                        ":\n"
                        "  .db 0\n");
                }
            }

            /* If there's an initializer, generate assignment */
            if (node->data.var_decl.initializer) {
                size_t size = codegen_type_storage_size(node->data.var_decl.var_type);
                if (size == 4) {
                    const char* temp = NULL;
                    cc_error_t err = codegen_expression_u32(gen, node->data.var_decl.initializer, NULL, &temp);
                    if (err != CC_OK) return err;
                    for (int i = 0; i < 4; i++) {
                        codegen_emit(gen, "  ld a, (");
                        codegen_emit_addr_offset(gen, temp, false, i);
                        codegen_emit(gen, ")\n  ld (");
                        codegen_emit_mangled_var(gen, node->data.var_decl.name);
                        codegen_emit(gen, "+");
                        codegen_emit_int(gen, i);
                        codegen_emit(gen, "), a\n");
                    }
                } else {
                    cc_error_t err = codegen_expression_u8(gen, node->data.var_decl.initializer);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, "  ld (");
                    codegen_emit_mangled_var(gen, node->data.var_decl.name);
                    codegen_emit(gen, "), a\n");
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
            const char* cond_temp = NULL;
            size_t cond_size = codegen_expr_size(gen, node->data.if_stmt.condition);
            cc_error_t err;
            if (cond_size == 4) {
                err = codegen_expression_u32(gen, node->data.if_stmt.condition, NULL, &cond_temp);
            } else {
                err = codegen_expression_u8(gen, node->data.if_stmt.condition);
            }
            if (err != CC_OK) return err;

            if (node->data.if_stmt.else_branch) {
                /* if-else: jump to else on false, fall through to then */
                char* else_label = codegen_new_label(gen);
                char* end_label = codegen_new_label(gen);

                if (cond_size == 4) {
                    codegen_emit(gen, "  ld a, (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 0);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 1);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 2);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 3);
                    codegen_emit(gen, ")\n  jp z, ");
                } else {
                    codegen_emit(gen, "  or a\n  jp z, ");
                }
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

                if (cond_size == 4) {
                    codegen_emit(gen, "  ld a, (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 0);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 1);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 2);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 3);
                    codegen_emit(gen, ")\n  jp z, ");
                } else {
                    codegen_emit(gen, "  or a\n  jp z, ");
                }
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
            const char* cond_temp = NULL;
            size_t cond_size = codegen_expr_size(gen, node->data.while_stmt.condition);
            cc_error_t err;
            if (cond_size == 4) {
                err = codegen_expression_u32(gen, node->data.while_stmt.condition, NULL, &cond_temp);
            } else {
                err = codegen_expression_u8(gen, node->data.while_stmt.condition);
            }
            if (err != CC_OK) {
                cc_free(loop_label);
                cc_free(end_label);
                return err;
            }

            /* Test condition (A register) */
            if (cond_size == 4) {
                codegen_emit(gen, "  ld a, (");
                codegen_emit_addr_offset(gen, cond_temp, false, 0);
                codegen_emit(gen, ")\n  or (");
                codegen_emit_addr_offset(gen, cond_temp, false, 1);
                codegen_emit(gen, ")\n  or (");
                codegen_emit_addr_offset(gen, cond_temp, false, 2);
                codegen_emit(gen, ")\n  or (");
                codegen_emit_addr_offset(gen, cond_temp, false, 3);
                codegen_emit(gen, ")\n  jp z,");
            } else {
                codegen_emit(gen, "  or a\n  jp z,");
            }
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
                const char* cond_temp = NULL;
                size_t cond_size = codegen_expr_size(gen, node->data.for_stmt.condition);
                if (cond_size == 4) {
                    err = codegen_expression_u32(gen, node->data.for_stmt.condition, NULL, &cond_temp);
                } else {
                    err = codegen_expression_u8(gen, node->data.for_stmt.condition);
                }
                if (err != CC_OK) {
                    cc_free(loop_label);
                    cc_free(end_label);
                    return err;
                }

                /* Test condition */
                if (cond_size == 4) {
                    codegen_emit(gen, "  ld a, (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 0);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 1);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 2);
                    codegen_emit(gen, ")\n  or (");
                    codegen_emit_addr_offset(gen, cond_temp, false, 3);
                    codegen_emit(gen, ")\n  jp z,");
                } else {
                    codegen_emit(gen, "  or a\n  jp z,");
                }
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
    gen->defer_var_storage = true;
    gen->param_count = 0;
    gen->temp_count = 0;
    gen->function_end_label = NULL;
    gen->return_direct = false;
    gen->use_function_end_label = false;

    codegen_emit(gen, node->data.function.name);
    codegen_emit(gen, ":\n");
    gen->current_return_size = codegen_type_storage_size(node->data.function.return_type);

    if (node->data.function.param_count > 0) {
        int offset = 4;
        for (size_t i = 0; i < node->data.function.param_count && i < 8; i++) {
            ast_node_t* param = node->data.function.params[i];
            if (!param || param->type != AST_VAR_DECL) continue;
            size_t size = codegen_type_storage_size(param->data.var_decl.var_type);
            int stack_size = (size == 4) ? 4 : 2;
            gen->param_names[gen->param_count] = param->data.var_decl.name;
            gen->param_sizes[gen->param_count] = size;
            gen->param_offsets[gen->param_count] = offset;
            gen->param_count++;
            offset += stack_size;
        }
    }

    gen->function_end_label = codegen_new_label_persist(gen);
    codegen_emit(gen,
        "  push ix\n"
        "  ld ix, 0\n"
        "  add ix, sp\n");

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
        codegen_emit(gen,
            "  pop ix\n"
            "  ret\n");
    } else if (!last_was_return) {
        if (gen->current_return_size == 4) {
            codegen_emit(gen,
                "  ld hl, 0\n"
                "  ld de, 0\n"
                "  ld a, 0\n");
        } else {
            codegen_emit(gen, "  ld a, 0\n");
        }
        codegen_emit(gen,
            "  pop ix\n"
            "  ret\n");
    }

    /* Local storage (kept out of instruction stream) */
    for (size_t i = 0; i < gen->local_var_count; i++) {
        const char* name = gen->local_var_names[i];
        size_t size = gen->local_var_sizes[i];
        if (!name) continue;
        codegen_emit(gen, "; Variable: ");
        codegen_emit(gen, name);
        codegen_emit(gen, "\n");
        codegen_emit_mangled_var(gen, name);
        if (size == 4) {
            codegen_emit(gen,
                ":\n"
                "  .db 0, 0, 0, 0\n");
        } else {
            codegen_emit(gen,
                ":\n"
                "  .db 0\n");
        }
    }

    for (size_t i = 0; i < gen->temp_count; i++) {
        const char* name = gen->temp_names[i];
        size_t size = gen->temp_sizes[i];
        if (!name) continue;
        codegen_emit(gen, "; Temp: ");
        codegen_emit(gen, name);
        codegen_emit(gen, "\n");
        codegen_emit(gen, name);
        if (size == 4) {
            codegen_emit(gen,
                ":\n"
                "  .db 0, 0, 0, 0\n");
        } else {
            codegen_emit(gen,
                ":\n"
                "  .db 0\n");
        }
    }

    gen->defer_var_storage = false;
    if (gen->function_end_label) {
        cc_free(gen->function_end_label);
        gen->function_end_label = NULL;
    }
    for (size_t i = 0; i < gen->temp_count; i++) {
        if (gen->temp_names[i]) {
            cc_free((void*)gen->temp_names[i]);
            gen->temp_names[i] = NULL;
        }
    }
    gen->temp_count = 0;
    codegen_emit(gen, "\n");

    return CC_OK;
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

    codegen_emit(gen,
        "\n; 32-bit helpers\n"
        "__add32:\n"
        "  or a\n"
        "  ld a, (hl)\n"
        "  add a, (de)\n"
        "  ld (bc), a\n"
        "  inc hl\n"
        "  inc de\n"
        "  inc bc\n"
        "  ld a, (hl)\n"
        "  adc a, (de)\n"
        "  ld (bc), a\n"
        "  inc hl\n"
        "  inc de\n"
        "  inc bc\n"
        "  ld a, (hl)\n"
        "  adc a, (de)\n"
        "  ld (bc), a\n"
        "  inc hl\n"
        "  inc de\n"
        "  inc bc\n"
        "  ld a, (hl)\n"
        "  adc a, (de)\n"
        "  ld (bc), a\n"
        "  ret\n"
        "\n"
        "__sub32:\n"
        "  or a\n"
        "  ld a, (hl)\n"
        "  sub (de)\n"
        "  ld (bc), a\n"
        "  inc hl\n"
        "  inc de\n"
        "  inc bc\n"
        "  ld a, (hl)\n"
        "  sbc a, (de)\n"
        "  ld (bc), a\n"
        "  inc hl\n"
        "  inc de\n"
        "  inc bc\n"
        "  ld a, (hl)\n"
        "  sbc a, (de)\n"
        "  ld (bc), a\n"
        "  inc hl\n"
        "  inc de\n"
        "  inc bc\n"
        "  ld a, (hl)\n"
        "  sbc a, (de)\n"
        "  ld (bc), a\n"
        "  ret\n"
        "\n"
        "__mul32:\n"
        "  push bc\n"
        "  push de\n"
        "  push hl\n"
        "  ld de, __mul32_left\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  pop hl\n"
        "  pop de\n"
        "  ex de, hl\n"
        "  ld de, __mul32_right\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ld hl, __mul32_accum\n"
        "  ld (hl), 0\n"
        "  inc hl\n"
        "  ld (hl), 0\n"
        "  inc hl\n"
        "  ld (hl), 0\n"
        "  inc hl\n"
        "  ld (hl), 0\n"
        "  ld b, 32\n"
        "__mul32_loop:\n"
        "  ld a, (__mul32_right)\n"
        "  and 1\n"
        "  jr z, __mul32_skip_add\n"
        "  ld hl, __mul32_accum\n"
        "  ld de, __mul32_left\n"
        "  ld bc, __mul32_accum\n"
        "  call __add32\n"
        "__mul32_skip_add:\n"
        "  xor a\n"
        "  ld hl, __mul32_left\n"
        "  rl (hl)\n"
        "  inc hl\n"
        "  rl (hl)\n"
        "  inc hl\n"
        "  rl (hl)\n"
        "  inc hl\n"
        "  rl (hl)\n"
        "  xor a\n"
        "  ld hl, __mul32_right+3\n"
        "  rr (hl)\n"
        "  dec hl\n"
        "  rr (hl)\n"
        "  dec hl\n"
        "  rr (hl)\n"
        "  dec hl\n"
        "  rr (hl)\n"
        "  djnz __mul32_loop\n"
        "  pop bc\n"
        "  ld d, b\n"
        "  ld e, c\n"
        "  ld hl, __mul32_accum\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ret\n"
        "\n"
        "__div32:\n"
        "  push bc\n"
        "  push de\n"
        "  ld de, __div32_rem\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  pop de\n"
        "  ex de, hl\n"
        "  ld de, __div32_div\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ld hl, __div32_quot\n"
        "  ld (hl), 0\n"
        "  inc hl\n"
        "  ld (hl), 0\n"
        "  inc hl\n"
        "  ld (hl), 0\n"
        "  inc hl\n"
        "  ld (hl), 0\n"
        "  ld a, (__div32_div)\n"
        "  or (__div32_div+1)\n"
        "  or (__div32_div+2)\n"
        "  or (__div32_div+3)\n"
        "  jr nz, __div32_loop\n"
        "  pop bc\n"
        "  ld hl, __div32_quot\n"
        "  ld d, b\n"
        "  ld e, c\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ret\n"
        "__div32_loop:\n"
        "  ld hl, __div32_rem+3\n"
        "  ld de, __div32_div+3\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __div32_done\n"
        "  jr nz, __div32_do_sub\n"
        "  dec hl\n"
        "  dec de\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __div32_done\n"
        "  jr nz, __div32_do_sub\n"
        "  dec hl\n"
        "  dec de\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __div32_done\n"
        "  jr nz, __div32_do_sub\n"
        "  dec hl\n"
        "  dec de\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __div32_done\n"
        "__div32_do_sub:\n"
        "  ld hl, __div32_rem\n"
        "  ld de, __div32_div\n"
        "  ld bc, __div32_rem\n"
        "  call __sub32\n"
        "  ld hl, __div32_quot\n"
        "  ld a, (hl)\n"
        "  inc a\n"
        "  ld (hl), a\n"
        "  jr nz, __div32_loop\n"
        "  inc hl\n"
        "  ld a, (hl)\n"
        "  inc a\n"
        "  ld (hl), a\n"
        "  jr nz, __div32_loop\n"
        "  inc hl\n"
        "  ld a, (hl)\n"
        "  inc a\n"
        "  ld (hl), a\n"
        "  jr nz, __div32_loop\n"
        "  inc hl\n"
        "  ld a, (hl)\n"
        "  inc a\n"
        "  ld (hl), a\n"
        "  jr __div32_loop\n"
        "__div32_done:\n"
        "  pop bc\n"
        "  ld hl, __div32_quot\n"
        "  ld d, b\n"
        "  ld e, c\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ret\n"
        "\n"
        "__mod32:\n"
        "  push bc\n"
        "  push de\n"
        "  ld de, __mod32_rem\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  pop de\n"
        "  ex de, hl\n"
        "  ld de, __mod32_div\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ld a, (__mod32_div)\n"
        "  or (__mod32_div+1)\n"
        "  or (__mod32_div+2)\n"
        "  or (__mod32_div+3)\n"
        "  jr nz, __mod32_loop\n"
        "  pop bc\n"
        "  ld hl, __mod32_rem\n"
        "  ld d, b\n"
        "  ld e, c\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ret\n"
        "__mod32_loop:\n"
        "  ld hl, __mod32_rem+3\n"
        "  ld de, __mod32_div+3\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __mod32_done\n"
        "  jr nz, __mod32_do_sub\n"
        "  dec hl\n"
        "  dec de\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __mod32_done\n"
        "  jr nz, __mod32_do_sub\n"
        "  dec hl\n"
        "  dec de\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __mod32_done\n"
        "  jr nz, __mod32_do_sub\n"
        "  dec hl\n"
        "  dec de\n"
        "  ld a, (hl)\n"
        "  cp (de)\n"
        "  jr c, __mod32_done\n"
        "__mod32_do_sub:\n"
        "  ld hl, __mod32_rem\n"
        "  ld de, __mod32_div\n"
        "  ld bc, __mod32_rem\n"
        "  call __sub32\n"
        "  jr __mod32_loop\n"
        "__mod32_done:\n"
        "  pop bc\n"
        "  ld hl, __mod32_rem\n"
        "  ld d, b\n"
        "  ld e, c\n"
        "  ld bc, 4\n"
        "  ldir\n"
        "  ret\n"
        "\n"
        "__mul32_left:\n"
        "  .db 0, 0, 0, 0\n"
        "__mul32_right:\n"
        "  .db 0, 0, 0, 0\n"
        "__mul32_accum:\n"
        "  .db 0, 0, 0, 0\n"
        "__div32_rem:\n"
        "  .db 0, 0, 0, 0\n"
        "__div32_div:\n"
        "  .db 0, 0, 0, 0\n"
        "__div32_quot:\n"
        "  .db 0, 0, 0, 0\n"
        "__mod32_rem:\n"
        "  .db 0, 0, 0, 0\n"
        "__mod32_div:\n"
        "  .db 0, 0, 0, 0\n"
    );
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
    codegen_collect_function_signatures(gen, ast);

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

        /* Emit runtime library */
        codegen_emit_runtime(gen);

        return CC_OK;
    } else if (ast->type == AST_FUNCTION) {
        /* Direct function node */
        cc_error_t err = codegen_function(gen, ast);
        if (err != CC_OK) return err;

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
