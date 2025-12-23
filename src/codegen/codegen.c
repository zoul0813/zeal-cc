#include "codegen.h"

#include "ast_format.h"
#include "ast_io.h"
#include "ast_reader.h"
#include "common.h"
#include "codegen_strings.h"
#include "target.h"
#include "cc_compat.h"

#ifdef __SDCC
#include <core.h>
#else
#include <stdarg.h>
#include <stdio.h>
#endif

#define INITIAL_OUTPUT_CAPACITY 1024

/* Helpers */
static void codegen_emit_ix_offset(codegen_t* gen, int16_t offset);
static void codegen_emit_mangled_var(codegen_t* gen, const char* name) {
    codegen_emit(gen, "_v_");
    codegen_emit(gen, name);
}

static bool codegen_names_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static void codegen_emit_int(codegen_t* gen, int16_t value) {
    char buf[16];
    uint16_t i = 0;
    if (value == 0) {
        buf[i++] = '0';
    } else {
        if (value < 0) {
            buf[i++] = '-';
            value = -value;
        }
        char temp[16];
        uint16_t j = 0;
        while (value > 0 && j < (uint16_t)sizeof(temp)) {
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

static cc_error_t codegen_emit_file(codegen_t* gen, const char* path) {
    if (!gen || !gen->output_handle || !path) return CC_ERROR_INVALID_ARG;
    reader_t* reader = reader_open(path);
    if (!reader) {
        cc_error("Failed to open runtime file");
        return CC_ERROR_FILE_NOT_FOUND;
    }
    int16_t ch = reader_next(reader);
    while (ch >= 0) {
        char c = (char)ch;
        output_write(gen->output_handle, &c, 1);
        ch = reader_next(reader);
    }
    reader_close(reader);
    return CC_OK;
}

static void codegen_emit_stack_adjust(codegen_t* gen, int16_t offset, bool subtract) {
    if (!gen || offset <= 0) return;
    codegen_emit(gen, CG_STR_LD_HL_ZERO);
    codegen_emit(gen, CG_STR_ADD_HL_SP);
    codegen_emit(gen, CG_STR_LD_DE);
    codegen_emit_int(gen, offset);
    codegen_emit(gen, CG_STR_NL);
    if (subtract) {
        codegen_emit(gen, CG_STR_OR_A_SBC_HL_DE);
    } else {
        codegen_emit(gen, CG_STR_ADD_HL_DE);
    }
    codegen_emit(gen, CG_STR_LD_SP_HL);
}

static void codegen_emit_label(codegen_t* gen, const char* label) {
    if (!gen || !label) return;
    codegen_emit(gen, label);
    codegen_emit(gen, CG_STR_COLON_NL);
}

static void codegen_emit_jump(codegen_t* gen, const char* prefix, const char* label) {
    if (!gen || !prefix || !label) return;
    codegen_emit(gen, prefix);
    codegen_emit(gen, label);
    codegen_emit(gen, CG_STR_NL);
}

static bool codegen_type_is_pointer(const type_t* type) {
    return type && type->kind == TYPE_POINTER;
}

static uint16_t codegen_type_storage_size(const type_t* type) {
    return codegen_type_is_pointer(type) ? 2u : 1u;
}

static const char* codegen_stream_string(ast_reader_t* ast, uint16_t index) {
    return ast_reader_string(ast, index);
}

static bool codegen_stream_type_is_pointer(uint8_t depth) {
    return depth > 0;
}

static uint16_t codegen_stream_type_storage_size(uint8_t depth) {
    return depth > 0 ? 2u : 1u;
}

static int8_t codegen_stream_read_identifier(ast_reader_t* ast, const char** name) {
    uint16_t index = 0;
    if (!name) return -1;
    if (ast_read_u16(ast->reader, &index) < 0) return -1;
    *name = codegen_stream_string(ast, index);
    return *name ? 0 : -1;
}

static int8_t codegen_stream_read_string(ast_reader_t* ast, const char** value) {
    uint16_t index = 0;
    if (!value) return -1;
    if (ast_read_u16(ast->reader, &index) < 0) return -1;
    *value = codegen_stream_string(ast, index);
    return *value ? 0 : -1;
}

static int16_t codegen_local_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (size_t i = 0; i < gen->local_var_count; i++) {
        if (gen->local_vars[i] == name || codegen_names_equal(gen->local_vars[i], name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static int16_t codegen_param_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (size_t i = 0; i < gen->param_count; i++) {
        if (gen->param_names[i] == name || codegen_names_equal(gen->param_names[i], name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static int16_t codegen_global_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (size_t i = 0; i < gen->global_count; i++) {
        if (gen->global_names[i] == name || codegen_names_equal(gen->global_names[i], name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static void codegen_record_local(codegen_t* gen, const char* name, uint16_t size, bool is_pointer) {
    if (!gen || !name) return;
    if (codegen_local_index(gen, name) >= 0) return;
    if (gen->local_var_count < (sizeof(gen->local_vars) / sizeof(gen->local_vars[0]))) {
        gen->local_vars[gen->local_var_count] = name;
        gen->local_offsets[gen->local_var_count] = gen->stack_offset;
        gen->local_sizes[gen->local_var_count] = size;
        gen->local_is_pointer[gen->local_var_count] = is_pointer;
        gen->stack_offset += size;
        gen->local_var_count++;
    }
}

static uint8_t codegen_param_offset(codegen_t* gen, const char* name, int16_t* out_offset) {
    if (!gen || !name || !out_offset) return 0;
    int16_t idx = codegen_param_index(gen, name);
    if (idx < 0) return 0;
    *out_offset = gen->param_offsets[idx];
    return 1;
}

static uint8_t codegen_local_offset(codegen_t* gen, const char* name, int16_t* out_offset) {
    if (!gen || !name || !out_offset) return 0;
    int16_t idx = codegen_local_index(gen, name);
    if (idx < 0) return 0;
    *out_offset = gen->local_offsets[idx];
    return 1;
}

static bool codegen_local_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && gen->local_is_pointer[idx];
}

static bool codegen_param_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_param_index(gen, name);
    return idx >= 0 && gen->param_is_pointer[idx];
}

static bool codegen_global_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && gen->global_is_pointer[idx];
}

static cc_error_t codegen_emit_address_of_identifier(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_offset(gen, name, &offset) || codegen_param_offset(gen, name, &offset)) {
        codegen_emit(gen, CG_STR_PUSH_IX_POP_HL);
        if (offset != 0) {
            codegen_emit(gen, CG_STR_LD_DE);
            codegen_emit_int(gen, offset);
            codegen_emit(gen, "\n  add hl, de\n");
        }
        return CC_OK;
    }

    codegen_emit(gen, CG_STR_LD_HL);
    codegen_emit_mangled_var(gen, name);
    codegen_emit(gen, "\n");
    return CC_OK;
}

static cc_error_t codegen_load_pointer_to_hl(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_offset(gen, name, &offset) || codegen_param_offset(gen, name, &offset)) {
        codegen_emit(gen, CG_STR_LD_L_PAREN);
        codegen_emit_ix_offset(gen, offset);
        codegen_emit(gen, CG_STR_RPAREN_NL);
        codegen_emit(gen, CG_STR_LD_H_PAREN);
        codegen_emit_ix_offset(gen, offset + 1);
        codegen_emit(gen, CG_STR_RPAREN_NL);
        return CC_OK;
    }

    codegen_emit(gen, CG_STR_LD_HL_PAREN);
    codegen_emit_mangled_var(gen, name);
    codegen_emit(gen, CG_STR_RPAREN_NL);
    return CC_OK;
}

static cc_error_t codegen_store_pointer_from_hl(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_offset(gen, name, &offset) || codegen_param_offset(gen, name, &offset)) {
        codegen_emit(gen, CG_STR_LD_LPAREN);
        codegen_emit_ix_offset(gen, offset);
        codegen_emit(gen, CG_STR_RPAREN_L);
        codegen_emit(gen, CG_STR_LD_LPAREN);
        codegen_emit_ix_offset(gen, offset + 1);
        codegen_emit(gen, CG_STR_RPAREN_H);
        return CC_OK;
    }

    codegen_emit(gen, CG_STR_LD_LPAREN);
    codegen_emit_mangled_var(gen, name);
    codegen_emit(gen, CG_STR_RPAREN_HL);
    return CC_OK;
}

static void codegen_emit_ix_offset(codegen_t* gen, int16_t offset) {
    (void)gen;
    codegen_emit(gen, CG_STR_IX_PLUS);
    codegen_emit_int(gen, offset);
}

static void codegen_collect_locals(codegen_t* gen, ast_node_t* node) {
    if (!gen || !node) return;
    if (node->type == AST_VAR_DECL) {
        uint16_t size = codegen_type_storage_size(node->data.var_decl.var_type);
        bool is_pointer = codegen_type_is_pointer(node->data.var_decl.var_type);
        codegen_record_local(gen, node->data.var_decl.name, size, is_pointer);
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
        if (gen->string_literals[i] && str_cmp(gen->string_literals[i], value) == 0) {
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
#ifdef __SDCC
    if (gen->output_handle < 0) {
#else
    if (!gen->output_handle) {
#endif
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
    gen->global_count = 0;
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
    static uint8_t slot = 0;
    char* label = labels[slot++ & 7];
    uint16_t n = gen->label_counter++;
    uint16_t i = 0;
    label[i++] = '_';
    label[i++] = 'l';
    if (n == 0) {
        label[i++] = '0';
    } else {
        char temp[8];
        uint16_t j = 0;
        while (n > 0 && j < (uint16_t)sizeof(temp)) {
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
    static uint8_t slot = 0;
    char* label = labels[slot++ & 7];
    uint16_t n = gen->string_counter++;
    uint16_t i = 0;
    label[i++] = '_';
    label[i++] = 's';
    if (n == 0) {
        label[i++] = '0';
    } else {
        char temp[8];
        uint16_t j = 0;
        while (n > 0 && j < (uint16_t)sizeof(temp)) {
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

/* Forward declarations */
static cc_error_t codegen_statement(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_expression(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_global_var(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_stream_expression_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag);
static cc_error_t codegen_stream_statement_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag);
static int8_t codegen_stream_collect_locals(codegen_t* gen, ast_reader_t* ast);
static cc_error_t codegen_stream_function(codegen_t* gen, ast_reader_t* ast);
static cc_error_t codegen_stream_global_var(codegen_t* gen, ast_reader_t* ast);

static uint32_t g_arg_offsets[16];
static const char* g_param_names[8];
static bool g_param_is_pointer[8];

static cc_error_t codegen_expression(codegen_t* gen, ast_node_t* node) {
    if (!node) return CC_ERROR_INTERNAL;

    switch (node->type) {
        case AST_CONSTANT:
            /* Load constant into A register */
            codegen_emit(gen, CG_STR_LD_A);
            {
                /* Convert int to string */
                int16_t val = node->data.constant.int_value;
                char buf[16];
                uint16_t i = 0;
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    uint8_t neg = 0;
                    if (val < 0) {
                        neg = 1;
                        val = -val;
                    }
                    char temp[16];
                    uint16_t j = 0;
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
                int16_t offset = 0;
                if (codegen_local_offset(gen, node->data.identifier.name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, ")  ; Load local: ");
                } else if (codegen_param_offset(gen, node->data.identifier.name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, ")  ; Load param: ");
                } else {
                    codegen_emit(gen, CG_STR_LD_A_LPAREN);
                    codegen_emit_mangled_var(gen, node->data.identifier.name);
                    codegen_emit(gen, ")  ; Load variable: ");
                }
                codegen_emit(gen, node->data.identifier.name);
                codegen_emit(gen, "\n");
            }
            return CC_OK;

        case AST_UNARY_OP: {
            if (node->data.unary_op.op == OP_DEREF) {
                ast_node_t* operand = node->data.unary_op.operand;
                if (operand && operand->type == AST_IDENTIFIER) {
                    cc_error_t err = codegen_load_pointer_to_hl(gen, operand->data.identifier.name);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, CG_STR_LD_A_HL);
                    return CC_OK;
                }
                cc_error("Unsupported dereference operand");
                return CC_ERROR_CODEGEN;
            }
            if (node->data.unary_op.op == OP_ADDR) {
                cc_error("Address-of used without pointer assignment");
                return CC_ERROR_CODEGEN;
            }
            return CC_ERROR_CODEGEN;
        }

        case AST_BINARY_OP: {
            /* Evaluate left operand (result in A) */
            cc_error_t err = codegen_expression(gen, node->data.binary_op.left);
            if (err != CC_OK) return err;

            /* For right operand, we need to save A and evaluate right */
            /* Push left result onto stack */
            codegen_emit(gen, CG_STR_PUSH_AF);

            /* Evaluate right operand (result in A) */
            err = codegen_expression(gen, node->data.binary_op.right);
            if (err != CC_OK) return err;

            /* Pop left operand into L (for arithmetic) or B (for comparison) */
            codegen_emit(gen, CG_STR_LD_L_A_POP_AF);

            /* Perform operation: A op L, result in A */
            switch (node->data.binary_op.op) {
                case OP_ADD:
                    codegen_emit(gen, CG_STR_ADD_A_L);
                    break;
                case OP_SUB:
                    codegen_emit(gen, CG_STR_SUB_L);
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
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_NZ, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_NE:
                    codegen_emit(gen,
                        "; Inequality test (A != L)\n"
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_Z, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_LT:
                    codegen_emit(gen,
                        "; Less than test (A < L)\n"
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_NC, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_GT:
                    codegen_emit(gen,
                        "; Greater than test (A > L)\n"
                        "  sub l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_Z, end);
                        codegen_emit_jump(gen, CG_STR_JR_C, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_LE:
                    codegen_emit(gen,
                        "; Less or equal test (A <= L)\n"
                        "  sub l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        char* set = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_Z, set);
                        codegen_emit_jump(gen, CG_STR_JR_C, set);
                        codegen_emit_jump(gen, CG_STR_JR, end);
                        codegen_emit_label(gen, set);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_GE:
                    codegen_emit(gen,
                        "; Greater or equal test (A >= L)\n"
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_C, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
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
                    codegen_emit(gen, CG_STR_LD_L_A_H_ZERO_PUSH_HL);
                }
            }
            codegen_emit(gen, CG_STR_CALL);
            codegen_emit(gen, node->data.call.name);
            codegen_emit(gen, CG_STR_NL);
            if (node->data.call.arg_count > 0) {
                for (size_t i = 0; i < node->data.call.arg_count; i++) {
                    codegen_emit(gen, CG_STR_POP_BC);
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
                int16_t offset = (int16_t)index->data.constant.int_value;
                codegen_emit(gen, CG_STR_LD_HL);
                codegen_emit(gen, label);
                codegen_emit(gen, CG_STR_NL);
                if (offset != 0) {
                    codegen_emit(gen, CG_STR_LD_DE);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, CG_STR_NL);
                    codegen_emit(gen, CG_STR_ADD_HL_DE);
                }
                codegen_emit(gen, CG_STR_LD_A_HL);
                return CC_OK;
            }
            if (base && base->type == AST_IDENTIFIER &&
                index && index->type == AST_CONSTANT) {
                const char* name = base->data.identifier.name;
                if (!codegen_local_is_pointer(gen, name) &&
                    !codegen_param_is_pointer(gen, name) &&
                    !codegen_global_is_pointer(gen, name)) {
                    cc_error("Unsupported array access");
                    return CC_ERROR_CODEGEN;
                }
                cc_error_t err = codegen_load_pointer_to_hl(gen, name);
                if (err != CC_OK) return err;
                int16_t offset = (int16_t)index->data.constant.int_value;
                if (offset != 0) {
                    codegen_emit(gen, CG_STR_LD_DE);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, CG_STR_NL);
                    codegen_emit(gen, CG_STR_ADD_HL_DE);
                }
                codegen_emit(gen, CG_STR_LD_A_HL);
                return CC_OK;
            }
            cc_error("Unsupported array access");
            return CC_ERROR_CODEGEN;
        }

        case AST_ASSIGN: {
            ast_node_t* lvalue = node->data.assign.lvalue;
            ast_node_t* rvalue = node->data.assign.rvalue;

            if (lvalue && lvalue->type == AST_UNARY_OP &&
                lvalue->data.unary_op.op == OP_DEREF) {
                cc_error_t err = codegen_expression(gen, rvalue);
                if (err != CC_OK) return err;
                ast_node_t* operand = lvalue->data.unary_op.operand;
                if (operand && operand->type == AST_IDENTIFIER) {
                    err = codegen_load_pointer_to_hl(gen, operand->data.identifier.name);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, CG_STR_LD_HL_A);
                    return CC_OK;
                }
                cc_error("Unsupported dereference assignment");
                return CC_ERROR_CODEGEN;
            }

            if (lvalue && lvalue->type == AST_IDENTIFIER &&
                (codegen_local_is_pointer(gen, lvalue->data.identifier.name) ||
                 codegen_param_is_pointer(gen, lvalue->data.identifier.name) ||
                 codegen_global_is_pointer(gen, lvalue->data.identifier.name))) {
                if (rvalue && rvalue->type == AST_STRING_LITERAL) {
                    const char* label = codegen_get_string_label(gen, rvalue->data.string_literal.value);
                    if (!label) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_LD_HL);
                    codegen_emit(gen, label);
                    codegen_emit(gen, CG_STR_NL);
                    return codegen_store_pointer_from_hl(gen, lvalue->data.identifier.name);
                }
                if (rvalue && rvalue->type == AST_UNARY_OP &&
                    rvalue->data.unary_op.op == OP_ADDR &&
                    rvalue->data.unary_op.operand &&
                    rvalue->data.unary_op.operand->type == AST_IDENTIFIER) {
                    cc_error_t err = codegen_emit_address_of_identifier(
                        gen,
                        rvalue->data.unary_op.operand->data.identifier.name);
                    if (err != CC_OK) return err;
                    return codegen_store_pointer_from_hl(gen, lvalue->data.identifier.name);
                }
                if (rvalue && rvalue->type == AST_IDENTIFIER &&
                    (codegen_local_is_pointer(gen, rvalue->data.identifier.name) ||
                     codegen_param_is_pointer(gen, rvalue->data.identifier.name) ||
                     codegen_global_is_pointer(gen, rvalue->data.identifier.name))) {
                    cc_error_t err = codegen_load_pointer_to_hl(gen, rvalue->data.identifier.name);
                    if (err != CC_OK) return err;
                    return codegen_store_pointer_from_hl(gen, lvalue->data.identifier.name);
                }
                if (rvalue && rvalue->type == AST_CONSTANT &&
                    rvalue->data.constant.int_value == 0) {
                    codegen_emit(gen, CG_STR_LD_HL_ZERO);
                    return codegen_store_pointer_from_hl(gen, lvalue->data.identifier.name);
                }
                cc_error("Unsupported pointer assignment");
                return CC_ERROR_CODEGEN;
            }

            /* Assignment: evaluate RHS, store to LHS */
            cc_error_t err = codegen_expression(gen, rvalue);
            if (err != CC_OK) return err;

            /* Store A to variable */
            if (lvalue && lvalue->type == AST_IDENTIFIER) {
                int16_t offset = 0;
                if (codegen_local_offset(gen, lvalue->data.identifier.name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_LPAREN);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                } else if (codegen_param_offset(gen, lvalue->data.identifier.name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                } else {
                    codegen_emit(gen, CG_STR_LD_LPAREN);
                    codegen_emit_mangled_var(gen, lvalue->data.identifier.name);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                }
            }
            return CC_OK;
        }

        default:
            return CC_ERROR_CODEGEN;
    }
}

static cc_error_t codegen_stream_expression_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    switch (tag) {
        case AST_TAG_CONSTANT: {
            int16_t value = 0;
            if (ast_read_i16(ast->reader, &value) < 0) return CC_ERROR_CODEGEN;
            codegen_emit(gen, CG_STR_LD_A);
            codegen_emit_int(gen, value);
            codegen_emit(gen, CG_STR_NL);
            return CC_OK;
        }
        case AST_TAG_IDENTIFIER: {
            const char* name = NULL;
            if (codegen_stream_read_identifier(ast, &name) < 0) return CC_ERROR_CODEGEN;
            int16_t offset = 0;
            if (codegen_local_offset(gen, name, &offset)) {
                codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                codegen_emit_int(gen, offset);
                codegen_emit(gen, ")  ; Load local: ");
            } else if (codegen_param_offset(gen, name, &offset)) {
                codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                codegen_emit_int(gen, offset);
                codegen_emit(gen, ")  ; Load param: ");
            } else {
                codegen_emit(gen, CG_STR_LD_A_LPAREN);
                codegen_emit_mangled_var(gen, name);
                codegen_emit(gen, ")  ; Load variable: ");
            }
            codegen_emit(gen, name);
            codegen_emit(gen, "\n");
            return CC_OK;
        }
        case AST_TAG_UNARY_OP: {
            uint8_t op = 0;
            uint8_t child_tag = 0;
            if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &child_tag) < 0) return CC_ERROR_CODEGEN;
            if (op == OP_DEREF) {
                if (child_tag == AST_TAG_IDENTIFIER) {
                    const char* name = NULL;
                    if (codegen_stream_read_identifier(ast, &name) < 0) return CC_ERROR_CODEGEN;
                    cc_error_t err = codegen_load_pointer_to_hl(gen, name);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, CG_STR_LD_A_HL);
                    return CC_OK;
                }
                ast_reader_skip_tag(ast, child_tag);
                cc_error("Unsupported dereference operand");
                return CC_ERROR_CODEGEN;
            }
            ast_reader_skip_tag(ast, child_tag);
            cc_error("Address-of used without pointer assignment");
            return CC_ERROR_CODEGEN;
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = 0;
            if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
            uint8_t left_tag = 0;
            if (ast_read_u8(ast->reader, &left_tag) < 0) return CC_ERROR_CODEGEN;
            cc_error_t err = codegen_stream_expression_tag(gen, ast, left_tag);
            if (err != CC_OK) return err;
            codegen_emit(gen, CG_STR_PUSH_AF);
            uint8_t right_tag = 0;
            if (ast_read_u8(ast->reader, &right_tag) < 0) return CC_ERROR_CODEGEN;
            err = codegen_stream_expression_tag(gen, ast, right_tag);
            if (err != CC_OK) return err;
            codegen_emit(gen, CG_STR_LD_L_A_POP_AF);
            switch (op) {
                case OP_ADD:
                    codegen_emit(gen, CG_STR_ADD_A_L);
                    break;
                case OP_SUB:
                    codegen_emit(gen, CG_STR_SUB_L);
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
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_NZ, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_NE:
                    codegen_emit(gen,
                        "; Inequality test (A != L)\n"
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_Z, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_LT:
                    codegen_emit(gen,
                        "; Less than test (A < L)\n"
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_NC, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_GT:
                    codegen_emit(gen,
                        "; Greater than test (A > L)\n"
                        "  sub l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_Z, end);
                        codegen_emit_jump(gen, CG_STR_JR_C, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_LE:
                    codegen_emit(gen,
                        "; Less or equal test (A <= L)\n"
                        "  sub l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        char* set = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_Z, set);
                        codegen_emit_jump(gen, CG_STR_JR_C, set);
                        codegen_emit_jump(gen, CG_STR_JR, end);
                        codegen_emit_label(gen, set);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                case OP_GE:
                    codegen_emit(gen,
                        "; Greater or equal test (A >= L)\n"
                        "  cp l\n");
                    codegen_emit(gen, CG_STR_LD_A_ZERO);
                    {
                        char* end = codegen_new_label(gen);
                        codegen_emit_jump(gen, CG_STR_JR_C, end);
                        codegen_emit(gen, CG_STR_LD_A_ONE);
                        codegen_emit_label(gen, end);
                    }
                    break;
                default:
                    return CC_ERROR_CODEGEN;
            }
            return CC_OK;
        }
        case AST_TAG_CALL: {
            uint16_t name_index = 0;
            uint8_t arg_count = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &arg_count) < 0) return CC_ERROR_CODEGEN;
            const char* name = codegen_stream_string(ast, name_index);
            if (!name) return CC_ERROR_CODEGEN;
            codegen_emit(gen, "; Call function: ");
            codegen_emit(gen, name);
            codegen_emit(gen, "\n");

            if (arg_count > 0) {
                if (arg_count > (uint8_t)(sizeof(g_arg_offsets) / sizeof(g_arg_offsets[0]))) {
                    for (uint8_t i = 0; i < arg_count; i++) {
                        if (ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
                    }
                    return CC_ERROR_CODEGEN;
                }
                for (uint8_t i = 0; i < arg_count; i++) {
                    g_arg_offsets[i] = reader_tell(ast->reader);
                    if (ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
                }
                uint32_t end_pos = reader_tell(ast->reader);
                for (uint8_t i = arg_count; i-- > 0;) {
                    if (reader_seek(ast->reader, g_arg_offsets[i]) < 0) return CC_ERROR_CODEGEN;
                    uint8_t arg_tag = 0;
                    if (ast_read_u8(ast->reader, &arg_tag) < 0) return CC_ERROR_CODEGEN;
                    cc_error_t err = codegen_stream_expression_tag(gen, ast, arg_tag);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, CG_STR_LD_L_A_H_ZERO_PUSH_HL);
                }
                if (reader_seek(ast->reader, end_pos) < 0) return CC_ERROR_CODEGEN;
            }

            codegen_emit(gen, CG_STR_CALL);
            codegen_emit(gen, name);
            codegen_emit(gen, CG_STR_NL);
            if (arg_count > 0) {
                for (uint8_t i = 0; i < arg_count; i++) {
                    codegen_emit(gen, CG_STR_POP_BC);
                }
            }
            return CC_OK;
        }
        case AST_TAG_STRING_LITERAL: {
            uint16_t value_index = 0;
            if (ast_read_u16(ast->reader, &value_index) < 0) return CC_ERROR_CODEGEN;
            cc_error("String literal used without index");
            return CC_ERROR_CODEGEN;
        }
        case AST_TAG_ARRAY_ACCESS: {
            uint8_t base_tag = 0;
            uint8_t index_tag = 0;
            const char* base_name = NULL;
            const char* base_string = NULL;
            int16_t offset = 0;
            if (ast_read_u8(ast->reader, &base_tag) < 0) return CC_ERROR_CODEGEN;
            if (base_tag == AST_TAG_STRING_LITERAL) {
                if (codegen_stream_read_string(ast, &base_string) < 0) return CC_ERROR_CODEGEN;
            } else if (base_tag == AST_TAG_IDENTIFIER) {
                if (codegen_stream_read_identifier(ast, &base_name) < 0) return CC_ERROR_CODEGEN;
            } else {
                if (ast_reader_skip_tag(ast, base_tag) < 0) return CC_ERROR_CODEGEN;
            }
            if (ast_read_u8(ast->reader, &index_tag) < 0) return CC_ERROR_CODEGEN;
            if (index_tag != AST_TAG_CONSTANT) {
                ast_reader_skip_tag(ast, index_tag);
                cc_error("Unsupported array access");
                return CC_ERROR_CODEGEN;
            }
            if (ast_read_i16(ast->reader, &offset) < 0) return CC_ERROR_CODEGEN;
            if (base_string) {
                const char* label = codegen_get_string_label(gen, base_string);
                if (!label) return CC_ERROR_CODEGEN;
                codegen_emit(gen, CG_STR_LD_HL);
                codegen_emit(gen, label);
                codegen_emit(gen, CG_STR_NL);
                if (offset != 0) {
                    codegen_emit(gen, CG_STR_LD_DE);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, CG_STR_NL);
                    codegen_emit(gen, CG_STR_ADD_HL_DE);
                }
                codegen_emit(gen, CG_STR_LD_A_HL);
                return CC_OK;
            }
            if (base_name) {
                if (!codegen_local_is_pointer(gen, base_name) &&
                    !codegen_param_is_pointer(gen, base_name) &&
                    !codegen_global_is_pointer(gen, base_name)) {
                    cc_error("Unsupported array access");
                    return CC_ERROR_CODEGEN;
                }
                cc_error_t err = codegen_load_pointer_to_hl(gen, base_name);
                if (err != CC_OK) return err;
                if (offset != 0) {
                    codegen_emit(gen, CG_STR_LD_DE);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, CG_STR_NL);
                    codegen_emit(gen, CG_STR_ADD_HL_DE);
                }
                codegen_emit(gen, CG_STR_LD_A_HL);
                return CC_OK;
            }
            cc_error("Unsupported array access");
            return CC_ERROR_CODEGEN;
        }
        case AST_TAG_ASSIGN: {
            uint8_t ltag = 0;
            uint8_t rtag = 0;
            uint8_t op = 0;
            const char* lvalue_name = NULL;
            const char* rvalue_name = NULL;
            const char* rvalue_string = NULL;
            int16_t rvalue_const = 0;
            bool lvalue_deref = false;

            if (ast_read_u8(ast->reader, &ltag) < 0) return CC_ERROR_CODEGEN;
            if (ltag == AST_TAG_UNARY_OP) {
                if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
                if (op == OP_DEREF) {
                    uint8_t operand_tag = 0;
                    if (ast_read_u8(ast->reader, &operand_tag) < 0) return CC_ERROR_CODEGEN;
                    if (operand_tag == AST_TAG_IDENTIFIER) {
                        if (codegen_stream_read_identifier(ast, &lvalue_name) < 0) return CC_ERROR_CODEGEN;
                        lvalue_deref = true;
                    } else {
                        ast_reader_skip_tag(ast, operand_tag);
                        cc_error("Unsupported dereference assignment");
                        return CC_ERROR_CODEGEN;
                    }
                } else {
                    ast_reader_skip_node(ast);
                    cc_error("Unsupported assignment target");
                    return CC_ERROR_CODEGEN;
                }
            } else if (ltag == AST_TAG_IDENTIFIER) {
                if (codegen_stream_read_identifier(ast, &lvalue_name) < 0) return CC_ERROR_CODEGEN;
            } else {
                ast_reader_skip_tag(ast, ltag);
                if (ast_read_u8(ast->reader, &rtag) < 0) return CC_ERROR_CODEGEN;
                ast_reader_skip_tag(ast, rtag);
                return CC_ERROR_CODEGEN;
            }

            if (ast_read_u8(ast->reader, &rtag) < 0) return CC_ERROR_CODEGEN;

            if (lvalue_deref && lvalue_name) {
                cc_error_t err = codegen_stream_expression_tag(gen, ast, rtag);
                if (err != CC_OK) return err;
                err = codegen_load_pointer_to_hl(gen, lvalue_name);
                if (err != CC_OK) return err;
                codegen_emit(gen, CG_STR_LD_HL_A);
                return CC_OK;
            }

            if (lvalue_name &&
                (codegen_local_is_pointer(gen, lvalue_name) ||
                 codegen_param_is_pointer(gen, lvalue_name) ||
                 codegen_global_is_pointer(gen, lvalue_name))) {
                if (rtag == AST_TAG_STRING_LITERAL) {
                    if (codegen_stream_read_string(ast, &rvalue_string) < 0) return CC_ERROR_CODEGEN;
                    const char* label = codegen_get_string_label(gen, rvalue_string);
                    if (!label) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_LD_HL);
                    codegen_emit(gen, label);
                    codegen_emit(gen, CG_STR_NL);
                    return codegen_store_pointer_from_hl(gen, lvalue_name);
                }
                if (rtag == AST_TAG_UNARY_OP) {
                    if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
                    if (op == OP_ADDR) {
                        uint8_t operand_tag = 0;
                        if (ast_read_u8(ast->reader, &operand_tag) < 0) return CC_ERROR_CODEGEN;
                        if (operand_tag == AST_TAG_IDENTIFIER) {
                            if (codegen_stream_read_identifier(ast, &rvalue_name) < 0) return CC_ERROR_CODEGEN;
                            cc_error_t err = codegen_emit_address_of_identifier(gen, rvalue_name);
                            if (err != CC_OK) return err;
                            return codegen_store_pointer_from_hl(gen, lvalue_name);
                        }
                        ast_reader_skip_tag(ast, operand_tag);
                        return CC_ERROR_CODEGEN;
                    }
                    ast_reader_skip_node(ast);
                    return CC_ERROR_CODEGEN;
                }
                if (rtag == AST_TAG_IDENTIFIER) {
                    if (codegen_stream_read_identifier(ast, &rvalue_name) < 0) return CC_ERROR_CODEGEN;
                    if (codegen_local_is_pointer(gen, rvalue_name) ||
                        codegen_param_is_pointer(gen, rvalue_name) ||
                        codegen_global_is_pointer(gen, rvalue_name)) {
                        cc_error_t err = codegen_load_pointer_to_hl(gen, rvalue_name);
                        if (err != CC_OK) return err;
                        return codegen_store_pointer_from_hl(gen, lvalue_name);
                    }
                    return CC_ERROR_CODEGEN;
                }
                if (rtag == AST_TAG_CONSTANT) {
                    if (ast_read_i16(ast->reader, &rvalue_const) < 0) return CC_ERROR_CODEGEN;
                    if (rvalue_const == 0) {
                        codegen_emit(gen, CG_STR_LD_HL_ZERO);
                        return codegen_store_pointer_from_hl(gen, lvalue_name);
                    }
                    return CC_ERROR_CODEGEN;
                }
                ast_reader_skip_tag(ast, rtag);
                return CC_ERROR_CODEGEN;
            }

            cc_error_t err = codegen_stream_expression_tag(gen, ast, rtag);
            if (err != CC_OK) return err;
            if (lvalue_name) {
                int16_t offset = 0;
                if (codegen_local_offset(gen, lvalue_name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_LPAREN);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                } else if (codegen_param_offset(gen, lvalue_name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                    codegen_emit_int(gen, offset);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                } else {
                    codegen_emit(gen, CG_STR_LD_LPAREN);
                    codegen_emit_mangled_var(gen, lvalue_name);
                    codegen_emit(gen, CG_STR_RPAREN_A);
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
                codegen_emit(gen, CG_STR_LD_A_ZERO);
            }
            if (gen->return_direct || !gen->function_end_label) {
                if (gen->stack_offset > 0) {
                    codegen_emit_stack_adjust(gen, gen->stack_offset, false);
                }
                codegen_emit(gen, CG_STR_POP_IX_RET);
            } else {
                gen->use_function_end_label = true;
                codegen_emit_jump(gen, CG_STR_JP, gen->function_end_label);
            }
            return CC_OK;

        case AST_VAR_DECL:
            /* Reserve space for variable */
            codegen_emit(gen, "; Variable: ");
            codegen_emit(gen, node->data.var_decl.name);
            codegen_emit(gen, "\n");

            /* If there's an initializer, generate assignment */
            if (node->data.var_decl.initializer) {
                bool is_pointer = codegen_type_is_pointer(node->data.var_decl.var_type);
                ast_node_t* init = node->data.var_decl.initializer;
                if (is_pointer) {
                    if (init->type == AST_STRING_LITERAL) {
                        const char* label = codegen_get_string_label(gen, init->data.string_literal.value);
                        if (!label) return CC_ERROR_CODEGEN;
                        codegen_emit(gen, CG_STR_LD_HL);
                        codegen_emit(gen, label);
                        codegen_emit(gen, CG_STR_NL);
                        return codegen_store_pointer_from_hl(gen, node->data.var_decl.name);
                    }
                    if (init->type == AST_UNARY_OP &&
                        init->data.unary_op.op == OP_ADDR &&
                        init->data.unary_op.operand &&
                        init->data.unary_op.operand->type == AST_IDENTIFIER) {
                        cc_error_t err = codegen_emit_address_of_identifier(
                            gen,
                            init->data.unary_op.operand->data.identifier.name);
                        if (err != CC_OK) return err;
                        return codegen_store_pointer_from_hl(gen, node->data.var_decl.name);
                    }
                    if (init->type == AST_IDENTIFIER &&
                        (codegen_local_is_pointer(gen, init->data.identifier.name) ||
                         codegen_param_is_pointer(gen, init->data.identifier.name) ||
                         codegen_global_is_pointer(gen, init->data.identifier.name))) {
                        cc_error_t err = codegen_load_pointer_to_hl(gen, init->data.identifier.name);
                        if (err != CC_OK) return err;
                        return codegen_store_pointer_from_hl(gen, node->data.var_decl.name);
                    }
                    if (init->type == AST_CONSTANT &&
                        init->data.constant.int_value == 0) {
                        codegen_emit(gen, CG_STR_LD_HL_ZERO);
                        return codegen_store_pointer_from_hl(gen, node->data.var_decl.name);
                    }
                    cc_error("Unsupported pointer initializer");
                    return CC_ERROR_CODEGEN;
                }

                cc_error_t err = codegen_expression(gen, init);
                if (err != CC_OK) return err;
                {
                    int16_t offset = 0;
                    if (codegen_local_offset(gen, node->data.var_decl.name, &offset)) {
                        codegen_emit(gen, CG_STR_LD_LPAREN);
                        codegen_emit_ix_offset(gen, offset);
                        codegen_emit(gen, CG_STR_RPAREN_A);
                    } else {
                        codegen_emit(gen, CG_STR_LD_LPAREN);
                        codegen_emit_mangled_var(gen, node->data.var_decl.name);
                        codegen_emit(gen, CG_STR_RPAREN_A);
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

                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, else_label); /* Test if A is zero */

                /* Then branch */
                err = codegen_statement(gen, node->data.if_stmt.then_branch);
                if (err != CC_OK) {
                    return err;
                }

                codegen_emit_jump(gen, CG_STR_JP, end_label);

                /* Else branch */
                codegen_emit_label(gen, else_label);
                err = codegen_statement(gen, node->data.if_stmt.else_branch);
                if (err != CC_OK) {
                    return err;
                }

                /* End label */
                codegen_emit_label(gen, end_label);

            } else {
                /* Simple if: jump over then branch on false */
                char* end_label = codegen_new_label(gen);

                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label); /* Test if A is zero */

                /* Then branch */
                err = codegen_statement(gen, node->data.if_stmt.then_branch);
                if (err != CC_OK) {
                    return err;
                }

                /* End label */
                codegen_emit_label(gen, end_label);

            }
            return CC_OK;
        }

        case AST_WHILE_STMT: {
            /* While loop: test at top, jump to end if false, jump back to top at end of body */
            char* loop_label = codegen_new_label(gen);
            char* end_label = codegen_new_label(gen);

            /* Loop start label */
            codegen_emit_label(gen, loop_label);

            /* Evaluate condition */
            cc_error_t err = codegen_expression(gen, node->data.while_stmt.condition);
            if (err != CC_OK) {
                return err;
            }

            /* Test condition (A register) */
            codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);

            /* Loop body */
            err = codegen_statement(gen, node->data.while_stmt.body);
            if (err != CC_OK) {
                return err;
            }

            /* Jump back to loop start */
            codegen_emit_jump(gen, CG_STR_JP, loop_label);

            /* End label */
            codegen_emit_label(gen, end_label);

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
                    return err;
                }
            }

            /* Loop start label */
            codegen_emit_label(gen, loop_label);

            /* Evaluate condition (if present) */
            if (node->data.for_stmt.condition) {
                err = codegen_expression(gen, node->data.for_stmt.condition);
                if (err != CC_OK) {
                    return err;
                }

                /* Test condition */
                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
            }

            /* Loop body */
            err = codegen_statement(gen, node->data.for_stmt.body);
            if (err != CC_OK) {
                return err;
            }

            /* Increment expression */
            if (node->data.for_stmt.increment) {
                err = codegen_expression(gen, node->data.for_stmt.increment);
                if (err != CC_OK) {
                    return err;
                }
            }

            /* Jump back to loop start */
            codegen_emit_jump(gen, CG_STR_JP, loop_label);

            /* End label */
            codegen_emit_label(gen, end_label);

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

static cc_error_t codegen_stream_statement_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    switch (tag) {
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = 0;
            if (ast_read_u8(ast->reader, &has_expr) < 0) return CC_ERROR_CODEGEN;
            if (has_expr) {
                uint8_t expr_tag = 0;
                if (ast_read_u8(ast->reader, &expr_tag) < 0) return CC_ERROR_CODEGEN;
                cc_error_t err = codegen_stream_expression_tag(gen, ast, expr_tag);
                if (err != CC_OK) return err;
            } else {
                codegen_emit(gen, CG_STR_LD_A_ZERO);
            }
            if (gen->return_direct || !gen->function_end_label) {
                if (gen->stack_offset > 0) {
                    codegen_emit_stack_adjust(gen, gen->stack_offset, false);
                }
                codegen_emit(gen, CG_STR_POP_IX_RET);
            } else {
                gen->use_function_end_label = true;
                codegen_emit_jump(gen, CG_STR_JP, gen->function_end_label);
            }
            return CC_OK;
        }
        case AST_TAG_VAR_DECL: {
            uint16_t name_index = 0;
            uint8_t has_init = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            const char* name = NULL;
            if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
            if (ast_reader_read_type_info(ast, &base, &depth) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
            name = codegen_stream_string(ast, name_index);
            if (!name) return CC_ERROR_CODEGEN;
            codegen_emit(gen, "; Variable: ");
            codegen_emit(gen, name);
            codegen_emit(gen, "\n");
            if (has_init) {
                uint8_t init_tag = 0;
                if (ast_read_u8(ast->reader, &init_tag) < 0) return CC_ERROR_CODEGEN;
                bool is_pointer = codegen_stream_type_is_pointer(depth);
                if (is_pointer) {
                    if (init_tag == AST_TAG_STRING_LITERAL) {
                        const char* init_str = NULL;
                        if (codegen_stream_read_string(ast, &init_str) < 0) return CC_ERROR_CODEGEN;
                        const char* label = codegen_get_string_label(gen, init_str);
                        if (!label) return CC_ERROR_CODEGEN;
                        codegen_emit(gen, CG_STR_LD_HL);
                        codegen_emit(gen, label);
                        codegen_emit(gen, CG_STR_NL);
                        return codegen_store_pointer_from_hl(gen, name);
                    }
                    if (init_tag == AST_TAG_UNARY_OP) {
                        uint8_t op = 0;
                        uint8_t operand_tag = 0;
                        if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
                        if (ast_read_u8(ast->reader, &operand_tag) < 0) return CC_ERROR_CODEGEN;
                        if (op == OP_ADDR && operand_tag == AST_TAG_IDENTIFIER) {
                            const char* ident = NULL;
                            if (codegen_stream_read_identifier(ast, &ident) < 0) return CC_ERROR_CODEGEN;
                            cc_error_t err = codegen_emit_address_of_identifier(gen, ident);
                            if (err != CC_OK) return err;
                            return codegen_store_pointer_from_hl(gen, name);
                        }
                        ast_reader_skip_tag(ast, operand_tag);
                        return CC_ERROR_CODEGEN;
                    }
                    if (init_tag == AST_TAG_IDENTIFIER) {
                        const char* ident = NULL;
                        if (codegen_stream_read_identifier(ast, &ident) < 0) return CC_ERROR_CODEGEN;
                        if (codegen_local_is_pointer(gen, ident) ||
                            codegen_param_is_pointer(gen, ident) ||
                            codegen_global_is_pointer(gen, ident)) {
                            cc_error_t err = codegen_load_pointer_to_hl(gen, ident);
                            if (err != CC_OK) return err;
                            return codegen_store_pointer_from_hl(gen, name);
                        }
                        return CC_ERROR_CODEGEN;
                    }
                    if (init_tag == AST_TAG_CONSTANT) {
                        int16_t val = 0;
                        if (ast_read_i16(ast->reader, &val) < 0) return CC_ERROR_CODEGEN;
                        if (val == 0) {
                            codegen_emit(gen, CG_STR_LD_HL_ZERO);
                            return codegen_store_pointer_from_hl(gen, name);
                        }
                        return CC_ERROR_CODEGEN;
                    }
                    ast_reader_skip_tag(ast, init_tag);
                    return CC_ERROR_CODEGEN;
                }
                cc_error_t err = codegen_stream_expression_tag(gen, ast, init_tag);
                if (err != CC_OK) return err;
                int16_t offset = 0;
                if (codegen_local_offset(gen, name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_LPAREN);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                } else {
                    codegen_emit(gen, CG_STR_LD_LPAREN);
                    codegen_emit_mangled_var(gen, name);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                }
            }
            return CC_OK;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = 0;
            if (ast_read_u16(ast->reader, &stmt_count) < 0) return CC_ERROR_CODEGEN;
            for (uint16_t i = 0; i < stmt_count; i++) {
                uint8_t stmt_tag = 0;
                if (ast_read_u8(ast->reader, &stmt_tag) < 0) return CC_ERROR_CODEGEN;
                cc_error_t err = codegen_stream_statement_tag(gen, ast, stmt_tag);
                if (err != CC_OK) return err;
            }
            return CC_OK;
        }
        case AST_TAG_IF_STMT: {
            uint8_t has_else = 0;
            if (ast_read_u8(ast->reader, &has_else) < 0) return CC_ERROR_CODEGEN;
            uint8_t cond_tag = 0;
            if (ast_read_u8(ast->reader, &cond_tag) < 0) return CC_ERROR_CODEGEN;
            cc_error_t err = codegen_stream_expression_tag(gen, ast, cond_tag);
            if (err != CC_OK) return err;
            if (has_else) {
                char* else_label = codegen_new_label(gen);
                char* end_label = codegen_new_label(gen);
                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, else_label);
                uint8_t then_tag = 0;
                if (ast_read_u8(ast->reader, &then_tag) < 0) return CC_ERROR_CODEGEN;
                err = codegen_stream_statement_tag(gen, ast, then_tag);
                if (err != CC_OK) {
                    return err;
                }
                codegen_emit_jump(gen, CG_STR_JP, end_label);
                codegen_emit_label(gen, else_label);
                uint8_t else_tag = 0;
                if (ast_read_u8(ast->reader, &else_tag) < 0) return CC_ERROR_CODEGEN;
                err = codegen_stream_statement_tag(gen, ast, else_tag);
                if (err != CC_OK) {
                    return err;
                }
                codegen_emit_label(gen, end_label);
            } else {
                char* end_label = codegen_new_label(gen);
                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
                uint8_t then_tag = 0;
                if (ast_read_u8(ast->reader, &then_tag) < 0) return CC_ERROR_CODEGEN;
                err = codegen_stream_statement_tag(gen, ast, then_tag);
                if (err != CC_OK) {
                    return err;
                }
                codegen_emit_label(gen, end_label);
            }
            return CC_OK;
        }
        case AST_TAG_WHILE_STMT: {
            char* loop_label = codegen_new_label(gen);
            char* end_label = codegen_new_label(gen);
            codegen_emit_label(gen, loop_label);
            uint8_t cond_tag = 0;
            if (ast_read_u8(ast->reader, &cond_tag) < 0) return CC_ERROR_CODEGEN;
            cc_error_t err = codegen_stream_expression_tag(gen, ast, cond_tag);
            if (err != CC_OK) {
                return err;
            }
            codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
            uint8_t body_tag = 0;
            if (ast_read_u8(ast->reader, &body_tag) < 0) return CC_ERROR_CODEGEN;
            err = codegen_stream_statement_tag(gen, ast, body_tag);
            if (err != CC_OK) {
                return err;
            }
            codegen_emit_jump(gen, CG_STR_JP, loop_label);
            codegen_emit_label(gen, end_label);
            return CC_OK;
        }
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = 0;
            uint8_t has_cond = 0;
            uint8_t has_inc = 0;
            uint32_t inc_offset = 0;
            uint32_t body_end = 0;
            if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &has_cond) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &has_inc) < 0) return CC_ERROR_CODEGEN;
            char* loop_label = codegen_new_label(gen);
            char* end_label = codegen_new_label(gen);
            cc_error_t err;
            if (has_init) {
                uint8_t init_tag = 0;
                if (ast_read_u8(ast->reader, &init_tag) < 0) return CC_ERROR_CODEGEN;
                err = codegen_stream_statement_tag(gen, ast, init_tag);
                if (err != CC_OK) {
                    return err;
                }
            }
            codegen_emit_label(gen, loop_label);
            if (has_cond) {
                uint8_t cond_tag = 0;
                if (ast_read_u8(ast->reader, &cond_tag) < 0) return CC_ERROR_CODEGEN;
                err = codegen_stream_expression_tag(gen, ast, cond_tag);
                if (err != CC_OK) {
                    return err;
                }
                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
            }
            if (has_inc) {
                inc_offset = reader_tell(ast->reader);
                if (ast_reader_skip_node(ast) < 0) {
                    return CC_ERROR_CODEGEN;
                }
            }
            {
                uint8_t body_tag = 0;
                if (ast_read_u8(ast->reader, &body_tag) < 0) return CC_ERROR_CODEGEN;
                err = codegen_stream_statement_tag(gen, ast, body_tag);
            }
            if (err != CC_OK) {
                return err;
            }
            body_end = reader_tell(ast->reader);
            if (has_inc) {
                if (reader_seek(ast->reader, inc_offset) < 0) {
                    return CC_ERROR_CODEGEN;
                }
                uint8_t inc_tag = 0;
                if (ast_read_u8(ast->reader, &inc_tag) < 0) return CC_ERROR_CODEGEN;
                err = codegen_stream_expression_tag(gen, ast, inc_tag);
                if (err != CC_OK) {
                    return err;
                }
                if (reader_seek(ast->reader, body_end) < 0) {
                    return CC_ERROR_CODEGEN;
                }
            }
            codegen_emit_jump(gen, CG_STR_JP, loop_label);
            codegen_emit_label(gen, end_label);
            return CC_OK;
        }
        case AST_TAG_ASSIGN:
        case AST_TAG_CALL:
            return codegen_stream_expression_tag(gen, ast, tag);
        default:
            return CC_OK;
    }
}

static int8_t codegen_stream_collect_locals(codegen_t* gen, ast_reader_t* ast) {
    uint8_t tag = 0;
    if (ast_read_u8(ast->reader, &tag) < 0) return -1;
    switch (tag) {
        case AST_TAG_VAR_DECL: {
            uint16_t name_index = 0;
            uint8_t has_init = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return -1;
            if (ast_reader_read_type_info(ast, &base, &depth) < 0) return -1;
            if (ast_read_u8(ast->reader, &has_init) < 0) return -1;
            const char* name = codegen_stream_string(ast, name_index);
            if (!name) return -1;
            codegen_record_local(gen, name,
                                 codegen_stream_type_storage_size(depth),
                                 codegen_stream_type_is_pointer(depth));
            if (has_init) return ast_reader_skip_node(ast);
            return 0;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = 0;
            if (ast_read_u16(ast->reader, &stmt_count) < 0) return -1;
            for (uint16_t i = 0; i < stmt_count; i++) {
                if (codegen_stream_collect_locals(gen, ast) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_IF_STMT: {
            uint8_t has_else = 0;
            if (ast_read_u8(ast->reader, &has_else) < 0) return -1;
            if (ast_reader_skip_node(ast) < 0) return -1;
            if (codegen_stream_collect_locals(gen, ast) < 0) return -1;
            if (has_else) return codegen_stream_collect_locals(gen, ast);
            return 0;
        }
        case AST_TAG_WHILE_STMT:
            if (ast_reader_skip_node(ast) < 0) return -1;
            return codegen_stream_collect_locals(gen, ast);
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = 0;
            uint8_t has_cond = 0;
            uint8_t has_inc = 0;
            if (ast_read_u8(ast->reader, &has_init) < 0) return -1;
            if (ast_read_u8(ast->reader, &has_cond) < 0) return -1;
            if (ast_read_u8(ast->reader, &has_inc) < 0) return -1;
            if (has_init && codegen_stream_collect_locals(gen, ast) < 0) return -1;
            if (has_cond && ast_reader_skip_node(ast) < 0) return -1;
            if (has_inc && ast_reader_skip_node(ast) < 0) return -1;
            return codegen_stream_collect_locals(gen, ast);
        }
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = 0;
            if (ast_read_u8(ast->reader, &has_expr) < 0) return -1;
            if (has_expr) return ast_reader_skip_node(ast);
            return 0;
        }
        default:
            return ast_reader_skip_tag(ast, tag);
    }
}

static cc_error_t codegen_stream_function(codegen_t* gen, ast_reader_t* ast) {
    uint16_t name_index = 0;
    uint8_t param_count = 0;
    uint8_t base = 0;
    uint8_t depth = 0;
    const char* name = NULL;
    const char** param_names = g_param_names;
    bool* param_is_pointer = g_param_is_pointer;
    uint8_t param_used = 0;
    uint32_t body_start = 0;
    uint32_t body_end = 0;
    bool last_was_return = false;

    if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
    if (ast_reader_read_type_info(ast, &base, &depth) < 0) return CC_ERROR_CODEGEN;
    if (ast_read_u8(ast->reader, &param_count) < 0) return CC_ERROR_CODEGEN;
    name = codegen_stream_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;

    gen->local_var_count = 0;
    gen->defer_var_storage = false;
    gen->param_count = 0;
    gen->function_end_label = NULL;
    gen->return_direct = false;
    gen->use_function_end_label = false;
    gen->stack_offset = 0;

    for (uint8_t i = 0; i < param_count; i++) {
        uint8_t tag = 0;
        uint16_t param_name_index = 0;
        uint8_t param_depth = 0;
        uint8_t param_base = 0;
        uint8_t has_init = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag != AST_TAG_VAR_DECL) return CC_ERROR_CODEGEN;
        if (ast_read_u16(ast->reader, &param_name_index) < 0) return CC_ERROR_CODEGEN;
        if (ast_reader_read_type_info(ast, &param_base, &param_depth) < 0) return CC_ERROR_CODEGEN;
        if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
        if (has_init && ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
        if (param_used < (uint8_t)(sizeof(param_names) / sizeof(param_names[0]))) {
            param_names[param_used] = codegen_stream_string(ast, param_name_index);
            param_is_pointer[param_used] = codegen_stream_type_is_pointer(param_depth);
            param_used++;
        }
    }

    codegen_emit_label(gen, name);

    body_start = reader_tell(ast->reader);
    if (codegen_stream_collect_locals(gen, ast) < 0) return CC_ERROR_CODEGEN;
    body_end = reader_tell(ast->reader);

    for (uint8_t i = 0; i < param_used; i++) {
        gen->param_names[gen->param_count] = param_names[i];
        gen->param_offsets[gen->param_count] =
            (int16_t)(gen->stack_offset + 4 + (int16_t)(2 * gen->param_count));
        gen->param_is_pointer[gen->param_count] = param_is_pointer[i];
        gen->param_count++;
    }

    gen->function_end_label = codegen_new_label_persist(gen);
    codegen_emit(gen, CG_STR_PUSH_IX);
    codegen_emit(gen, CG_STR_IX_FRAME_SET);
    if (gen->stack_offset > 0) {
        codegen_emit_stack_adjust(gen, gen->stack_offset, true);
        codegen_emit(gen, CG_STR_IX_FRAME_SET);
    }

    if (reader_seek(ast->reader, body_start) < 0) return CC_ERROR_CODEGEN;
    uint8_t body_tag = 0;
    if (ast_read_u8(ast->reader, &body_tag) < 0) return CC_ERROR_CODEGEN;
    if (body_tag == AST_TAG_COMPOUND_STMT) {
        uint16_t stmt_count = 0;
        if (ast_read_u16(ast->reader, &stmt_count) < 0) return CC_ERROR_CODEGEN;
        for (uint16_t i = 0; i < stmt_count; i++) {
            uint8_t stmt_tag = 0;
            if (ast_read_u8(ast->reader, &stmt_tag) < 0) return CC_ERROR_CODEGEN;
            if (i + 1 == stmt_count && stmt_tag == AST_TAG_RETURN_STMT) {
                last_was_return = true;
                gen->return_direct = true;
            }
            cc_error_t err = codegen_stream_statement_tag(gen, ast, stmt_tag);
            gen->return_direct = false;
            if (err != CC_OK) return err;
        }
    } else if (body_tag == AST_TAG_RETURN_STMT) {
        last_was_return = true;
        gen->return_direct = true;
        cc_error_t err = codegen_stream_statement_tag(gen, ast, body_tag);
        gen->return_direct = false;
        if (err != CC_OK) return err;
    } else {
        cc_error_t err = codegen_stream_statement_tag(gen, ast, body_tag);
        if (err != CC_OK) return err;
    }

    if (gen->use_function_end_label) {
        codegen_emit_label(gen, gen->function_end_label);
        if (gen->stack_offset > 0) {
            codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    } else if (!last_was_return) {
        if (gen->stack_offset > 0) {
            codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    }

    gen->defer_var_storage = false;
    if (gen->function_end_label) {
        cc_free(gen->function_end_label);
        gen->function_end_label = NULL;
    }
    codegen_emit(gen, CG_STR_NL);

    if (reader_seek(ast->reader, body_end) < 0) return CC_ERROR_CODEGEN;
    return CC_OK;
}

static cc_error_t codegen_stream_global_var(codegen_t* gen, ast_reader_t* ast) {
    uint16_t name_index = 0;
    uint8_t has_init = 0;
    uint8_t base = 0;
    uint8_t depth = 0;
    if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
    if (ast_reader_read_type_info(ast, &base, &depth) < 0) return CC_ERROR_CODEGEN;
    if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
    const char* name = codegen_stream_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;
    bool is_pointer = codegen_stream_type_is_pointer(depth);
    codegen_emit(gen, "; Global variable: ");
    codegen_emit(gen, name);
    codegen_emit(gen, "\n");
    codegen_emit_mangled_var(gen, name);

    if (is_pointer) {
        if (has_init) {
            uint8_t tag = 0;
            if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
            if (tag == AST_TAG_STRING_LITERAL) {
                const char* init_str = NULL;
                if (codegen_stream_read_string(ast, &init_str) < 0) return CC_ERROR_CODEGEN;
                const char* label = codegen_get_string_label(gen, init_str);
                if (!label) return CC_ERROR_CODEGEN;
                codegen_emit(gen, CG_STR_COLON_DW);
                codegen_emit(gen, label);
                codegen_emit(gen, CG_STR_NL);
                return CC_OK;
            }
            if (tag == AST_TAG_UNARY_OP) {
                uint8_t op = 0;
                uint8_t operand_tag = 0;
                if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
                if (ast_read_u8(ast->reader, &operand_tag) < 0) return CC_ERROR_CODEGEN;
                if (op == OP_ADDR && operand_tag == AST_TAG_IDENTIFIER) {
                    const char* ident = NULL;
                    if (codegen_stream_read_identifier(ast, &ident) < 0) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_COLON_DW);
                    codegen_emit_mangled_var(gen, ident);
                    codegen_emit(gen, CG_STR_NL);
                    return CC_OK;
                }
                ast_reader_skip_tag(ast, operand_tag);
                codegen_emit(gen, CG_STR_COLON_DW_ZERO);
                return CC_OK;
            }
            if (tag == AST_TAG_CONSTANT) {
                int16_t value = 0;
                if (ast_read_i16(ast->reader, &value) < 0) return CC_ERROR_CODEGEN;
                if (value == 0) {
                    codegen_emit(gen, CG_STR_COLON_DW_ZERO);
                    return CC_OK;
                }
            } else {
                ast_reader_skip_tag(ast, tag);
            }
        }
        codegen_emit(gen, CG_STR_COLON_DW_ZERO);
        return CC_OK;
    }

    if (has_init) {
        uint8_t tag = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag == AST_TAG_CONSTANT) {
            int16_t value = 0;
            if (ast_read_i16(ast->reader, &value) < 0) return CC_ERROR_CODEGEN;
            codegen_emit(gen, CG_STR_COLON_DB);
            codegen_emit_int(gen, value);
            codegen_emit(gen, CG_STR_NL);
            return CC_OK;
        }
        ast_reader_skip_tag(ast, tag);
    }
    codegen_emit(gen, CG_STR_COLON_DB_ZERO);
    return CC_OK;
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

    codegen_emit_label(gen, node->data.function.name);

    /* Collect locals for stack allocation */
    if (node->data.function.body) {
        codegen_collect_locals(gen, node->data.function.body);
    }

    if (node->data.function.param_count > 0) {
        for (size_t i = 0; i < node->data.function.param_count && i < 8; i++) {
            ast_node_t* param = node->data.function.params[i];
            if (!param || param->type != AST_VAR_DECL) continue;
            gen->param_names[gen->param_count] = param->data.var_decl.name;
            gen->param_offsets[gen->param_count] = (int16_t)(gen->stack_offset + 4 + (int16_t)(2 * gen->param_count));
            gen->param_is_pointer[gen->param_count] =
                codegen_type_is_pointer(param->data.var_decl.var_type);
            gen->param_count++;
        }
    }

    gen->function_end_label = codegen_new_label_persist(gen);
    codegen_emit(gen, CG_STR_PUSH_IX);
    codegen_emit(gen, CG_STR_IX_FRAME_SET);
    if (gen->stack_offset > 0) {
        codegen_emit_stack_adjust(gen, gen->stack_offset, true);
        codegen_emit(gen, CG_STR_IX_FRAME_SET);
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
                codegen_emit(gen, CG_STR_LD_A_ZERO);
            }

    if (gen->use_function_end_label) {
        codegen_emit_label(gen, gen->function_end_label);
        if (gen->stack_offset > 0) {
            codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    } else if (!last_was_return) {
        if (gen->stack_offset > 0) {
            codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    }

    gen->defer_var_storage = false;
    if (gen->function_end_label) {
        cc_free(gen->function_end_label);
        gen->function_end_label = NULL;
    }
    codegen_emit(gen, CG_STR_NL);

    return CC_OK;
}

static cc_error_t codegen_global_var(codegen_t* gen, ast_node_t* node) {
    if (!node || node->type != AST_VAR_DECL) {
        return CC_ERROR_INTERNAL;
    }
    bool is_pointer = codegen_type_is_pointer(node->data.var_decl.var_type);
    if (gen->global_count < (sizeof(gen->global_names) / sizeof(gen->global_names[0]))) {
        if (codegen_global_index(gen, node->data.var_decl.name) < 0) {
            gen->global_names[gen->global_count] = node->data.var_decl.name;
            gen->global_is_pointer[gen->global_count] = is_pointer;
            gen->global_count++;
        }
    }
    codegen_emit(gen, "; Global variable: ");
    codegen_emit(gen, node->data.var_decl.name);
    codegen_emit(gen, "\n");
    codegen_emit_mangled_var(gen, node->data.var_decl.name);
    if (is_pointer) {
        if (node->data.var_decl.initializer &&
            node->data.var_decl.initializer->type == AST_STRING_LITERAL) {
            const char* label = codegen_get_string_label(
                gen,
                node->data.var_decl.initializer->data.string_literal.value);
            if (!label) return CC_ERROR_CODEGEN;
            codegen_emit(gen, CG_STR_COLON_DW);
            codegen_emit(gen, label);
            codegen_emit(gen, CG_STR_NL);
            return CC_OK;
        }
        if (node->data.var_decl.initializer &&
            node->data.var_decl.initializer->type == AST_UNARY_OP &&
            node->data.var_decl.initializer->data.unary_op.op == OP_ADDR &&
            node->data.var_decl.initializer->data.unary_op.operand &&
            node->data.var_decl.initializer->data.unary_op.operand->type == AST_IDENTIFIER) {
            codegen_emit(gen, CG_STR_COLON_DW);
            codegen_emit_mangled_var(
                gen,
                node->data.var_decl.initializer->data.unary_op.operand->data.identifier.name);
            codegen_emit(gen, CG_STR_NL);
            return CC_OK;
        }
        if (node->data.var_decl.initializer &&
            node->data.var_decl.initializer->type == AST_CONSTANT &&
            node->data.var_decl.initializer->data.constant.int_value == 0) {
            codegen_emit(gen, CG_STR_COLON_DW_ZERO);
            return CC_OK;
        }
        codegen_emit(gen, CG_STR_COLON_DW_ZERO);
        return CC_OK;
    }
    if (node->data.var_decl.initializer &&
        node->data.var_decl.initializer->type == AST_CONSTANT) {
        int16_t value = node->data.var_decl.initializer->data.constant.int_value;
        codegen_emit(gen, CG_STR_COLON_DB);
        codegen_emit_int(gen, value);
        codegen_emit(gen, CG_STR_NL);
    } else {
        codegen_emit(gen, CG_STR_COLON_DB_ZERO);
    }
    return CC_OK;
}

cc_error_t codegen_generate_global(codegen_t* gen, ast_node_t* decl) {
    return codegen_global_var(gen, decl);
}

void codegen_register_global(codegen_t* gen, ast_node_t* decl) {
    if (!gen || !decl || decl->type != AST_VAR_DECL) return;
    if (codegen_global_index(gen, decl->data.var_decl.name) >= 0) return;
    if (gen->global_count >= (sizeof(gen->global_names) / sizeof(gen->global_names[0]))) return;
    gen->global_names[gen->global_count] = decl->data.var_decl.name;
    gen->global_is_pointer[gen->global_count] =
        codegen_type_is_pointer(decl->data.var_decl.var_type);
    gen->global_count++;
}

void codegen_emit_runtime(codegen_t* gen) {
    codegen_emit_file(gen, "runtime/runtime.asm");
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
            codegen_emit(gen, CG_STR_DB);
            codegen_emit_int(gen, (unsigned char)value[idx]);
            codegen_emit(gen, CG_STR_NL);
            idx++;
        }
        codegen_emit(gen, CG_STR_DB_ZERO);
    }
}

void codegen_emit_preamble(codegen_t* gen) {
    if (!gen) return;
    codegen_emit(gen,
        "; Generated by Zeal 8-bit C Compiler\n"
        "; Target: Z80\n\n"
        "  org 0x4000\n"
        "\n");
    codegen_emit_file(gen, "runtime/crt0.asm");
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

cc_error_t codegen_generate_stream(codegen_t* gen, ast_reader_t* ast) {
    uint16_t decl_count = 0;
    if (!gen || !ast) return CC_ERROR_INTERNAL;

    codegen_emit_preamble(gen);

    if (ast_reader_begin_program(ast, &decl_count) < 0) {
        return CC_ERROR_CODEGEN;
    }
    for (uint16_t i = 0; i < decl_count; i++) {
        uint8_t tag = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag == AST_TAG_VAR_DECL) {
            uint16_t name_index = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint8_t has_init = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
            if (ast_reader_read_type_info(ast, &base, &depth) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
            const char* name = codegen_stream_string(ast, name_index);
            if (name && codegen_global_index(gen, name) < 0) {
                if (gen->global_count < (sizeof(gen->global_names) / sizeof(gen->global_names[0]))) {
                    gen->global_names[gen->global_count] = name;
                    gen->global_is_pointer[gen->global_count] = codegen_stream_type_is_pointer(depth);
                    gen->global_count++;
                }
            }
            if (has_init && ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
        } else {
            if (ast_reader_skip_tag(ast, tag) < 0) return CC_ERROR_CODEGEN;
        }
    }

    if (ast_reader_begin_program(ast, &decl_count) < 0) {
        return CC_ERROR_CODEGEN;
    }
    for (uint16_t i = 0; i < decl_count; i++) {
        uint8_t tag = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag == AST_TAG_FUNCTION) {
            cc_error_t err = codegen_stream_function(gen, ast);
            if (err != CC_OK) return err;
        } else {
            if (ast_reader_skip_tag(ast, tag) < 0) return CC_ERROR_CODEGEN;
        }
    }

    if (ast_reader_begin_program(ast, &decl_count) < 0) {
        return CC_ERROR_CODEGEN;
    }
    for (uint16_t i = 0; i < decl_count; i++) {
        uint8_t tag = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag == AST_TAG_VAR_DECL) {
            cc_error_t err = codegen_stream_global_var(gen, ast);
            if (err != CC_OK) return err;
        } else {
            if (ast_reader_skip_tag(ast, tag) < 0) return CC_ERROR_CODEGEN;
        }
    }

    codegen_emit_strings(gen);
    codegen_emit_runtime(gen);

    return CC_OK;
}

cc_error_t codegen_write_output(codegen_t* gen) {
    /* Streaming write already performed; nothing to do */
    (void)gen;
    return CC_OK;
}
