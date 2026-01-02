#include "codegen.h"

#include "ast_format.h"
#include "ast_io.h"
#include "ast_reader.h"
#include "common.h"
#include "codegen_strings.h"
#include "target.h"
#include "cc_compat.h"

#define INITIAL_OUTPUT_CAPACITY 1024
#define CODEGEN_LABEL_MAX 15 /* Zealasm docs say 16, but 15 avoids edge-case failures. */
#define CODEGEN_LABEL_HASH_LEN 4

typedef struct {
    uint8_t op;
    const char* prelude;
    const char* jump1;
    const char* jump2;
} compare_entry_t;

typedef struct {
    uint8_t op;
    const char* seq;
} op_emit_entry_t;

typedef cc_error_t (*statement_handler_t)(codegen_t* gen, ast_reader_t* ast, uint8_t tag);

/* Forward declarations */
static cc_error_t codegen_stream_expression_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag);
static cc_error_t codegen_stream_expression_expect(codegen_t* gen, ast_reader_t* ast,
                                                   uint8_t tag, bool expect_hl);
static cc_error_t codegen_stream_statement_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag);
static int8_t codegen_stream_collect_locals(codegen_t* gen, ast_reader_t* ast);
static cc_error_t codegen_stream_function(codegen_t* gen, ast_reader_t* ast);
static cc_error_t codegen_stream_global_var(codegen_t* gen, ast_reader_t* ast);
static void codegen_emit_ix_offset(codegen_t* gen, int16_t offset);
static void codegen_emit_hex(codegen_t* gen, uint16_t value);
static bool codegen_stream_type_is_16bit(uint8_t base, uint8_t depth);
static void codegen_emit_string_literal(codegen_t* gen, const char* value);


static uint32_t g_arg_offsets[8];
static char g_emit_buf[CODEGEN_LABEL_MAX + 1];
static codegen_t g_codegen;
/* Indicates whether the last expression emitted left its result in HL (true)
   or in A (false). Used by call-site code to decide how to push arguments. */
static bool g_result_in_hl = false;
/* Forces expression emission to produce a 16-bit result in HL when true. */
static bool g_expect_result_in_hl = false;

/* Helpers */

static bool codegen_tag_is_simple_expr(uint8_t tag) {
    return tag == AST_TAG_CONSTANT ||
           tag == AST_TAG_IDENTIFIER ||
           tag == AST_TAG_CALL ||
           tag == AST_TAG_BINARY_OP ||
           tag == AST_TAG_UNARY_OP;
}

static const char g_hex_digits[] = "0123456789abcdef";

static uint8_t codegen_base_type(uint8_t base) {
    return (uint8_t)(base & AST_BASE_MASK);
}

static bool codegen_base_is_unsigned(uint8_t base) {
    return (base & AST_BASE_FLAG_UNSIGNED) != 0;
}

static void codegen_result_to_hl(codegen_t* gen) {
    if (g_result_in_hl) {
        return;
    }
    codegen_emit(gen, CG_STR_LD_L_A_H_ZERO);
    g_result_in_hl = true;
}

static void codegen_result_sign_extend_to_hl(codegen_t* gen) {
    if (g_result_in_hl) {
        return;
    }
    codegen_emit(gen,
        "  ld l, a\n"
        "  add a, a\n"
        "  sbc a, a\n"
        "  ld h, a\n");
    g_result_in_hl = true;
}

static void codegen_result_to_a(codegen_t* gen) {
    if (!g_result_in_hl) {
        return;
    }
    codegen_emit(gen, CG_STR_LD_A_L);
    g_result_in_hl = false;
}

static char* codegen_format_label(char labels[8][16], uint8_t* slot,
                                  char prefix, uint16_t n) {
    char* label = labels[(*slot)++ & 7];
    char* out = label;
    *out++ = '_';
    *out++ = prefix;
    for (uint16_t pos = 0; pos < 6; pos++) {
        uint16_t digit = n % 10;
        out[5 - pos] = (char)('0' + digit);
        n /= 10;
    }
    out += 6;
    *out = '\0';
    return label;
}

static void codegen_emit_label_name(codegen_t* gen, const char* name) {
    if (!gen || !name) return;
    uint16_t i = 0;
    uint16_t hash = 0x811c;
    bool need_hash = false;
    while (name[i]) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + ('a' - 'A'));
        }
        if (i < CODEGEN_LABEL_MAX) {
            g_emit_buf[i] = c;
        } else {
            need_hash = true;
        }
        hash = (uint16_t)((hash * 33u) ^ (uint8_t)c);
        i++;
    }
    if (!need_hash) {
        g_emit_buf[i] = '\0';
        codegen_emit(gen, g_emit_buf);
        return;
    }
    {
        uint16_t keep = CODEGEN_LABEL_MAX - 1 - CODEGEN_LABEL_HASH_LEN;
        uint16_t out = keep;
        g_emit_buf[out++] = '_';
        for (int shift = 12; shift >= 0; shift -= 4) {
            g_emit_buf[out++] = g_hex_digits[(hash >> shift) & 0xF];
        }
        g_emit_buf[out] = '\0';
        codegen_emit(gen, g_emit_buf);
    }
}

static void codegen_emit_mangled_var(codegen_t* gen, const char* name) {
    if (!gen || !name) return;
    char* out = g_emit_buf;
    uint16_t i = 0;
    uint16_t hash = 0x811c;
    bool need_hash = false;
    *out++ = '_';
    *out++ = 'v';
    *out++ = '_';
    i = 3;
    for (uint16_t n = 0; name[n]; n++) {
        char c = name[n];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + ('a' - 'A'));
        }
        if (i < CODEGEN_LABEL_MAX) {
            *out = c;
        } else {
            need_hash = true;
        }
        hash = (uint16_t)((hash * 33u) ^ (uint8_t)c);
        i++;
        if (i <= CODEGEN_LABEL_MAX) {
            out++;
        }
    }
    if (!need_hash) {
        *out = '\0';
        codegen_emit(gen, g_emit_buf);
        return;
    }
    {
        uint16_t keep = CODEGEN_LABEL_MAX - 1 - CODEGEN_LABEL_HASH_LEN;
        uint16_t out = keep;
        g_emit_buf[out++] = '_';
        for (int shift = 12; shift >= 0; shift -= 4) {
            g_emit_buf[out++] = g_hex_digits[(hash >> shift) & 0xF];
        }
        g_emit_buf[out] = '\0';
        codegen_emit(gen, g_emit_buf);
    }
}

static bool codegen_names_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    return str_cmp(a, b) == 0;
}

static void codegen_emit_hex(codegen_t* gen, uint16_t value) {
    char* buf = g_emit_buf;
    *buf++ = '0';
    *buf++ = 'x';
    if (value > 0xFF) {
        *buf++ = g_hex_digits[(value >> 12) & 0xF];
        *buf++ = g_hex_digits[(value >> 8) & 0xF];
    }
    *buf++ = g_hex_digits[(value >> 4) & 0xF];
    *buf++ = g_hex_digits[value & 0xF];
    *buf = '\0';
    codegen_emit(gen, g_emit_buf);
}

static void codegen_emit_file(codegen_t* gen, const char* path) {
    if (!gen || !gen->output_handle || !path) return;
    reader_t* reader = reader_open(path);
    if (!reader) {
        cc_error("Failed to open runtime file");
        return;
    }
    int16_t ch = reader_next(reader);
    while (ch >= 0) {
        char c = (char)ch;
        output_write(gen->output_handle, &c, 1);
        ch = reader_next(reader);
    }
    reader_close(reader);
}

static void codegen_emit_stack_adjust(codegen_t* gen, int16_t offset, bool subtract) {
    if (!gen || offset <= 0) return;
    codegen_emit(gen,
        "  ld hl, 0\n"
        "  add hl, sp\n"
        "  ld de, ");
    codegen_emit_hex(gen, (uint16_t)offset);
    codegen_emit(gen, CG_STR_NL);
    codegen_emit(gen, subtract ? CG_STR_OR_A_SBC_HL_DE : CG_STR_ADD_HL_DE);
    codegen_emit(gen, "  ld sp, hl\n");
}

static void codegen_emit_label(codegen_t* gen, const char* label) {
    if (!gen || !label) return;
    codegen_emit_label_name(gen, label);
    codegen_emit(gen, CG_STR_COLON);
}

static void codegen_emit_jump(codegen_t* gen, const char* prefix, const char* label) {
    if (!gen || !prefix || !label) return;
    codegen_emit(gen, prefix);
    codegen_emit_label_name(gen, label);
    codegen_emit(gen, CG_STR_NL);
}

static void codegen_build_scoped_label(codegen_t* gen, const char* label, char* out, uint16_t out_len) {
    uint16_t pos = 0;
    if (!out || out_len == 0) return;
    if (gen && gen->current_function_name && gen->current_function_name[0]) {
        const char* p = gen->current_function_name;
        while (*p && pos + 1 < out_len) {
            out[pos++] = *p++;
        }
        if (pos + 1 < out_len) {
            out[pos++] = '_';
        }
    }
    if (label) {
        const char* p = label;
        while (*p && pos + 1 < out_len) {
            out[pos++] = *p++;
        }
    }
    out[pos] = '\0';
}

static void codegen_loop_push(codegen_t* gen, char* break_label, char* continue_label) {
    if (!gen) return;
    if (gen->loop_depth >= (uint8_t)DIM(gen->loop_break_labels)) {
        cc_error("Loop nesting too deep");
        return;
    }
    gen->loop_break_labels[gen->loop_depth] = break_label;
    gen->loop_continue_labels[gen->loop_depth] = continue_label;
    gen->loop_depth++;
}

static void codegen_loop_pop(codegen_t* gen) {
    if (!gen || gen->loop_depth == 0) return;
    gen->loop_depth--;
    gen->loop_break_labels[gen->loop_depth] = NULL;
    gen->loop_continue_labels[gen->loop_depth] = NULL;
}

static char* codegen_loop_break_label(codegen_t* gen) {
    if (!gen || gen->loop_depth == 0) return NULL;
    return gen->loop_break_labels[gen->loop_depth - 1];
}

static char* codegen_loop_continue_label(codegen_t* gen) {
    if (!gen || gen->loop_depth == 0) return NULL;
    return gen->loop_continue_labels[gen->loop_depth - 1];
}

static void codegen_emit_string_literal(codegen_t* gen, const char* value) {
    if (!gen || !value) return;
    codegen_emit(gen, CG_STR_DM);
    codegen_emit(gen, "\"");
    for (const char* p = value; *p; ++p) {
        char c = *p;
        if (c == '"') {
            codegen_emit(gen, "\\\"");
            continue;
        }
        if (c == '\\') {
            codegen_emit(gen, "\\\\");
            continue;
        }
        if (c == '\n') {
            codegen_emit(gen, "\\n");
            continue;
        }
        g_emit_buf[0] = c;
        g_emit_buf[1] = '\0';
        codegen_emit(gen, g_emit_buf);
    }
    codegen_emit(gen, "\"\n");
}

static int8_t codegen_stream_read_name(ast_reader_t* ast, const char** value) {
    uint16_t index = 0;
    if (!value) return -1;
    index = ast_read_u16(ast->reader);
    *value = ast_reader_string(ast, index);
    return *value ? 0 : -1;
}

static int16_t codegen_local_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (codegen_local_count_t i = 0; i < gen->local_var_count; i++) {
        if (gen->locals[i].name == name || codegen_names_equal(gen->locals[i].name, name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static int16_t codegen_param_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (codegen_param_count_t i = 0; i < gen->param_count; i++) {
        if (gen->params[i].name == name || codegen_names_equal(gen->params[i].name, name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static int16_t codegen_global_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (codegen_global_count_t i = 0; i < gen->global_count; i++) {
        if (gen->globals[i].name == name || codegen_names_equal(gen->globals[i].name, name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static void codegen_record_local(codegen_t* gen, const char* name, uint16_t size,
                                 bool is_16bit, bool is_signed, bool is_pointer,
                                 bool is_array, uint8_t elem_size, bool elem_signed) {
    if (!gen || !name) return;
    if (codegen_local_index(gen, name) >= 0) return;
    if (gen->local_var_count < DIM(gen->locals)) {
        codegen_local_t local;
        local.name = name;
        local.offset = gen->stack_offset;
        local.elem_size = elem_size;
        local.flags = 0;
        if (is_16bit) local.flags |= CG_FLAG_IS_16;
        if (is_signed) local.flags |= CG_FLAG_IS_SIGNED;
        if (is_pointer) local.flags |= CG_FLAG_IS_POINTER;
        if (is_array) local.flags |= CG_FLAG_IS_ARRAY;
        if (elem_signed) local.flags |= CG_FLAG_ELEM_SIGNED;
        mem_cpy(&gen->locals[gen->local_var_count], &local, sizeof(local));
        gen->stack_offset += size;
        gen->local_var_count++;
    }
}

static uint8_t codegen_param_offset(codegen_t* gen, const char* name, int16_t* out_offset) {
    if (!gen || !name || !out_offset) return 0;
    int16_t idx = codegen_param_index(gen, name);
    if (idx < 0) return 0;
    *out_offset = gen->params[idx].offset;
    return 1;
}

static uint8_t codegen_local_offset(codegen_t* gen, const char* name, int16_t* out_offset) {
    if (!gen || !name || !out_offset) return 0;
    int16_t idx = codegen_local_index(gen, name);
    if (idx < 0) return 0;
    *out_offset = gen->locals[idx].offset;
    return 1;
}

static uint8_t codegen_local_or_param_offset(codegen_t* gen, const char* name,
                                             int16_t* out_offset) {
    return codegen_local_offset(gen, name, out_offset) ||
           codegen_param_offset(gen, name, out_offset);
}

static bool codegen_local_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_16);
}

static bool codegen_local_is_signed(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_SIGNED);
}

static bool codegen_local_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_POINTER);
}

static bool codegen_local_is_array(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_ARRAY);
}

static bool codegen_param_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_param_index(gen, name);
    return idx >= 0 && (gen->params[idx].flags & CG_FLAG_IS_16);
}

static bool codegen_param_is_signed(codegen_t* gen, const char* name) {
    int16_t idx = codegen_param_index(gen, name);
    return idx >= 0 && (gen->params[idx].flags & CG_FLAG_IS_SIGNED);
}

static bool codegen_param_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_param_index(gen, name);
    return idx >= 0 && (gen->params[idx].flags & CG_FLAG_IS_POINTER);
}

static bool codegen_global_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_16);
}

static bool codegen_global_is_signed(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_SIGNED);
}

static bool codegen_global_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_POINTER);
}

static bool codegen_global_is_array(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_ARRAY);
}

static bool codegen_name_is_16(codegen_t* gen, const char* name) {
    return codegen_local_is_16(gen, name) ||
           codegen_param_is_16(gen, name) ||
           codegen_global_is_16(gen, name);
}

static bool codegen_name_is_signed(codegen_t* gen, const char* name) {
    return codegen_local_is_signed(gen, name) ||
           codegen_param_is_signed(gen, name) ||
           codegen_global_is_signed(gen, name);
}

static bool codegen_name_is_pointer(codegen_t* gen, const char* name) {
    return codegen_local_is_pointer(gen, name) ||
           codegen_param_is_pointer(gen, name) ||
           codegen_global_is_pointer(gen, name);
}

static bool codegen_name_is_array(codegen_t* gen, const char* name) {
    return codegen_local_is_array(gen, name) ||
           codegen_global_is_array(gen, name);
}

static uint8_t codegen_type_size(uint8_t base, uint8_t depth) {
    base = codegen_base_type(base);
    if (depth > 0) return 2;
    if (base == AST_BASE_CHAR) return 1;
    if (base == AST_BASE_INT) return 2;
    return 0;
}

static uint8_t codegen_pointer_elem_size(uint8_t base, uint8_t depth) {
    base = codegen_base_type(base);
    if (depth == 0) return 0;
    return codegen_type_size(base, (uint8_t)(depth - 1));
}

static uint8_t codegen_array_elem_size_by_name(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    if (idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_ARRAY)) {
        return gen->locals[idx].elem_size;
    }
    idx = codegen_global_index(gen, name);
    if (idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_ARRAY)) {
        return gen->globals[idx].elem_size;
    }
    return 0;
}

static bool codegen_array_elem_signed_by_name(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    if (idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_ARRAY)) {
        return (gen->locals[idx].flags & CG_FLAG_ELEM_SIGNED) != 0;
    }
    idx = codegen_global_index(gen, name);
    if (idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_ARRAY)) {
        return (gen->globals[idx].flags & CG_FLAG_ELEM_SIGNED) != 0;
    }
    return false;
}

static uint8_t codegen_pointer_elem_size_by_name(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    if (idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_POINTER)) {
        return gen->locals[idx].elem_size;
    }
    idx = codegen_param_index(gen, name);
    if (idx >= 0 && (gen->params[idx].flags & CG_FLAG_IS_POINTER)) {
        return gen->params[idx].elem_size;
    }
    idx = codegen_global_index(gen, name);
    if (idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_POINTER)) {
        return gen->globals[idx].elem_size;
    }
    return 0;
}

static bool codegen_pointer_elem_signed_by_name(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    if (idx >= 0 && (gen->locals[idx].flags & CG_FLAG_IS_POINTER)) {
        return (gen->locals[idx].flags & CG_FLAG_ELEM_SIGNED) != 0;
    }
    idx = codegen_param_index(gen, name);
    if (idx >= 0 && (gen->params[idx].flags & CG_FLAG_IS_POINTER)) {
        return (gen->params[idx].flags & CG_FLAG_ELEM_SIGNED) != 0;
    }
    idx = codegen_global_index(gen, name);
    if (idx >= 0 && (gen->globals[idx].flags & CG_FLAG_IS_POINTER)) {
        return (gen->globals[idx].flags & CG_FLAG_ELEM_SIGNED) != 0;
    }
    return false;
}

static const char* codegen_get_string_label(codegen_t* gen, const char* value);

static uint8_t codegen_peek_array_elem_size(codegen_t* gen, ast_reader_t* ast) {
    uint8_t base_tag = 0;
    uint8_t index_tag = 0;
    uint8_t elem_size = 1;
    base_tag = ast_read_u8(ast->reader);
    if (base_tag == AST_TAG_STRING_LITERAL) {
        const char* base_string = NULL;
        if (codegen_stream_read_name(ast, &base_string) < 0) return 0;
    } else if (base_tag == AST_TAG_IDENTIFIER) {
        const char* base_name = NULL;
        if (codegen_stream_read_name(ast, &base_name) < 0) return 0;
        if (codegen_name_is_array(gen, base_name)) {
            elem_size = codegen_array_elem_size_by_name(gen, base_name);
        } else if (codegen_name_is_pointer(gen, base_name)) {
            elem_size = codegen_pointer_elem_size_by_name(gen, base_name);
        } else {
            elem_size = 0;
        }
    } else {
        if (ast_reader_skip_tag(ast, base_tag) < 0) return 0;
    }
    index_tag = ast_read_u8(ast->reader);
    if (ast_reader_skip_tag(ast, index_tag) < 0) return 0;
    return elem_size;
}

static cc_error_t codegen_emit_address_of_identifier(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_or_param_offset(gen, name, &offset)) {
        codegen_emit(gen, CG_STR_PUSH_IX_POP_HL);
        if (offset != 0) {
            codegen_emit(gen, "  ld bc, ");
            codegen_emit_hex(gen, (uint16_t)offset);
            codegen_emit(gen,
                "\n"
                "  add hl, bc\n");
        }
        return CC_OK;
    }

    codegen_emit(gen, CG_STR_LD_HL);
    codegen_emit_mangled_var(gen, name);
    codegen_emit(gen, CG_STR_NL);
    return CC_OK;
}

static cc_error_t codegen_load_pointer_to_hl(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_or_param_offset(gen, name, &offset)) {
        codegen_emit(gen, "  ld l, (ix");
        codegen_emit_ix_offset(gen, offset);
        codegen_emit(gen,
            ")\n"
            "  ld h, (ix");
        codegen_emit_ix_offset(gen, offset + 1);
        codegen_emit(gen, ")\n");
        return CC_OK;
    }

    codegen_emit(gen, CG_STR_LD_HL_PAREN);
    codegen_emit_mangled_var(gen, name);
    codegen_emit(gen, CG_STR_RPAREN_NL);
    return CC_OK;
}

static cc_error_t codegen_store_pointer_from_hl(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_or_param_offset(gen, name, &offset)) {
        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
        codegen_emit_ix_offset(gen, offset);
        codegen_emit(gen, CG_STR_RPAREN_L);
        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
        codegen_emit_ix_offset(gen, offset + 1);
        codegen_emit(gen, CG_STR_RPAREN_H);
        return CC_OK;
    }

    codegen_emit(gen, CG_STR_LD_LPAREN);
    codegen_emit_mangled_var(gen, name);
    codegen_emit(gen, CG_STR_RPAREN_HL);
    return CC_OK;
}

static cc_error_t codegen_store_a_to_identifier(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_or_param_offset(gen, name, &offset)) {
        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
        codegen_emit_ix_offset(gen, offset);
        codegen_emit(gen, CG_STR_RPAREN_A);
        return CC_OK;
    }
    codegen_emit(gen, CG_STR_LD_LPAREN);
    codegen_emit_mangled_var(gen, name);
    codegen_emit(gen, CG_STR_RPAREN_A);
    return CC_OK;
}

static cc_error_t codegen_load_array_base_to_hl(codegen_t* gen,
                                                const char* base_string,
                                                const char* base_name) {
    if (base_string) {
        const char* label = codegen_get_string_label(gen, base_string);
        if (!label) return CC_ERROR_CODEGEN;
        codegen_emit(gen, CG_STR_LD_HL);
        codegen_emit(gen, label);
        codegen_emit(gen, CG_STR_NL);
        return CC_OK;
    }
    if (base_name) {
        if (codegen_name_is_array(gen, base_name)) {
            return codegen_emit_address_of_identifier(gen, base_name);
        }
        if (!codegen_name_is_pointer(gen, base_name)) {
#if !CC_TRUST_SEMANTIC
            cc_error(CG_MSG_UNSUPPORTED_ARRAY_ACCESS);
#endif
            return CC_ERROR_CODEGEN;
        }
        return codegen_load_pointer_to_hl(gen, base_name);
    }
#if !CC_TRUST_SEMANTIC
    cc_error(CG_MSG_UNSUPPORTED_ARRAY_ACCESS);
#endif
    return CC_ERROR_CODEGEN;
}

static cc_error_t codegen_emit_array_address(codegen_t* gen, ast_reader_t* ast,
                                             uint8_t* out_elem_size,
                                             bool* out_elem_signed) {
    uint8_t base_tag = 0;
    uint8_t index_tag = 0;
    const char* base_name = NULL;
    const char* base_string = NULL;

    base_tag = ast_read_u8(ast->reader);
    if (base_tag == AST_TAG_STRING_LITERAL) {
        if (codegen_stream_read_name(ast, &base_string) < 0) return CC_ERROR_CODEGEN;
    } else if (base_tag == AST_TAG_IDENTIFIER) {
        if (codegen_stream_read_name(ast, &base_name) < 0) return CC_ERROR_CODEGEN;
    } else {
        if (ast_reader_skip_tag(ast, base_tag) < 0) return CC_ERROR_CODEGEN;
#if !CC_TRUST_SEMANTIC
        cc_error(CG_MSG_UNSUPPORTED_ARRAY_ACCESS);
#endif
        return CC_ERROR_CODEGEN;
    }
    index_tag = ast_read_u8(ast->reader);

    uint8_t elem_size = 1;
    bool elem_signed = false;
    if (base_name) {
        if (codegen_name_is_array(gen, base_name)) {
            elem_size = codegen_array_elem_size_by_name(gen, base_name);
            elem_signed = codegen_array_elem_signed_by_name(gen, base_name);
        } else if (codegen_name_is_pointer(gen, base_name)) {
            elem_size = codegen_pointer_elem_size_by_name(gen, base_name);
            elem_signed = codegen_pointer_elem_signed_by_name(gen, base_name);
        } else {
#if !CC_TRUST_SEMANTIC
            cc_error(CG_MSG_UNSUPPORTED_ARRAY_ACCESS);
#endif
            return CC_ERROR_CODEGEN;
        }
        if (elem_size == 0) {
#if !CC_TRUST_SEMANTIC
            cc_error("Unsupported array element size");
#endif
            return CC_ERROR_CODEGEN;
        }
    }

    cc_error_t err = codegen_stream_expression_tag(gen, ast, index_tag);
    if (err != CC_OK) return err;
    codegen_result_to_a(gen);
    if (elem_size == 2) {
        codegen_emit(gen, "  add a, a\n");
    }
    codegen_emit(gen,
        "  ld e, a\n"
        "  ld d, 0\n");

    err = codegen_load_array_base_to_hl(gen, base_string, base_name);
    if (err != CC_OK) return err;
    codegen_emit(gen, CG_STR_ADD_HL_DE);

    if (out_elem_size) {
        *out_elem_size = elem_size;
    }
    if (out_elem_signed) {
        *out_elem_signed = elem_signed;
    }
    return CC_OK;
}

static void codegen_emit_ix_offset(codegen_t* gen, int16_t offset) {
    if (offset < 0) {
        codegen_emit(gen, "-");
        codegen_emit_hex(gen, (uint8_t)(-offset));
        return;
    }
    codegen_emit(gen, "+");
    codegen_emit_hex(gen, (uint8_t)offset);
}

static char* codegen_new_label_persist(codegen_t* gen) {
    char* tmp = codegen_new_label(gen);
    char* out = cc_strdup(tmp);
    return out ? out : tmp;
}

static const char* codegen_get_string_label(codegen_t* gen, const char* value) {
    if (!gen || !value) return NULL;
    for (codegen_string_count_t i = 0; i < gen->string_count; i++) {
        if (gen->string_literals[i] && str_cmp(gen->string_literals[i], value) == 0) {
            return gen->string_labels[i];
        }
    }
    if (gen->string_count >= DIM(gen->string_labels)) {
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

codegen_t* codegen_create(const char* output_file) {
    codegen_t* gen = &g_codegen;
    mem_set(gen, 0, sizeof(*gen));

    gen->output_handle = output_open(output_file);
#ifdef __SDCC
    if (gen->output_handle < 0) {
#else
    if (!gen->output_handle) {
#endif
        cc_free(gen);
        return NULL;
    }

    return gen;
}

void codegen_destroy(codegen_t* gen) {
    if (!gen) return;
    if (gen->output_handle) {
        output_close(gen->output_handle);
    }
    for (codegen_string_count_t i = 0; i < gen->string_count; i++) {
        if (gen->string_labels[i]) {
            cc_free((void*)gen->string_labels[i]);
        }
        if (gen->string_literals[i]) {
            cc_free(gen->string_literals[i]);
        }
    }
}

void codegen_emit(codegen_t* gen, const char* fmt) {
    if (!gen || !gen->output_handle || !fmt) return;
    uint16_t len = 0;
    const char* p = fmt;
    while (p[len]) len++;
    if (len > 0) {
        output_write(gen->output_handle, fmt, len);
    }
}

char* codegen_new_label(codegen_t* gen) {
    static char labels[8][16];
    static uint8_t slot = 0;
    return codegen_format_label(labels, &slot, 'l', gen->label_counter++);
}

char* codegen_new_string_label(codegen_t* gen) {
    static char labels[8][16];
    static uint8_t slot = 0;
    return codegen_format_label(labels, &slot, 's', (uint16_t)gen->string_count);
}

static bool codegen_function_return_is_16bit(codegen_t* gen, uint16_t name_index);

static bool codegen_op_is_compare(uint8_t op) {
    return op == OP_EQ || op == OP_NE || op == OP_LT ||
           op == OP_GT || op == OP_LE || op == OP_GE;
}

static void codegen_emit_compare(codegen_t* gen, const char* jump1, const char* jump2,
                                 bool output_in_hl, bool use_set_label) {
    char* end = codegen_new_label(gen);
    codegen_emit(gen, output_in_hl ? CG_STR_LD_HL_ZERO : CG_STR_LD_A_ZERO);
    if (use_set_label) {
        char* set = codegen_new_label(gen);
        if (jump1) {
            codegen_emit_jump(gen, jump1, set);
        }
        if (jump2) {
            codegen_emit_jump(gen, jump2, set);
        }
        codegen_emit_jump(gen, CG_STR_JR, end);
        codegen_emit_label(gen, set);
    } else {
        if (jump1) {
            codegen_emit_jump(gen, jump1, end);
        }
        if (jump2) {
            codegen_emit_jump(gen, jump2, end);
        }
    }
    codegen_emit(gen, output_in_hl ? "  ld hl, 1\n" : CG_STR_LD_A_ONE);
    codegen_emit_label(gen, end);
}

static bool codegen_emit_compare_table(codegen_t* gen, uint8_t op,
                                       const compare_entry_t* table, uint8_t count,
                                       bool output_in_hl);

static bool codegen_emit_op_table(codegen_t* gen, uint8_t op,
                                  const op_emit_entry_t* table, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (table[i].op != op) {
            continue;
        }
        codegen_emit(gen, table[i].seq);
        return true;
    }
    return false;
}

static bool codegen_emit_compare_table(codegen_t* gen, uint8_t op,
                                       const compare_entry_t* table, uint8_t count,
                                       bool output_in_hl) {
    for (uint8_t i = 0; i < count; i++) {
        if (table[i].op != op) {
            continue;
        }
        if (table[i].prelude) {
            codegen_emit(gen, table[i].prelude);
        }
        codegen_emit_compare(gen, table[i].jump1, table[i].jump2, output_in_hl, true);
        return true;
    }
    return false;
}

static cc_error_t codegen_emit_binary_op_hl(codegen_t* gen, ast_reader_t* ast, uint8_t op,
                                            uint8_t left_tag, bool output_in_hl) {
    cc_error_t err = codegen_stream_expression_expect(gen, ast, left_tag, true);
    if (err != CC_OK) return err;
    codegen_result_to_hl(gen);
    codegen_emit(gen, CG_STR_PUSH_HL);
    uint8_t right_tag = 0;
    right_tag = ast_read_u8(ast->reader);
    err = codegen_stream_expression_expect(gen, ast, right_tag, true);
    if (err != CC_OK) return err;
    codegen_result_to_hl(gen);
    codegen_emit(gen, "  pop de\n");
    if (codegen_op_is_compare(op)) {
        static const compare_entry_t compare16_table[] = {
            { OP_EQ, NULL, CG_STR_JR_Z,  NULL },
            { OP_NE, NULL, CG_STR_JR_NZ, NULL },
            { OP_LT, NULL, CG_STR_JR_C,  NULL },
            { OP_LE, NULL, CG_STR_JR_Z,  CG_STR_JR_C },
            { OP_GE, NULL, CG_STR_JR_NC, NULL },
        };
        codegen_emit(gen, CG_STR_EX_DE_HL_OR_A_SBC_HL_DE);
        if (codegen_emit_compare_table(gen, op, compare16_table, (uint8_t)DIM(compare16_table), output_in_hl)) {
            g_result_in_hl = output_in_hl;
            return CC_OK;
        }
        if (op == OP_GT) {
            codegen_emit_compare(gen, CG_STR_JR_Z, CG_STR_JR_C, output_in_hl, false);
            g_result_in_hl = output_in_hl;
            return CC_OK;
        }
        return CC_ERROR_CODEGEN;
    }
    if (op == OP_AND || op == OP_OR || op == OP_XOR) {
        if (op == OP_AND) {
            codegen_emit(gen,
                "  ld a, h\n"
                "  and d\n"
                "  ld h, a\n"
                "  ld a, l\n"
                "  and e\n"
                "  ld l, a\n");
        } else if (op == OP_OR) {
            codegen_emit(gen,
                "  ld a, h\n"
                "  or d\n"
                "  ld h, a\n"
                "  ld a, l\n"
                "  or e\n"
                "  ld l, a\n");
        } else {
            codegen_emit(gen,
                "  ld a, h\n"
                "  or d\n"
                "  ld b, a\n"
                "  ld a, h\n"
                "  and d\n"
                "  cpl\n"
                "  and b\n"
                "  ld h, a\n"
                "  ld a, l\n"
                "  or e\n"
                "  ld b, a\n"
                "  ld a, l\n"
                "  and e\n"
                "  cpl\n"
                "  and b\n"
                "  ld l, a\n");
        }
        g_result_in_hl = true;
        return CC_OK;
    }
    if (op == OP_SHL || op == OP_SHR) {
        char* loop_label = codegen_new_label(gen);
        char* end_label = codegen_new_label(gen);
        codegen_emit(gen, "  ex de, hl\n");
        codegen_emit(gen,
            "  ld b, e\n"
            "  ld a, b\n"
            "  or a\n");
        codegen_emit_jump(gen, CG_STR_JR_Z, end_label);
        codegen_emit_label(gen, loop_label);
        if (op == OP_SHL) {
            codegen_emit(gen, "  add hl, hl\n");
        } else {
            codegen_emit(gen,
                "  ld a, h\n"
                "  or a\n"
                "  rra\n"
                "  ld h, a\n"
                "  ld a, l\n"
                "  rra\n"
                "  ld l, a\n");
        }
        codegen_emit(gen, "  djnz ");
        codegen_emit_label_name(gen, loop_label);
        codegen_emit(gen, CG_STR_NL);
        codegen_emit_label(gen, end_label);
        g_result_in_hl = true;
        return CC_OK;
    }
    {
        static const op_emit_entry_t op16_table[] = {
            { OP_ADD, "  add hl, de\n" },
            { OP_SUB,
                "  ex de, hl\n"
                "  or a\n"
                "  sbc hl, de\n" },
            { OP_MUL,
                "  ex de, hl\n"
                "  call __mul_hl_de\n" },
            { OP_DIV,
                "  ex de, hl\n"
                "  call __div_hl_de\n" },
            { OP_MOD,
                "  ex de, hl\n"
                "  call __mod_hl_de\n" },
        };
        if (!codegen_emit_op_table(gen, op, op16_table, (uint8_t)DIM(op16_table))) {
            return CC_ERROR_CODEGEN;
        }
    }
    g_result_in_hl = true;
    return CC_OK;
}

static cc_error_t codegen_emit_binary_op_a(codegen_t* gen, ast_reader_t* ast, uint8_t op,
                                           uint8_t left_tag) {
    cc_error_t err = codegen_stream_expression_tag(gen, ast, left_tag);
    if (err != CC_OK) return err;
    codegen_emit(gen, CG_STR_PUSH_AF);
    uint8_t right_tag = 0;
    right_tag = ast_read_u8(ast->reader);
    err = codegen_stream_expression_tag(gen, ast, right_tag);
    if (err != CC_OK) return err;
    codegen_emit(gen, CG_STR_LD_L_A_POP_AF);
    if (codegen_op_is_compare(op)) {
        static const compare_entry_t compare8_table[] = {
            { OP_EQ, "  cp l\n", CG_STR_JR_Z,  NULL },
            { OP_NE, "  cp l\n", CG_STR_JR_NZ, NULL },
            { OP_LT, "  cp l\n", CG_STR_JR_C,  NULL },
            { OP_LE, "  sub l\n", CG_STR_JR_Z, CG_STR_JR_C },
            { OP_GE, "  cp l\n", CG_STR_JR_NC, NULL },
        };
        if (codegen_emit_compare_table(gen, op, compare8_table, (uint8_t)DIM(compare8_table), false)) {
            g_result_in_hl = false;
            return CC_OK;
        }
        if (op == OP_GT) {
            codegen_emit(gen, "  sub l\n");
            codegen_emit_compare(gen, CG_STR_JR_Z, CG_STR_JR_C, false, false);
            g_result_in_hl = false;
            return CC_OK;
        }
        return CC_ERROR_CODEGEN;
    }
    if (op == OP_AND || op == OP_OR || op == OP_XOR) {
        if (op == OP_AND) {
            codegen_emit(gen, "  and l\n");
        } else if (op == OP_OR) {
            codegen_emit(gen, "  or l\n");
        } else {
            codegen_emit(gen,
                "  ld b, a\n"
                "  or l\n"
                "  ld c, a\n"
                "  ld a, b\n"
                "  and l\n"
                "  cpl\n"
                "  and c\n");
        }
        g_result_in_hl = false;
        return CC_OK;
    }
    if (op == OP_SHL || op == OP_SHR) {
        char* loop_label = codegen_new_label(gen);
        char* zero_label = codegen_new_label(gen);
        char* end_label = codegen_new_label(gen);
        codegen_emit(gen,
            "  ld b, l\n"
            "  ld c, a\n"
            "  ld a, b\n"
            "  or a\n");
        codegen_emit_jump(gen, CG_STR_JR_Z, zero_label);
        codegen_emit(gen, "  ld a, c\n");
        codegen_emit_label(gen, loop_label);
        if (op == OP_SHL) {
            codegen_emit(gen, "  add a, a\n");
        } else {
            codegen_emit(gen,
                "  or a\n"
                "  rra\n");
        }
        codegen_emit(gen, "  djnz ");
        codegen_emit_label_name(gen, loop_label);
        codegen_emit(gen, CG_STR_NL);
        codegen_emit_jump(gen, CG_STR_JR, end_label);
        codegen_emit_label(gen, zero_label);
        codegen_emit(gen, "  ld a, c\n");
        codegen_emit_label(gen, end_label);
        g_result_in_hl = false;
        return CC_OK;
    }
    {
        static const op_emit_entry_t op8_table[] = {
            { OP_ADD, "  add a, l\n" },
            { OP_SUB, "  sub l\n" },
            { OP_MUL, "  call __mul_a_l\n" },
            { OP_DIV, "  call __div_a_l\n" },
            { OP_MOD, "  call __mod_a_l\n" },
        };
        if (!codegen_emit_op_table(gen, op, op8_table, (uint8_t)DIM(op8_table))) {
            return CC_ERROR_CODEGEN;
        }
    }
    g_result_in_hl = false;
    return CC_OK;
}

static cc_error_t codegen_read_and_stream_statement(codegen_t* gen, ast_reader_t* ast) {
    uint8_t tag = 0;
    tag = ast_read_u8(ast->reader);
    return codegen_stream_statement_tag(gen, ast, tag);
}

static cc_error_t codegen_read_and_stream_expression(codegen_t* gen, ast_reader_t* ast) {
    uint8_t tag = 0;
    tag = ast_read_u8(ast->reader);
    return codegen_stream_expression_tag(gen, ast, tag);
}

static cc_error_t codegen_stream_expression_expect(codegen_t* gen, ast_reader_t* ast,
                                                   uint8_t tag, bool expect_hl) {
    bool prev_expect = g_expect_result_in_hl;
    g_expect_result_in_hl = expect_hl;
    cc_error_t err = codegen_stream_expression_tag(gen, ast, tag);
    g_expect_result_in_hl = prev_expect;
    return err;
}

static cc_error_t codegen_statement_return(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    uint8_t has_expr = 0;
    has_expr = ast_read_u8(ast->reader);
    if (has_expr) {
        uint8_t expr_tag = 0;
        expr_tag = ast_read_u8(ast->reader);
        bool expect_hl = gen->function_return_is_16 &&
                         codegen_tag_is_simple_expr(expr_tag);
        cc_error_t err = codegen_stream_expression_expect(gen, ast, expr_tag, expect_hl);
        if (err != CC_OK) return err;
        if (gen->function_return_is_16) {
            codegen_result_to_hl(gen);
        } else {
            codegen_result_to_a(gen);
        }
    } else {
        bool is_16bit = gen->function_return_is_16;
        codegen_emit(gen, is_16bit ? CG_STR_LD_HL_ZERO : CG_STR_LD_A_ZERO);
        g_result_in_hl = is_16bit;
    }
    codegen_emit_jump(gen, CG_STR_JP, gen->function_end_label);
    return CC_OK;
}

static cc_error_t codegen_statement_break(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)ast;
    (void)tag;
    char* label = codegen_loop_break_label(gen);
#if !CC_TRUST_SEMANTIC
    if (!label) {
        cc_error("break used outside of loop");
        return CC_ERROR_CODEGEN;
    }
#endif
    codegen_emit_jump(gen, CG_STR_JP, label);
    return CC_OK;
}

static cc_error_t codegen_statement_continue(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)ast;
    (void)tag;
    char* label = codegen_loop_continue_label(gen);
#if !CC_TRUST_SEMANTIC
    if (!label) {
        cc_error("continue used outside of loop");
        return CC_ERROR_CODEGEN;
    }
#endif
    codegen_emit_jump(gen, CG_STR_JP, label);
    return CC_OK;
}

static cc_error_t codegen_statement_goto(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    const char* name = NULL;
    char scoped[64];
    if (codegen_stream_read_name(ast, &name) < 0) return CC_ERROR_CODEGEN;
    codegen_build_scoped_label(gen, name, scoped, (uint16_t)sizeof(scoped));
    codegen_emit_jump(gen, CG_STR_JP, scoped);
    return CC_OK;
}

static cc_error_t codegen_statement_label(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    const char* name = NULL;
    char scoped[64];
    if (codegen_stream_read_name(ast, &name) < 0) return CC_ERROR_CODEGEN;
    codegen_build_scoped_label(gen, name, scoped, (uint16_t)sizeof(scoped));
    codegen_emit_label(gen, scoped);
    return CC_OK;
}

static cc_error_t codegen_statement_var_decl(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    uint16_t name_index = 0;
    uint8_t has_init = 0;
    uint8_t base = 0;
    uint8_t depth = 0;
    uint16_t array_len = 0;
    const char* name = NULL;
    name_index = ast_read_u16(ast->reader);
    if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
    (void)array_len;
    has_init = ast_read_u8(ast->reader);
    name = ast_reader_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;
    (void)name;
    if (array_len > 0) {
        if (has_init) {
            if (ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
#if !CC_TRUST_SEMANTIC
            cc_error(CG_MSG_ARRAY_INIT_NOT_SUPPORTED);
#endif
            return CC_ERROR_CODEGEN;
        }
        return CC_OK;
    }
    if (has_init) {
        uint8_t init_tag = 0;
        init_tag = ast_read_u8(ast->reader);
        bool is_pointer = depth > 0;
        if (is_pointer) {
            if (init_tag == AST_TAG_STRING_LITERAL) {
                const char* init_str = NULL;
                if (codegen_stream_read_name(ast, &init_str) < 0) return CC_ERROR_CODEGEN;
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
                op = ast_read_u8(ast->reader);
                operand_tag = ast_read_u8(ast->reader);
                if (op == OP_ADDR && operand_tag == AST_TAG_IDENTIFIER) {
                    const char* ident = NULL;
                    if (codegen_stream_read_name(ast, &ident) < 0) return CC_ERROR_CODEGEN;
                    cc_error_t err = codegen_emit_address_of_identifier(gen, ident);
                    if (err != CC_OK) return err;
                    return codegen_store_pointer_from_hl(gen, name);
                }
                ast_reader_skip_tag(ast, operand_tag);
                return CC_ERROR_CODEGEN;
            }
            if (init_tag == AST_TAG_IDENTIFIER) {
                const char* ident = NULL;
                if (codegen_stream_read_name(ast, &ident) < 0) return CC_ERROR_CODEGEN;
                if (codegen_name_is_16(gen, ident)) {
                    cc_error_t err = codegen_load_pointer_to_hl(gen, ident);
                    if (err != CC_OK) return err;
                    return codegen_store_pointer_from_hl(gen, name);
                }
                return CC_ERROR_CODEGEN;
            }
            if (init_tag == AST_TAG_CONSTANT) {
                int16_t val = 0;
                val = ast_read_i16(ast->reader);
                if (val == 0) {
                    codegen_emit(gen, CG_STR_LD_HL_ZERO);
                    return codegen_store_pointer_from_hl(gen, name);
                }
                return CC_ERROR_CODEGEN;
            }
            ast_reader_skip_tag(ast, init_tag);
            return CC_ERROR_CODEGEN;
        }
        bool is_16bit = codegen_stream_type_is_16bit(base, depth);
        bool expect_hl = is_16bit &&
                         codegen_tag_is_simple_expr(init_tag);
        cc_error_t err = codegen_stream_expression_expect(gen, ast, init_tag, expect_hl);
        if (err != CC_OK) return err;
        if (is_16bit) {
            codegen_result_to_hl(gen);
            return codegen_store_pointer_from_hl(gen, name);
        } else {
            return codegen_store_a_to_identifier(gen, name);
        }
    }
    return CC_OK;
}

static cc_error_t codegen_statement_compound(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    uint16_t stmt_count = 0;
    stmt_count = ast_read_u16(ast->reader);
    for (uint16_t i = 0; i < stmt_count; i++) {
        cc_error_t err = codegen_read_and_stream_statement(gen, ast);
        if (err != CC_OK) return err;
    }
    return CC_OK;
}

static cc_error_t codegen_statement_if(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    uint8_t has_else = 0;
    char* else_label = NULL;
    char* end_label = NULL;
    has_else = ast_read_u8(ast->reader);
    cc_error_t err = codegen_read_and_stream_expression(gen, ast);
    if (err != CC_OK) return err;
    else_label = codegen_new_label_persist(gen);
    if (has_else) {
        end_label = codegen_new_label_persist(gen);
    } else {
        end_label = else_label;
    }
    codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, else_label);
    err = codegen_read_and_stream_statement(gen, ast);
    if (err != CC_OK) {
        goto if_cleanup;
    }
    if (has_else) {
        codegen_emit_jump(gen, CG_STR_JP, end_label);
    }
    codegen_emit_label(gen, else_label);
    if (has_else) {
        err = codegen_read_and_stream_statement(gen, ast);
        if (err != CC_OK) {
            goto if_cleanup;
        }
        codegen_emit_label(gen, end_label);
    }
    err = CC_OK;
if_cleanup:
    if (else_label) cc_free(else_label);
    if (has_else && end_label) cc_free(end_label);
    return err;
}

static cc_error_t codegen_statement_while(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    char* loop_label = codegen_new_label_persist(gen);
    char* end_label = codegen_new_label_persist(gen);
    cc_error_t err = CC_OK;
    codegen_emit_label(gen, loop_label);
    err = codegen_read_and_stream_expression(gen, ast);
    if (err != CC_OK) {
        goto while_cleanup;
    }
    codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
    codegen_loop_push(gen, end_label, loop_label);
    err = codegen_read_and_stream_statement(gen, ast);
    codegen_loop_pop(gen);
    if (err != CC_OK) {
        goto while_cleanup;
    }
    codegen_emit_jump(gen, CG_STR_JP, loop_label);
    codegen_emit_label(gen, end_label);
    err = CC_OK;
while_cleanup:
    if (loop_label) cc_free(loop_label);
    if (end_label) cc_free(end_label);
    return err;
}

static cc_error_t codegen_statement_for(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    (void)tag;
    uint8_t has_init = 0;
    uint8_t has_cond = 0;
    uint8_t has_inc = 0;
    uint32_t inc_offset = 0;
    has_init = ast_read_u8(ast->reader);
    has_cond = ast_read_u8(ast->reader);
    has_inc = ast_read_u8(ast->reader);
    char* loop_label = codegen_new_label_persist(gen);
    char* end_label = codegen_new_label_persist(gen);
    char* inc_label = has_inc ? codegen_new_label_persist(gen) : NULL;
    cc_error_t err;
    if (has_init) {
        err = codegen_read_and_stream_statement(gen, ast);
        if (err != CC_OK) {
            goto for_cleanup;
        }
    }
    codegen_emit_label(gen, loop_label);
    if (has_cond) {
        err = codegen_read_and_stream_expression(gen, ast);
        if (err != CC_OK) {
            goto for_cleanup;
        }
        codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
    }
    if (has_inc) {
        inc_offset = reader_tell(ast->reader);
        if (ast_reader_skip_node(ast) < 0) {
            err = CC_ERROR_CODEGEN;
            goto for_cleanup;
        }
    }
    codegen_loop_push(gen, end_label, inc_label ? inc_label : loop_label);
    err = codegen_read_and_stream_statement(gen, ast);
    codegen_loop_pop(gen);
    if (err != CC_OK) {
        goto for_cleanup;
    }
    if (has_inc) {
        uint32_t body_end = reader_tell(ast->reader);
        if (reader_seek(ast->reader, inc_offset) < 0) {
            err = CC_ERROR_CODEGEN;
            goto for_cleanup;
        }
        codegen_emit_label(gen, inc_label);
        err = codegen_read_and_stream_expression(gen, ast);
        if (err != CC_OK) {
            goto for_cleanup;
        }
        if (reader_seek(ast->reader, body_end) < 0) {
            err = CC_ERROR_CODEGEN;
            goto for_cleanup;
        }
    }
    codegen_emit_jump(gen, CG_STR_JP, loop_label);
    codegen_emit_label(gen, end_label);
    err = CC_OK;
for_cleanup:
    if (loop_label) cc_free(loop_label);
    if (end_label) cc_free(end_label);
    if (inc_label) cc_free(inc_label);
    return err;
}

static bool codegen_expression_is_16bit_at(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    switch (tag) {
        case AST_TAG_CONSTANT: {
            int16_t value = 0;
            value = ast_read_i16(ast->reader);
            return value < 0 || value > 0xFF;
        }
        case AST_TAG_IDENTIFIER: {
            const char* name = NULL;
            if (codegen_stream_read_name(ast, &name) < 0) return false;
            return codegen_name_is_16(gen, name) || codegen_name_is_array(gen, name);
        }
        case AST_TAG_UNARY_OP: {
            uint8_t op = 0;
            uint8_t child_tag = 0;
            op = ast_read_u8(ast->reader);
            child_tag = ast_read_u8(ast->reader);
            if (op == OP_DEREF) {
                if (ast_reader_skip_tag(ast, child_tag) < 0) return false;
                return false;
            }
            if (op == OP_ADDR) {
                if (ast_reader_skip_tag(ast, child_tag) < 0) return false;
                return true;
            }
            {
                bool child_is_16 = codegen_expression_is_16bit_at(gen, ast, child_tag);
                if (op == OP_LNOT) {
                    return false;
                }
                return child_is_16;
            }
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = 0;
            uint8_t left_tag = 0;
            uint8_t right_tag = 0;
            op = ast_read_u8(ast->reader);
            left_tag = ast_read_u8(ast->reader);
            bool left_is_16 = codegen_expression_is_16bit_at(gen, ast, left_tag);
            right_tag = ast_read_u8(ast->reader);
            bool right_is_16 = codegen_expression_is_16bit_at(gen, ast, right_tag);
            if (codegen_op_is_compare(op) || op == OP_LAND || op == OP_LOR) {
                return false;
            }
            return left_is_16 || right_is_16;
        }
        case AST_TAG_CALL: {
            uint16_t name_index = 0;
            uint8_t arg_count = 0;
            name_index = ast_read_u16(ast->reader);
            arg_count = ast_read_u8(ast->reader);
            for (uint8_t i = 0; i < arg_count; i++) {
                uint8_t arg_tag = 0;
                arg_tag = ast_read_u8(ast->reader);
                if (ast_reader_skip_tag(ast, arg_tag) < 0) return false;
            }
            return codegen_function_return_is_16bit(gen, name_index);
        }
        case AST_TAG_ARRAY_ACCESS: {
            uint8_t elem_size = codegen_peek_array_elem_size(gen, ast);
            return elem_size == 2;
        }
        case AST_TAG_ASSIGN: {
            uint8_t ltag = 0;
            uint8_t rtag = 0;
            bool lvalue_is_16 = false;
            ltag = ast_read_u8(ast->reader);
            if (ltag == AST_TAG_ARRAY_ACCESS) {
                uint8_t elem_size = codegen_peek_array_elem_size(gen, ast);
                lvalue_is_16 = elem_size == 2;
            } else if (ltag == AST_TAG_IDENTIFIER) {
                const char* lvalue_name = NULL;
                if (codegen_stream_read_name(ast, &lvalue_name) < 0) return false;
                lvalue_is_16 = codegen_name_is_16(gen, lvalue_name);
            } else {
                if (ast_reader_skip_tag(ast, ltag) < 0) return false;
            }
            rtag = ast_read_u8(ast->reader);
            if (ast_reader_skip_tag(ast, rtag) < 0) return false;
            return lvalue_is_16;
        }
        case AST_TAG_STRING_LITERAL: {
            (void)ast_read_u16(ast->reader);
            return true;
        }
        default:
            if (ast_reader_skip_tag(ast, tag) < 0) return false;
            return false;
    }
}

static bool codegen_stream_type_is_16bit(uint8_t base, uint8_t depth) {
    base = codegen_base_type(base);
    if (depth > 0) return true;
    return base == AST_BASE_INT;
}

static bool codegen_function_return_is_16bit(codegen_t* gen, uint16_t name_index) {
    if (!gen) return false;
    for (codegen_function_count_t i = 0; i < gen->function_count; i++) {
        uint16_t flags = gen->function_return_flags[i];
        if ((uint16_t)(flags & 0x7FFFu) == name_index) {
            return (flags & 0x8000u) != 0;
        }
    }
    return false;
}

static void codegen_register_function_return(codegen_t* gen, uint16_t name_index, bool is_16bit) {
    if (!gen) return;
    for (codegen_function_count_t i = 0; i < gen->function_count; i++) {
        if ((uint16_t)(gen->function_return_flags[i] & 0x7FFFu) == name_index) {
            gen->function_return_flags[i] =
                (uint16_t)((name_index & 0x7FFFu) | (is_16bit ? 0x8000u : 0));
            return;
        }
    }
    if (gen->function_count >= DIM(gen->function_return_flags)) {
        return;
    }
    gen->function_return_flags[gen->function_count] =
        (uint16_t)((name_index & 0x7FFFu) | (is_16bit ? 0x8000u : 0));
    gen->function_count++;
}

static cc_error_t codegen_stream_expression_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    switch (tag) {
        case AST_TAG_CONSTANT: {
            int16_t value = 0;
            value = ast_read_i16(ast->reader);
            g_result_in_hl = g_expect_result_in_hl;
            if (g_expect_result_in_hl) {
                codegen_emit(gen, CG_STR_LD_HL);
            } else {
                codegen_emit(gen, CG_STR_LD_A);
                value = (uint8_t)value;
            }
            codegen_emit_hex(gen, value);
            codegen_emit(gen, CG_STR_NL);
            return CC_OK;
        }
        case AST_TAG_IDENTIFIER: {
            uint16_t name_index = 0;
            name_index = ast_read_u16(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (!name) return CC_ERROR_CODEGEN;
            int16_t offset = 0;
            {
                if (codegen_name_is_array(gen, name)) {
                    cc_error_t err = codegen_emit_address_of_identifier(gen, name);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, CG_STR_LD_A_L);
                    g_result_in_hl = true;
                    return CC_OK;
                }
                bool is_16bit = codegen_name_is_16(gen, name);
                bool is_signed = codegen_name_is_signed(gen, name);
                if (is_16bit) {
                    cc_error_t err = codegen_load_pointer_to_hl(gen, name);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, CG_STR_LD_A_L);
                    g_result_in_hl = true;
                    return CC_OK;
                }
                if (codegen_local_or_param_offset(gen, name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, CG_STR_RPAREN_NL);
                } else {
                    codegen_emit(gen, CG_STR_LD_A_LPAREN);
                    codegen_emit_mangled_var(gen, name);
                    codegen_emit(gen, CG_STR_RPAREN_NL);
                }
                if (g_expect_result_in_hl) {
                    if (is_signed) {
                        codegen_result_sign_extend_to_hl(gen);
                    } else {
                        codegen_emit(gen, CG_STR_LD_L_A_H_ZERO);
                    }
                }
                /* result stays in A when not widened */
                g_result_in_hl = g_expect_result_in_hl;
            }
            return CC_OK;
        }
        case AST_TAG_UNARY_OP: {
            uint8_t op = 0;
            uint8_t child_tag = 0;
            op = ast_read_u8(ast->reader);
            child_tag = ast_read_u8(ast->reader);
            if (op == OP_DEREF) {
                if (child_tag == AST_TAG_IDENTIFIER) {
                    const char* name = NULL;
                    if (codegen_stream_read_name(ast, &name) < 0) return CC_ERROR_CODEGEN;
                    cc_error_t err = codegen_load_pointer_to_hl(gen, name);
                    if (err != CC_OK) return err;
                    codegen_emit(gen, CG_STR_LD_A_HL);
                    g_result_in_hl = false;
                    return CC_OK;
                }
                ast_reader_skip_tag(ast, child_tag);
#if !CC_TRUST_SEMANTIC
                cc_error("Unsupported dereference operand");
#endif
                return CC_ERROR_CODEGEN;
            }
            if (op == OP_ADDR && child_tag == AST_TAG_IDENTIFIER) {
                const char* name = NULL;
                if (codegen_stream_read_name(ast, &name) < 0) return CC_ERROR_CODEGEN;
                cc_error_t err = codegen_emit_address_of_identifier(gen, name);
                if (err != CC_OK) return err;
                g_result_in_hl = true;
                return CC_OK;
            }
            if (op == OP_PREINC || op == OP_PREDEC ||
                op == OP_POSTINC || op == OP_POSTDEC) {
                bool is_post = (op == OP_POSTINC || op == OP_POSTDEC);
                bool is_inc = (op == OP_PREINC || op == OP_POSTINC);
                if (child_tag == AST_TAG_IDENTIFIER) {
                    const char* name = NULL;
                    if (codegen_stream_read_name(ast, &name) < 0) return CC_ERROR_CODEGEN;
                    bool is_signed = codegen_name_is_signed(gen, name);
                    if (codegen_name_is_array(gen, name)) {
#if !CC_TRUST_SEMANTIC
                        cc_error("Unsupported ++/-- on array");
#endif
                        return CC_ERROR_CODEGEN;
                    }
                    if (codegen_name_is_16(gen, name)) {
                        cc_error_t err = codegen_load_pointer_to_hl(gen, name);
                        if (err != CC_OK) return err;
                        g_result_in_hl = true;
                        if (is_post) {
                            codegen_emit(gen, CG_STR_PUSH_HL);
                        }
                        codegen_emit(gen, is_inc ? "  inc hl\n" : "  dec hl\n");
                        err = codegen_store_pointer_from_hl(gen, name);
                        if (err != CC_OK) return err;
                        if (is_post) {
                            codegen_emit(gen, "  pop hl\n");
                        }
                        if (!g_expect_result_in_hl) {
                            codegen_result_to_a(gen);
                        }
                        return CC_OK;
                    }
                    {
                        int16_t offset = 0;
                        if (codegen_local_or_param_offset(gen, name, &offset)) {
                            codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                            codegen_emit_ix_offset(gen, offset);
                            codegen_emit(gen, CG_STR_RPAREN_NL);
                        } else {
                            codegen_emit(gen, CG_STR_LD_A_LPAREN);
                            codegen_emit_mangled_var(gen, name);
                            codegen_emit(gen, CG_STR_RPAREN_NL);
                        }
                        g_result_in_hl = false;
                        if (is_post) {
                            codegen_emit(gen, CG_STR_PUSH_AF);
                        }
                        codegen_emit(gen, is_inc ? "  inc a\n" : "  dec a\n");
                        cc_error_t err = codegen_store_a_to_identifier(gen, name);
                        if (err != CC_OK) return err;
                        if (is_post) {
                            codegen_emit(gen, "  pop af\n");
                        }
                        if (g_expect_result_in_hl) {
                            if (is_signed) {
                                codegen_result_sign_extend_to_hl(gen);
                            } else {
                                codegen_result_to_hl(gen);
                            }
                        }
                        return CC_OK;
                    }
                }
                if (child_tag == AST_TAG_ARRAY_ACCESS) {
                    uint8_t elem_size = 0;
                    bool elem_signed = false;
                    cc_error_t err = codegen_emit_array_address(gen, ast, &elem_size, &elem_signed);
                    if (err != CC_OK) return err;
                    if (elem_size == 2) {
                        codegen_emit(gen,
                            "  ld d, h\n"
                            "  ld e, l\n"
                            "  ld a, (de)\n"
                            "  inc de\n"
                            "  ld h, (de)\n"
                            "  ld l, a\n"
                            "  dec de\n");
                        g_result_in_hl = true;
                        if (is_post) {
                            codegen_emit(gen, CG_STR_PUSH_HL);
                        }
                        codegen_emit(gen, is_inc ? "  inc hl\n" : "  dec hl\n");
                        codegen_emit(gen,
                            "  ld a, l\n"
                            "  ld (de), a\n"
                            "  inc de\n"
                            "  ld a, h\n"
                            "  ld (de), a\n");
                        if (is_post) {
                            codegen_emit(gen, "  pop hl\n");
                        }
                        if (!g_expect_result_in_hl) {
                            codegen_result_to_a(gen);
                        }
                        return CC_OK;
                    }
                    codegen_emit(gen, CG_STR_LD_A_HL);
                    g_result_in_hl = false;
                    if (is_post) {
                        codegen_emit(gen, CG_STR_PUSH_AF);
                    }
                    codegen_emit(gen, is_inc ? "  inc a\n" : "  dec a\n");
                    codegen_emit(gen, "  ld (hl), a\n");
                    if (is_post) {
                        codegen_emit(gen, "  pop af\n");
                    }
                    if (g_expect_result_in_hl) {
                        if (elem_signed) {
                            codegen_result_sign_extend_to_hl(gen);
                        } else {
                            codegen_result_to_hl(gen);
                        }
                    }
                    return CC_OK;
                }
                ast_reader_skip_tag(ast, child_tag);
#if !CC_TRUST_SEMANTIC
                cc_error("Unsupported ++/-- operand");
#endif
                return CC_ERROR_CODEGEN;
            }
            if (op == OP_NEG || op == OP_LNOT || op == OP_NOT) {
                cc_error_t err = codegen_stream_expression_expect(gen, ast, child_tag, g_expect_result_in_hl);
                if (err != CC_OK) return err;
                if (op == OP_NEG) {
                    if (g_result_in_hl) {
                        codegen_emit(gen,
                            "  ld a, h\n"
                            "  cpl\n"
                            "  ld h, a\n"
                            "  ld a, l\n"
                            "  cpl\n"
                            "  ld l, a\n"
                            "  inc hl\n");
                    } else {
                        codegen_emit(gen, "  neg\n");
                    }
                    if (g_expect_result_in_hl && !g_result_in_hl) {
                        codegen_result_to_hl(gen);
                    } else if (!g_expect_result_in_hl && g_result_in_hl) {
                        codegen_result_to_a(gen);
                    }
                    return CC_OK;
                }
                if (op == OP_NOT) {
                    if (g_result_in_hl) {
                        codegen_emit(gen,
                            "  ld a, h\n"
                            "  cpl\n"
                            "  ld h, a\n"
                            "  ld a, l\n"
                            "  cpl\n"
                            "  ld l, a\n");
                    } else {
                        codegen_emit(gen, "  cpl\n");
                    }
                    if (g_expect_result_in_hl && !g_result_in_hl) {
                        codegen_result_to_hl(gen);
                    } else if (!g_expect_result_in_hl && g_result_in_hl) {
                        codegen_result_to_a(gen);
                    }
                    return CC_OK;
                }
                if (g_result_in_hl) {
                    codegen_emit(gen,
                        "  ld a, h\n"
                        "  or l\n");
                } else {
                    codegen_emit(gen, CG_STR_OR_A);
                }
                codegen_emit_compare(gen, CG_STR_JR_Z, NULL, g_expect_result_in_hl, true);
                g_result_in_hl = g_expect_result_in_hl;
                return CC_OK;
            }
            ast_reader_skip_tag(ast, child_tag);
            cc_error("Unsupported unary op");
            return CC_ERROR_CODEGEN;
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = 0;
            op = ast_read_u8(ast->reader);
            uint8_t left_tag = 0;
            left_tag = ast_read_u8(ast->reader);
            if (op == OP_LAND || op == OP_LOR) {
                cc_error_t err = CC_OK;
                bool output_in_hl = g_expect_result_in_hl;
                char* short_label = codegen_new_label_persist(gen);
                char* end_label = codegen_new_label_persist(gen);
                if (!short_label || !end_label) {
                    err = CC_ERROR_CODEGEN;
                    goto logical_cleanup;
                }
                err = codegen_stream_expression_expect(gen, ast, left_tag, true);
                if (err != CC_OK) goto logical_cleanup;
                codegen_emit(gen,
                    "  ld a, h\n"
                    "  or l\n");
                if (op == OP_LAND) {
                    codegen_emit_jump(gen, CG_STR_JR_Z, short_label);
                } else {
                    codegen_emit_jump(gen, CG_STR_JR_NZ, short_label);
                }
                {
                    uint8_t right_tag = 0;
                    right_tag = ast_read_u8(ast->reader);
                    err = codegen_stream_expression_expect(gen, ast, right_tag, true);
                    if (err != CC_OK) goto logical_cleanup;
                }
                codegen_emit(gen,
                    "  ld a, h\n"
                    "  or l\n");
                codegen_emit_compare(gen, CG_STR_JR_NZ, NULL, output_in_hl, true);
                codegen_emit_jump(gen, CG_STR_JR, end_label);
                codegen_emit_label(gen, short_label);
                if (op == OP_LAND) {
                    codegen_emit(gen, output_in_hl ? CG_STR_LD_HL_ZERO : CG_STR_LD_A_ZERO);
                } else {
                    codegen_emit(gen, output_in_hl ? "  ld hl, 1\n" : CG_STR_LD_A_ONE);
                }
                codegen_emit_label(gen, end_label);
                g_result_in_hl = output_in_hl;
                err = CC_OK;
logical_cleanup:
                if (short_label) cc_free(short_label);
                if (end_label) cc_free(end_label);
                return err;
            }
            bool is_compare = codegen_op_is_compare(op);
            bool force_16bit_compare = false;
            if (is_compare && !g_expect_result_in_hl) {
                uint32_t expr_pos = reader_tell(ast->reader);
                bool left_is_16 = codegen_expression_is_16bit_at(gen, ast, left_tag);
                uint8_t right_tag_peek = 0;
                right_tag_peek = ast_read_u8(ast->reader);
                bool right_is_16 = codegen_expression_is_16bit_at(gen, ast, right_tag_peek);
                if (reader_seek(ast->reader, expr_pos) < 0) return CC_ERROR_CODEGEN;
                force_16bit_compare = left_is_16 || right_is_16;
            }
            if (g_expect_result_in_hl || (is_compare && force_16bit_compare)) {
                bool output_in_hl = g_expect_result_in_hl;
                return codegen_emit_binary_op_hl(gen, ast, op, left_tag, output_in_hl);
            } else {
                return codegen_emit_binary_op_a(gen, ast, op, left_tag);
            }
        }
        case AST_TAG_CALL: {
            uint16_t name_index = 0;
            uint8_t arg_count = 0;
            name_index = ast_read_u16(ast->reader);
            arg_count = ast_read_u8(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (!name) return CC_ERROR_CODEGEN;
            (void)name;

            if (arg_count > 0) {
                if (arg_count > (uint8_t)DIM(g_arg_offsets)) {
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
                    arg_tag = ast_read_u8(ast->reader);
                    cc_error_t err = codegen_stream_expression_tag(gen, ast, arg_tag);
                    if (err != CC_OK) return err;
                    /* If the expression left a 16-bit result in HL, push HL directly.
                       Otherwise widen A to HL and push as before. */
                    codegen_emit(gen,
                        g_result_in_hl ? "  push hl\n" : CG_STR_LD_L_A_H_ZERO_PUSH_HL);
                }
                if (reader_seek(ast->reader, end_pos) < 0) return CC_ERROR_CODEGEN;
            }

            codegen_emit(gen, CG_STR_CALL);
            codegen_emit_label_name(gen, name);
            codegen_emit(gen, CG_STR_NL);
            if (arg_count > 0) {
                for (uint8_t i = 0; i < arg_count; i++) {
                    codegen_emit(gen, CG_STR_POP_BC);
                }
            }
            g_result_in_hl = codegen_function_return_is_16bit(gen, name_index);
            if (g_result_in_hl && !g_expect_result_in_hl) {
                codegen_result_to_a(gen);
            }
            return CC_OK;
        }
        case AST_TAG_STRING_LITERAL: {
            (void)ast_read_u16(ast->reader);
            cc_error("String literal used without index");
            return CC_ERROR_CODEGEN;
        }
        case AST_TAG_ARRAY_ACCESS: {
            uint8_t elem_size = 0;
            bool elem_signed = false;
            cc_error_t err = codegen_emit_array_address(gen, ast, &elem_size, &elem_signed);
            if (err != CC_OK) return err;
            if (elem_size == 2) {
                codegen_emit(gen,
                    "  ld a, (hl)\n"
                    "  inc hl\n"
                    "  ld h, (hl)\n"
                    "  ld l, a\n");
                codegen_emit(gen, CG_STR_LD_A_L);
                g_result_in_hl = true;
            } else {
                codegen_emit(gen, CG_STR_LD_A_HL);
                g_result_in_hl = false;
                if (g_expect_result_in_hl) {
                    if (elem_signed) {
                        codegen_result_sign_extend_to_hl(gen);
                    } else {
                        codegen_result_to_hl(gen);
                    }
                }
            }
            return CC_OK;
        }
        case AST_TAG_ASSIGN: {
            uint8_t ltag = 0;
            uint8_t rtag = 0;
            const char* lvalue_name = NULL;
            bool lvalue_deref = false;

            ltag = ast_read_u8(ast->reader);
            if (ltag == AST_TAG_ARRAY_ACCESS) {
                uint8_t elem_size = 0;
                cc_error_t err = codegen_emit_array_address(gen, ast, &elem_size, NULL);
                if (err != CC_OK) return err;
                rtag = ast_read_u8(ast->reader);
                codegen_emit(gen, CG_STR_PUSH_HL);
                bool expect_hl = (elem_size == 2) &&
                                 codegen_tag_is_simple_expr(rtag);
                err = codegen_stream_expression_expect(gen, ast, rtag, expect_hl);
                if (err != CC_OK) return err;
                codegen_emit(gen, "  pop de\n");
                if (elem_size == 2) {
                    codegen_result_to_hl(gen);
                    codegen_emit(gen,
                        "  ex de, hl\n"
                        "  ld (hl), e\n"
                        "  inc hl\n"
                        "  ld (hl), d\n"
                        "  ex de, hl\n");
                    g_result_in_hl = true;
                } else {
                    codegen_result_to_a(gen);
                    codegen_emit(gen, "  ld (de), a\n");
                }
                return CC_OK;
            }
            if (ltag == AST_TAG_UNARY_OP) {
                uint8_t op = 0;
                op = ast_read_u8(ast->reader);
                if (op == OP_DEREF) {
                    uint8_t operand_tag = 0;
                    operand_tag = ast_read_u8(ast->reader);
                    if (operand_tag == AST_TAG_IDENTIFIER) {
                        if (codegen_stream_read_name(ast, &lvalue_name) < 0) return CC_ERROR_CODEGEN;
                        lvalue_deref = true;
                    } else {
                        ast_reader_skip_tag(ast, operand_tag);
#if !CC_TRUST_SEMANTIC
                        cc_error("Unsupported dereference assignment");
#endif
                        return CC_ERROR_CODEGEN;
                    }
                } else {
                    ast_reader_skip_node(ast);
#if !CC_TRUST_SEMANTIC
                    cc_error("Unsupported assignment target");
#endif
                    return CC_ERROR_CODEGEN;
                }
            } else if (ltag == AST_TAG_IDENTIFIER) {
                if (codegen_stream_read_name(ast, &lvalue_name) < 0) return CC_ERROR_CODEGEN;
            } else {
                ast_reader_skip_tag(ast, ltag);
                rtag = ast_read_u8(ast->reader);
                ast_reader_skip_tag(ast, rtag);
                return CC_ERROR_CODEGEN;
            }

            rtag = ast_read_u8(ast->reader);

            if (lvalue_name && codegen_name_is_array(gen, lvalue_name)) {
                if (ast_reader_skip_tag(ast, rtag) < 0) return CC_ERROR_CODEGEN;
#if !CC_TRUST_SEMANTIC
                cc_error("Unsupported assignment to array");
#endif
                return CC_ERROR_CODEGEN;
            }

            if (lvalue_deref && lvalue_name) {
                cc_error_t err = codegen_stream_expression_tag(gen, ast, rtag);
                if (err != CC_OK) return err;
                err = codegen_load_pointer_to_hl(gen, lvalue_name);
                if (err != CC_OK) return err;
                codegen_emit(gen, CG_STR_LD_HL_A);
                return CC_OK;
            }

            if (lvalue_name &&
                codegen_name_is_16(gen, lvalue_name)) {
                if (rtag == AST_TAG_STRING_LITERAL) {
                    const char* rvalue_string = NULL;
                    if (codegen_stream_read_name(ast, &rvalue_string) < 0) return CC_ERROR_CODEGEN;
                    const char* label = codegen_get_string_label(gen, rvalue_string);
                    if (!label) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_LD_HL);
                    codegen_emit(gen, label);
                    codegen_emit(gen, CG_STR_NL);
                    return codegen_store_pointer_from_hl(gen, lvalue_name);
                }
            }

            bool lvalue_is_16 = lvalue_name && codegen_name_is_16(gen, lvalue_name);
            bool expect_hl = lvalue_is_16 &&
                             codegen_tag_is_simple_expr(rtag);
            cc_error_t err = codegen_stream_expression_expect(gen, ast, rtag, expect_hl);
            if (err != CC_OK) return err;
            if (lvalue_name) {
                if (lvalue_is_16) {
                    codegen_result_to_hl(gen);
                    return codegen_store_pointer_from_hl(gen, lvalue_name);
                }
                return codegen_store_a_to_identifier(gen, lvalue_name);
            }
            return CC_OK;
        }
        default:
            return CC_ERROR_CODEGEN;
    }
}

static cc_error_t codegen_stream_statement_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    static const struct {
        uint8_t tag;
        statement_handler_t handler;
    } handlers[] = {
        { AST_TAG_RETURN_STMT, codegen_statement_return },
        { AST_TAG_BREAK_STMT, codegen_statement_break },
        { AST_TAG_CONTINUE_STMT, codegen_statement_continue },
        { AST_TAG_GOTO_STMT, codegen_statement_goto },
        { AST_TAG_LABEL_STMT, codegen_statement_label },
        { AST_TAG_VAR_DECL, codegen_statement_var_decl },
        { AST_TAG_COMPOUND_STMT, codegen_statement_compound },
        { AST_TAG_IF_STMT, codegen_statement_if },
        { AST_TAG_WHILE_STMT, codegen_statement_while },
        { AST_TAG_FOR_STMT, codegen_statement_for },
        { AST_TAG_ASSIGN, codegen_stream_expression_tag },
        { AST_TAG_CALL, codegen_stream_expression_tag },
    };
    uint8_t count = (uint8_t)DIM(handlers);
    for (uint8_t i = 0; i < count; i++) {
        if (handlers[i].tag == tag) {
            return handlers[i].handler(gen, ast, tag);
        }
    }
    return CC_OK;
}

static int8_t codegen_stream_collect_locals(codegen_t* gen, ast_reader_t* ast) {
    uint8_t tag = 0;
    tag = ast_read_u8(ast->reader);
    switch (tag) {
        case AST_TAG_VAR_DECL: {
            uint16_t name_index = 0;
            uint8_t has_init = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            name_index = ast_read_u16(ast->reader);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            has_init = ast_read_u8(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (!name) return -1;
            bool is_array = array_len > 0;
            bool is_pointer = (!is_array && depth > 0);
            uint8_t elem_size = 0;
            uint8_t base_kind = codegen_base_type(base);
            bool is_signed = !codegen_base_is_unsigned(base);
            if (base_kind == AST_BASE_VOID) {
                is_signed = true;
            }
            bool is_16bit = codegen_stream_type_is_16bit(base, depth);
            uint16_t size = 0;
            bool elem_signed = is_signed;
            if (is_array) {
                elem_size = codegen_type_size(base, depth);
                size = (uint16_t)(elem_size * array_len);
                is_16bit = false;
            } else {
                size = is_16bit ? 2u : 1u;
                if (is_pointer) {
                    elem_size = codegen_pointer_elem_size(base, depth);
                }
            }
            codegen_record_local(gen, name, size, is_16bit, is_signed, is_pointer,
                                 is_array, elem_size, elem_signed);
            if (has_init) return ast_reader_skip_node(ast);
            return 0;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = 0;
            stmt_count = ast_read_u16(ast->reader);
            for (uint16_t i = 0; i < stmt_count; i++) {
                if (codegen_stream_collect_locals(gen, ast) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_IF_STMT: {
            uint8_t has_else = 0;
            has_else = ast_read_u8(ast->reader);
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
            has_init = ast_read_u8(ast->reader);
            has_cond = ast_read_u8(ast->reader);
            has_inc = ast_read_u8(ast->reader);
            if (has_init && codegen_stream_collect_locals(gen, ast) < 0) return -1;
            if (has_cond && ast_reader_skip_node(ast) < 0) return -1;
            if (has_inc && ast_reader_skip_node(ast) < 0) return -1;
            return codegen_stream_collect_locals(gen, ast);
        }
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = 0;
            has_expr = ast_read_u8(ast->reader);
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
    uint16_t array_len = 0;
    const char* name = NULL;
    name_index = ast_read_u16(ast->reader);
    if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
    param_count = ast_read_u8(ast->reader);
    name = ast_reader_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;

    gen->current_function_name = name;
    gen->function_return_is_16 = codegen_stream_type_is_16bit(base, depth);
    codegen_register_function_return(gen, name_index, gen->function_return_is_16);

    gen->local_var_count = 0;
    gen->param_count = 0;
    gen->function_end_label = NULL;
    gen->stack_offset = 0;
    gen->loop_depth = 0;

    for (uint8_t i = 0; i < param_count; i++) {
        uint8_t tag = 0;
        uint16_t param_name_index = 0;
        uint8_t param_depth = 0;
        uint8_t param_base = 0;
        uint16_t param_array_len = 0;
        uint8_t has_init = 0;
        tag = ast_read_u8(ast->reader);
        if (tag != AST_TAG_VAR_DECL) return CC_ERROR_CODEGEN;
        param_name_index = ast_read_u16(ast->reader);
        if (ast_reader_read_type_info(ast, &param_base, &param_depth,
                                      &param_array_len) < 0) return CC_ERROR_CODEGEN;
        has_init = ast_read_u8(ast->reader);
        if (has_init && ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
        if (gen->param_count < (uint8_t)DIM(gen->params)) {
            bool is_pointer = (param_depth > 0) || (param_array_len > 0);
            uint8_t param_base_kind = codegen_base_type(param_base);
            bool param_signed = !codegen_base_is_unsigned(param_base);
            if (param_base_kind == AST_BASE_VOID) {
                param_signed = true;
            }
            {
                codegen_param_t param;
                param.name = ast_reader_string(ast, param_name_index);
                param.offset = 0;
                param.elem_size = 0;
                param.flags = 0;
                if (codegen_stream_type_is_16bit(param_base, param_depth)) {
                    param.flags |= CG_FLAG_IS_16;
                }
                if (param_signed) {
                    param.flags |= CG_FLAG_IS_SIGNED;
                }
                if (is_pointer) {
                    param.flags |= CG_FLAG_IS_POINTER;
                    param.elem_size = (param_depth > 0)
                        ? codegen_pointer_elem_size(param_base, param_depth)
                        : codegen_type_size(param_base, 0);
                    if (param_signed) {
                        param.flags |= CG_FLAG_ELEM_SIGNED;
                    }
                }
                mem_cpy(&gen->params[gen->param_count], &param, sizeof(param));
            }
            gen->param_count++;
        }
    }

    codegen_emit_label(gen, name);

    uint32_t body_start = reader_tell(ast->reader);
    if (codegen_stream_collect_locals(gen, ast) < 0) return CC_ERROR_CODEGEN;

    for (codegen_param_count_t i = 0; i < gen->param_count; i++) {
        gen->params[i].offset =
            (int16_t)(gen->stack_offset + 4 + (int16_t)(2 * i));
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
    body_tag = ast_read_u8(ast->reader);
    if (body_tag == AST_TAG_COMPOUND_STMT) {
        uint16_t stmt_count = 0;
        stmt_count = ast_read_u16(ast->reader);
        for (uint16_t i = 0; i < stmt_count; i++) {
            uint8_t stmt_tag = 0;
            stmt_tag = ast_read_u8(ast->reader);
            cc_error_t err = codegen_stream_statement_tag(gen, ast, stmt_tag);
            if (err != CC_OK) return err;
        }
    } else if (body_tag == AST_TAG_RETURN_STMT) {
        cc_error_t err = codegen_stream_statement_tag(gen, ast, body_tag);
        if (err != CC_OK) return err;
    } else {
        cc_error_t err = codegen_stream_statement_tag(gen, ast, body_tag);
        if (err != CC_OK) return err;
    }

    {
        bool preserve_hl = gen->function_return_is_16;
        codegen_emit_label(gen, gen->function_end_label);
        if (preserve_hl) {
            codegen_emit(gen,
                "  ld b, h\n"
                "  ld c, l\n");
        }
        codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        if (preserve_hl) {
            codegen_emit(gen,
                "  ld h, b\n"
                "  ld l, c\n");
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    }

    if (gen->function_end_label) {
        cc_free(gen->function_end_label);
        gen->function_end_label = NULL;
    }
    codegen_emit(gen, CG_STR_NL);
    gen->current_function_name = NULL;

    return CC_OK;
}

static cc_error_t codegen_stream_global_var(codegen_t* gen, ast_reader_t* ast) {
    uint16_t name_index = 0;
    uint8_t has_init = 0;
    uint8_t base = 0;
    uint8_t depth = 0;
    uint16_t array_len = 0;
    name_index = ast_read_u16(ast->reader);
    if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
    has_init = ast_read_u8(ast->reader);
    const char* name = ast_reader_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;
    bool is_array = array_len > 0;
    bool is_pointer = (!is_array && depth > 0);
    uint8_t base_kind = codegen_base_type(base);
    (void)name;
    codegen_emit_mangled_var(gen, name);

    if (is_array) {
        uint8_t elem_size = codegen_type_size(base, depth);
        uint16_t total_size = (uint16_t)(elem_size * array_len);
        codegen_emit(gen, CG_STR_COLON);
        if (elem_size == 0) {
#if !CC_TRUST_SEMANTIC
            cc_error("Unsupported array element type");
#endif
            return CC_ERROR_CODEGEN;
        }
        if (has_init) {
            uint8_t tag = 0;
            tag = ast_read_u8(ast->reader);
            if (tag == AST_TAG_STRING_LITERAL && base_kind == AST_BASE_CHAR && depth == 0) {
                const char* init_str = NULL;
                uint16_t len = 0;
                if (codegen_stream_read_name(ast, &init_str) < 0) return CC_ERROR_CODEGEN;
                while (init_str[len]) len++;
                if ((uint16_t)(len + 1u) > array_len) {
                    cc_error("String literal too long for array");
                    return CC_ERROR_CODEGEN;
                }
                codegen_emit_string_literal(gen, init_str);
                codegen_emit(gen, ".db 0\n");
                if ((uint16_t)(len + 1u) < array_len) {
                    codegen_emit(gen, CG_STR_DS);
                    codegen_emit_hex(gen, (uint16_t)(array_len - (uint16_t)(len + 1u)));
                    codegen_emit(gen, CG_STR_NL);
                }
                return CC_OK;
            }
            if (ast_reader_skip_tag(ast, tag) < 0) return CC_ERROR_CODEGEN;
#if !CC_TRUST_SEMANTIC
            cc_error(CG_MSG_ARRAY_INIT_NOT_SUPPORTED);
#endif
            return CC_ERROR_CODEGEN;
        }
        codegen_emit(gen, CG_STR_DS);
        codegen_emit_hex(gen, (uint16_t)total_size);
        codegen_emit(gen, CG_STR_NL);
        return CC_OK;
    }

    if (is_pointer) {
        if (has_init) {
            uint8_t tag = 0;
            tag = ast_read_u8(ast->reader);
            if (tag == AST_TAG_STRING_LITERAL) {
                const char* init_str = NULL;
                if (codegen_stream_read_name(ast, &init_str) < 0) return CC_ERROR_CODEGEN;
                const char* label = codegen_get_string_label(gen, init_str);
                if (!label) return CC_ERROR_CODEGEN;
                codegen_emit(gen, CG_STR_COLON);
                codegen_emit(gen, CG_STR_DW);
                codegen_emit(gen, label);
                codegen_emit(gen, CG_STR_NL);
                return CC_OK;
            }
            if (tag == AST_TAG_UNARY_OP) {
                uint8_t op = 0;
                uint8_t operand_tag = 0;
                op = ast_read_u8(ast->reader);
                operand_tag = ast_read_u8(ast->reader);
                if (op == OP_ADDR && operand_tag == AST_TAG_IDENTIFIER) {
                    const char* ident = NULL;
                    if (codegen_stream_read_name(ast, &ident) < 0) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_COLON);
                    codegen_emit(gen, CG_STR_DW);
                    codegen_emit_mangled_var(gen, ident);
                    codegen_emit(gen, CG_STR_NL);
                    return CC_OK;
                }
                ast_reader_skip_tag(ast, operand_tag);
            }
            if (tag == AST_TAG_CONSTANT) {
                (void)ast_read_i16(ast->reader);
            } else {
                ast_reader_skip_tag(ast, tag);
            }
        }
        codegen_emit(gen, CG_STR_COLON);
        codegen_emit(gen, CG_STR_DW);
        codegen_emit_hex(gen, 0);
        return CC_OK;
    }

    bool is_16bit = codegen_stream_type_is_16bit(base, depth);
    if (has_init) {
        uint8_t tag = 0;
        tag = ast_read_u8(ast->reader);
        if (tag == AST_TAG_CONSTANT) {
            int16_t value = 0;
            value = ast_read_i16(ast->reader);
            codegen_emit(gen, CG_STR_COLON);
            codegen_emit(gen, is_16bit ? CG_STR_DW : CG_STR_DB);
            {
                uint16_t emit_value = is_16bit ? (uint16_t)value : (uint8_t)value;
                codegen_emit_hex(gen, emit_value);
            }
            codegen_emit(gen, CG_STR_NL);
            return CC_OK;
        }
        ast_reader_skip_tag(ast, tag);
    }
    codegen_emit(gen, CG_STR_COLON);
    codegen_emit(gen, is_16bit ? CG_STR_DW : CG_STR_DB);
    codegen_emit_hex(gen, 0);
    return CC_OK;
}

cc_error_t codegen_generate_stream(codegen_t* gen, ast_reader_t* ast) {
    uint16_t decl_count = 0;
    if (!gen || !ast) return CC_ERROR_INTERNAL;

    codegen_emit_file(gen, "runtime/crt0.asm");
    codegen_emit(gen, "\n; Program code\n");

    if (ast_reader_begin_program(ast, &decl_count) < 0) {
        return CC_ERROR_CODEGEN;
    }
    for (uint16_t i = 0; i < decl_count; i++) {
        uint8_t tag = 0;
        tag = ast_read_u8(ast->reader);
        if (tag == AST_TAG_VAR_DECL) {
            uint16_t name_index = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint8_t has_init = 0;
            uint16_t array_len = 0;
            name_index = ast_read_u16(ast->reader);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
            has_init = ast_read_u8(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (name && codegen_global_index(gen, name) < 0) {
                if (gen->global_count < DIM(gen->globals)) {
                    bool is_array = array_len > 0;
                    bool is_pointer = (!is_array && depth > 0);
                    bool is_16bit = codegen_stream_type_is_16bit(base, depth);
                    uint8_t base_kind = codegen_base_type(base);
                    bool is_signed = !codegen_base_is_unsigned(base);
                    bool elem_signed = is_signed;
                    if (base_kind == AST_BASE_VOID) {
                        is_signed = true;
                        elem_signed = true;
                    }
                    uint8_t elem_size = 0;
                    if (is_array) {
                        elem_size = codegen_type_size(base, depth);
                        is_16bit = false;
                    } else if (is_pointer) {
                        elem_size = codegen_pointer_elem_size(base, depth);
                    }
                    {
                        codegen_global_t global;
                        global.name = name;
                        global.elem_size = elem_size;
                        global.flags = 0;
                        if (is_16bit) global.flags |= CG_FLAG_IS_16;
                        if (is_signed) global.flags |= CG_FLAG_IS_SIGNED;
                        if (is_pointer) global.flags |= CG_FLAG_IS_POINTER;
                        if (is_array) global.flags |= CG_FLAG_IS_ARRAY;
                        if (elem_signed) global.flags |= CG_FLAG_ELEM_SIGNED;
                        mem_cpy(&gen->globals[gen->global_count], &global, sizeof(global));
                    }
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
        tag = ast_read_u8(ast->reader);
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
        tag = ast_read_u8(ast->reader);
        if (tag == AST_TAG_VAR_DECL) {
            cc_error_t err = codegen_stream_global_var(gen, ast);
            if (err != CC_OK) return err;
        } else {
            if (ast_reader_skip_tag(ast, tag) < 0) return CC_ERROR_CODEGEN;
        }
    }

    if (gen->string_count > 0) {
        codegen_emit(gen, "\n; String literals\n");
        for (codegen_string_count_t i = 0; i < gen->string_count; i++) {
            const char* label = gen->string_labels[i];
            const char* value = gen->string_literals[i];
            if (!label || !value) continue;
            codegen_emit(gen, label);
            codegen_emit(gen, ":\n");
            codegen_emit_string_literal(gen, value);
            // Emit .db 0
            codegen_emit(gen, "  .db 0\n");
        }
    }
    codegen_emit_file(gen, "runtime/zeal8bit.asm");
    codegen_emit_file(gen, "runtime/math_8.asm");
    codegen_emit_file(gen, "runtime/math_16.asm");

    return CC_OK;
}
