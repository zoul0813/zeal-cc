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
static void codegen_emit_u16_hex(codegen_t* gen, uint16_t value);
static bool codegen_stream_type_is_16bit(uint8_t base, uint8_t depth);
static void codegen_emit_string_literal(codegen_t* gen, const char* value);


static uint32_t g_arg_offsets[8];
static const char* g_param_names[8];
static bool g_param_is_16[8];
static bool g_param_is_pointer[8];
static uint8_t g_param_elem_size[8];
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
           tag == AST_TAG_BINARY_OP;
}

static const char g_hex_digits[] = "0123456789abcdef";

static void codegen_result_to_hl(codegen_t* gen) {
    if (g_result_in_hl) {
        return;
    }
    codegen_emit(gen, CG_STR_LD_L_A_H_ZERO);
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
    uint16_t i = 0;
    label[i++] = '_';
    label[i++] = prefix;
    for (uint16_t pos = 0; pos < 6; pos++) {
        uint16_t digit = n % 10;
        label[i + 5 - pos] = (char)('0' + digit);
        n /= 10;
    }
    i += 6;
    label[i] = '\0';
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
    uint16_t i = 0;
    uint16_t hash = 0x811c;
    bool need_hash = false;
    g_emit_buf[i++] = '_';
    g_emit_buf[i++] = 'v';
    g_emit_buf[i++] = '_';
    for (uint16_t n = 0; name[n]; n++) {
        char c = name[n];
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

static bool codegen_names_equal(const char* a, const char* b) {
    if (!a || !b) return 0;
    return str_cmp(a, b) == 0;
}

static void codegen_emit_int(codegen_t* gen, int16_t value) {
    codegen_emit_u16_hex(gen, (uint16_t)value);
}

static void codegen_emit_u8_dec(codegen_t* gen, uint8_t value) {
    char* buf = g_emit_buf;
    uint8_t i = 0;
    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0 && i < 4) {
            buf[i++] = (char)('0' + (value % 10));
            value /= 10;
        }
        uint8_t j = 0;
        uint8_t k = (uint8_t)(i - 1);
        while (j < k) {
            char tmp = buf[j];
            buf[j] = buf[k];
            buf[k] = tmp;
            j++;
            k--;
        }
    }
    buf[i] = '\0';
    codegen_emit(gen, buf);
}

static void codegen_emit_u8_hex(codegen_t* gen, uint8_t value) {
    char* buf = g_emit_buf;
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = g_hex_digits[(value >> 4) & 0xF];
    buf[3] = g_hex_digits[value & 0xF];
    buf[4] = '\0';
    codegen_emit(gen, buf);
}

static void codegen_emit_u16_hex(codegen_t* gen, uint16_t value) {
    char* buf = g_emit_buf;
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = g_hex_digits[(value >> 12) & 0xF];
    buf[3] = g_hex_digits[(value >> 8) & 0xF];
    buf[4] = g_hex_digits[(value >> 4) & 0xF];
    buf[5] = g_hex_digits[value & 0xF];
    buf[6] = '\0';
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
    codegen_emit_label_name(gen, label);
    codegen_emit(gen, CG_STR_COLON_NL);
}

static void codegen_emit_jump(codegen_t* gen, const char* prefix, const char* label) {
    if (!gen || !prefix || !label) return;
    codegen_emit(gen, prefix);
    codegen_emit_label_name(gen, label);
    codegen_emit(gen, CG_STR_NL);
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

static int8_t codegen_stream_read_identifier(ast_reader_t* ast, const char** name) {
    uint16_t index = 0;
    if (!name) return -1;
    if (ast_read_u16(ast->reader, &index) < 0) return -1;
    *name = ast_reader_string(ast, index);
    return *name ? 0 : -1;
}

static int8_t codegen_stream_read_string(ast_reader_t* ast, const char** value) {
    uint16_t index = 0;
    if (!value) return -1;
    if (ast_read_u16(ast->reader, &index) < 0) return -1;
    *value = ast_reader_string(ast, index);
    return *value ? 0 : -1;
}

static int16_t codegen_local_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (codegen_local_count_t i = 0; i < gen->local_var_count; i++) {
        if (gen->local_vars[i] == name || codegen_names_equal(gen->local_vars[i], name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static int16_t codegen_param_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (codegen_param_count_t i = 0; i < gen->param_count; i++) {
        if (gen->param_names[i] == name || codegen_names_equal(gen->param_names[i], name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static int16_t codegen_global_index(codegen_t* gen, const char* name) {
    if (!gen || !name) return -1;
    for (codegen_global_count_t i = 0; i < gen->global_count; i++) {
        if (gen->global_names[i] == name || codegen_names_equal(gen->global_names[i], name)) {
            return (int16_t)i;
        }
    }
    return -1;
}

static void codegen_record_local(codegen_t* gen, const char* name, uint16_t size,
                                 bool is_16bit, bool is_pointer, bool is_array,
                                 uint16_t array_len, uint8_t elem_size) {
    if (!gen || !name) return;
    if (codegen_local_index(gen, name) >= 0) return;
    if (gen->local_var_count < (sizeof(gen->local_vars) / sizeof(gen->local_vars[0]))) {
        gen->local_vars[gen->local_var_count] = name;
        gen->local_offsets[gen->local_var_count] = gen->stack_offset;
        gen->local_is_16[gen->local_var_count] = is_16bit;
        gen->local_is_pointer[gen->local_var_count] = is_pointer;
        gen->local_is_array[gen->local_var_count] = is_array;
        gen->local_array_len[gen->local_var_count] = array_len;
        gen->local_elem_size[gen->local_var_count] = elem_size;
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

static uint8_t codegen_local_or_param_offset(codegen_t* gen, const char* name,
                                             int16_t* out_offset) {
    return codegen_local_offset(gen, name, out_offset) ||
           codegen_param_offset(gen, name, out_offset);
}

static bool codegen_local_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && gen->local_is_16[idx];
}

static bool codegen_local_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && gen->local_is_pointer[idx];
}

static bool codegen_local_is_array(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && gen->local_is_array[idx];
}

static bool codegen_param_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_param_index(gen, name);
    return idx >= 0 && gen->param_is_16[idx];
}

static bool codegen_param_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_param_index(gen, name);
    return idx >= 0 && gen->param_is_pointer[idx];
}

static bool codegen_global_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && gen->global_is_16[idx];
}

static bool codegen_global_is_pointer(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && gen->global_is_pointer[idx];
}

static bool codegen_global_is_array(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && gen->global_is_array[idx];
}

static bool codegen_name_is_16(codegen_t* gen, const char* name) {
    return codegen_local_is_16(gen, name) ||
           codegen_param_is_16(gen, name) ||
           codegen_global_is_16(gen, name);
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
    if (depth > 0) return 2;
    if (base == AST_BASE_CHAR) return 1;
    if (base == AST_BASE_INT) return 2;
    return 0;
}

static uint8_t codegen_pointer_elem_size(uint8_t base, uint8_t depth) {
    if (depth == 0) return 0;
    return codegen_type_size(base, (uint8_t)(depth - 1));
}

static uint8_t codegen_array_elem_size_by_name(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    if (idx >= 0 && gen->local_is_array[idx]) {
        return gen->local_elem_size[idx];
    }
    idx = codegen_global_index(gen, name);
    if (idx >= 0 && gen->global_is_array[idx]) {
        return gen->global_elem_size[idx];
    }
    return 0;
}

static uint8_t codegen_pointer_elem_size_by_name(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    if (idx >= 0 && gen->local_is_pointer[idx]) {
        return gen->local_elem_size[idx];
    }
    idx = codegen_param_index(gen, name);
    if (idx >= 0 && gen->param_is_pointer[idx]) {
        return gen->param_elem_size[idx];
    }
    idx = codegen_global_index(gen, name);
    if (idx >= 0 && gen->global_is_pointer[idx]) {
        return gen->global_elem_size[idx];
    }
    return 0;
}

static const char* codegen_get_string_label(codegen_t* gen, const char* value);

static cc_error_t codegen_emit_address_of_identifier(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_or_param_offset(gen, name, &offset)) {
        codegen_emit(gen, CG_STR_PUSH_IX_POP_HL);
        if (offset != 0) {
            codegen_emit(gen, "  ld bc, ");
            codegen_emit_int(gen, offset);
            codegen_emit(gen, "\n  add hl, bc\n");
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
        codegen_emit(gen, ")\n  ld h, (ix");
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
            cc_error("Unsupported array access");
            return CC_ERROR_CODEGEN;
        }
        return codegen_load_pointer_to_hl(gen, base_name);
    }
    cc_error("Unsupported array access");
    return CC_ERROR_CODEGEN;
}

static cc_error_t codegen_emit_array_address(codegen_t* gen, ast_reader_t* ast,
                                             uint8_t* out_elem_size) {
    uint8_t base_tag = 0;
    uint8_t index_tag = 0;
    const char* base_name = NULL;
    const char* base_string = NULL;

    if (ast_read_u8(ast->reader, &base_tag) < 0) return CC_ERROR_CODEGEN;
    if (base_tag == AST_TAG_STRING_LITERAL) {
        if (codegen_stream_read_string(ast, &base_string) < 0) return CC_ERROR_CODEGEN;
    } else if (base_tag == AST_TAG_IDENTIFIER) {
        if (codegen_stream_read_identifier(ast, &base_name) < 0) return CC_ERROR_CODEGEN;
    } else {
        if (ast_reader_skip_tag(ast, base_tag) < 0) return CC_ERROR_CODEGEN;
        cc_error("Unsupported array access");
        return CC_ERROR_CODEGEN;
    }
    if (ast_read_u8(ast->reader, &index_tag) < 0) return CC_ERROR_CODEGEN;

    uint8_t elem_size = 1;
    if (base_string) {
        elem_size = 1;
    } else if (base_name) {
        if (codegen_name_is_array(gen, base_name)) {
            elem_size = codegen_array_elem_size_by_name(gen, base_name);
        } else if (codegen_name_is_pointer(gen, base_name)) {
            elem_size = codegen_pointer_elem_size_by_name(gen, base_name);
        } else {
            cc_error("Unsupported array access");
            return CC_ERROR_CODEGEN;
        }
        if (elem_size == 0) {
            cc_error("Unsupported array element size");
            return CC_ERROR_CODEGEN;
        }
    }

    cc_error_t err = codegen_stream_expression_tag(gen, ast, index_tag);
    if (err != CC_OK) return err;
    codegen_result_to_a(gen);
    if (elem_size == 2) {
        codegen_emit(gen, "  add a, a\n");
    }
    codegen_emit(gen, "  ld e, a\n  ld d, 0\n");

    err = codegen_load_array_base_to_hl(gen, base_string, base_name);
    if (err != CC_OK) return err;
    codegen_emit(gen, CG_STR_ADD_HL_DE);

    if (out_elem_size) {
        *out_elem_size = elem_size;
    }
    return CC_OK;
}

static void codegen_emit_ix_offset(codegen_t* gen, int16_t offset) {
    if (offset < 0) {
        codegen_emit(gen, "-");
        codegen_emit_u8_dec(gen, (uint8_t)(-offset));
        return;
    }
    codegen_emit(gen, "+");
    codegen_emit_u8_dec(gen, (uint8_t)offset);
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

void codegen_emit(codegen_t* gen, const char* fmt, ...) {
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
    if (output_in_hl) {
        codegen_emit(gen, CG_STR_LD_HL_ZERO);
    } else {
        codegen_emit(gen, CG_STR_LD_A_ZERO);
    }
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
    if (output_in_hl) {
        codegen_emit(gen, "  ld hl, 1\n");
    } else {
        codegen_emit(gen, CG_STR_LD_A_ONE);
    }
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
    if (ast_read_u8(ast->reader, &right_tag) < 0) return CC_ERROR_CODEGEN;
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
        if (codegen_emit_compare_table(gen, op, compare16_table,
                                       (uint8_t)(sizeof(compare16_table) / sizeof(compare16_table[0])),
                                       output_in_hl)) {
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
    {
        static const op_emit_entry_t op16_table[] = {
            { OP_ADD, "  add hl, de\n" },
            { OP_SUB, "  ex de, hl\n  or a\n  sbc hl, de\n" },
            { OP_MUL, "  ex de, hl\n  call __mul_hl_de\n" },
            { OP_DIV, "  ex de, hl\n  call __div_hl_de\n" },
            { OP_MOD, "  ex de, hl\n  call __mod_hl_de\n" },
        };
        if (!codegen_emit_op_table(gen, op, op16_table,
                                   (uint8_t)(sizeof(op16_table) / sizeof(op16_table[0])))) {
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
    if (ast_read_u8(ast->reader, &right_tag) < 0) return CC_ERROR_CODEGEN;
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
        if (codegen_emit_compare_table(gen, op, compare8_table,
                                       (uint8_t)(sizeof(compare8_table) / sizeof(compare8_table[0])),
                                       false)) {
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
    {
        static const op_emit_entry_t op8_table[] = {
            { OP_ADD, "  add a, l\n" },
            { OP_SUB, "  sub l\n" },
            { OP_MUL, "  call __mul_a_l\n" },
            { OP_DIV, "  call __div_a_l\n" },
            { OP_MOD, "  call __mod_a_l\n" },
        };
        if (!codegen_emit_op_table(gen, op, op8_table,
                                   (uint8_t)(sizeof(op8_table) / sizeof(op8_table[0])))) {
            return CC_ERROR_CODEGEN;
        }
    }
    g_result_in_hl = false;
    return CC_OK;
}

static cc_error_t codegen_read_and_stream_statement(codegen_t* gen, ast_reader_t* ast) {
    uint8_t tag = 0;
    if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
    return codegen_stream_statement_tag(gen, ast, tag);
}

static cc_error_t codegen_read_and_stream_expression(codegen_t* gen, ast_reader_t* ast) {
    uint8_t tag = 0;
    if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
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
    if (ast_read_u8(ast->reader, &has_expr) < 0) return CC_ERROR_CODEGEN;
    if (has_expr) {
        uint8_t expr_tag = 0;
        if (ast_read_u8(ast->reader, &expr_tag) < 0) return CC_ERROR_CODEGEN;
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
        if (gen->function_return_is_16) {
            codegen_emit(gen, CG_STR_LD_HL_ZERO);
            g_result_in_hl = true;
        } else {
            codegen_emit(gen, CG_STR_LD_A_ZERO);
            g_result_in_hl = false;
        }
    }
    bool preserve_hl = gen->function_return_is_16;
    if (gen->return_direct || !gen->function_end_label) {
        if (preserve_hl) {
            codegen_emit(gen, "  ld b, h\n  ld c, l\n");
        }
        if (gen->stack_offset > 0) {
            codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        }
        if (preserve_hl) {
            codegen_emit(gen, "  ld h, b\n  ld l, c\n");
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    } else {
        gen->use_function_end_label = true;
        codegen_emit_jump(gen, CG_STR_JP, gen->function_end_label);
    }
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
    if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
    if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
    (void)array_len;
    if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
    name = ast_reader_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;
    (void)name;
    if (array_len > 0) {
        if (has_init) {
            if (ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
            cc_error("Array initialization not supported");
            return CC_ERROR_CODEGEN;
        }
        return CC_OK;
    }
    if (has_init) {
        uint8_t init_tag = 0;
        if (ast_read_u8(ast->reader, &init_tag) < 0) return CC_ERROR_CODEGEN;
        bool is_pointer = depth > 0;
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
                if (codegen_name_is_16(gen, ident)) {
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
    if (ast_read_u16(ast->reader, &stmt_count) < 0) return CC_ERROR_CODEGEN;
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
    if (ast_read_u8(ast->reader, &has_else) < 0) return CC_ERROR_CODEGEN;
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
    err = codegen_read_and_stream_statement(gen, ast);
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
    if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
    if (ast_read_u8(ast->reader, &has_cond) < 0) return CC_ERROR_CODEGEN;
    if (ast_read_u8(ast->reader, &has_inc) < 0) return CC_ERROR_CODEGEN;
    char* loop_label = codegen_new_label_persist(gen);
    char* end_label = codegen_new_label_persist(gen);
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
    err = codegen_read_and_stream_statement(gen, ast);
    if (err != CC_OK) {
        goto for_cleanup;
    }
    if (has_inc) {
        uint32_t body_end = reader_tell(ast->reader);
        if (reader_seek(ast->reader, inc_offset) < 0) {
            err = CC_ERROR_CODEGEN;
            goto for_cleanup;
        }
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
    return err;
}

static bool codegen_expression_is_16bit_at(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    switch (tag) {
        case AST_TAG_CONSTANT: {
            int16_t value = 0;
            if (ast_read_i16(ast->reader, &value) < 0) return false;
            return value < 0 || value > 0xFF;
        }
        case AST_TAG_IDENTIFIER: {
            const char* name = NULL;
            if (codegen_stream_read_identifier(ast, &name) < 0) return false;
            return codegen_name_is_16(gen, name) || codegen_name_is_array(gen, name);
        }
        case AST_TAG_UNARY_OP: {
            uint8_t op = 0;
            uint8_t child_tag = 0;
            if (ast_read_u8(ast->reader, &op) < 0) return false;
            if (ast_read_u8(ast->reader, &child_tag) < 0) return false;
            if (ast_reader_skip_tag(ast, child_tag) < 0) return false;
            return op == OP_ADDR;
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = 0;
            uint8_t left_tag = 0;
            uint8_t right_tag = 0;
            if (ast_read_u8(ast->reader, &op) < 0) return false;
            if (ast_read_u8(ast->reader, &left_tag) < 0) return false;
            bool left_is_16 = codegen_expression_is_16bit_at(gen, ast, left_tag);
            if (ast_read_u8(ast->reader, &right_tag) < 0) return false;
            bool right_is_16 = codegen_expression_is_16bit_at(gen, ast, right_tag);
            if (codegen_op_is_compare(op)) {
                return false;
            }
            return left_is_16 || right_is_16;
        }
        case AST_TAG_CALL: {
            uint16_t name_index = 0;
            uint8_t arg_count = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return false;
            if (ast_read_u8(ast->reader, &arg_count) < 0) return false;
            for (uint8_t i = 0; i < arg_count; i++) {
                uint8_t arg_tag = 0;
                if (ast_read_u8(ast->reader, &arg_tag) < 0) return false;
                if (ast_reader_skip_tag(ast, arg_tag) < 0) return false;
            }
            return codegen_function_return_is_16bit(gen, name_index);
        }
        case AST_TAG_ARRAY_ACCESS: {
            uint8_t base_tag = 0;
            uint8_t index_tag = 0;
            uint8_t elem_size = 1;
            if (ast_read_u8(ast->reader, &base_tag) < 0) return false;
            if (base_tag == AST_TAG_STRING_LITERAL) {
                const char* base_string = NULL;
                if (codegen_stream_read_string(ast, &base_string) < 0) return false;
            } else if (base_tag == AST_TAG_IDENTIFIER) {
                const char* base_name = NULL;
                if (codegen_stream_read_identifier(ast, &base_name) < 0) return false;
                if (codegen_name_is_array(gen, base_name)) {
                    elem_size = codegen_array_elem_size_by_name(gen, base_name);
                } else if (codegen_name_is_pointer(gen, base_name)) {
                    elem_size = codegen_pointer_elem_size_by_name(gen, base_name);
                } else {
                    elem_size = 0;
                }
            } else {
                if (ast_reader_skip_tag(ast, base_tag) < 0) return false;
            }
            if (ast_read_u8(ast->reader, &index_tag) < 0) return false;
            if (ast_reader_skip_tag(ast, index_tag) < 0) return false;
            return elem_size == 2;
        }
        case AST_TAG_ASSIGN: {
            uint8_t ltag = 0;
            uint8_t rtag = 0;
            bool lvalue_is_16 = false;
            if (ast_read_u8(ast->reader, &ltag) < 0) return false;
            if (ltag == AST_TAG_ARRAY_ACCESS) {
                uint8_t base_tag = 0;
                uint8_t index_tag = 0;
                uint8_t elem_size = 1;
                if (ast_read_u8(ast->reader, &base_tag) < 0) return false;
                if (base_tag == AST_TAG_STRING_LITERAL) {
                    const char* base_string = NULL;
                    if (codegen_stream_read_string(ast, &base_string) < 0) return false;
                } else if (base_tag == AST_TAG_IDENTIFIER) {
                    const char* base_name = NULL;
                    if (codegen_stream_read_identifier(ast, &base_name) < 0) return false;
                    if (codegen_name_is_array(gen, base_name)) {
                        elem_size = codegen_array_elem_size_by_name(gen, base_name);
                    } else if (codegen_name_is_pointer(gen, base_name)) {
                        elem_size = codegen_pointer_elem_size_by_name(gen, base_name);
                    } else {
                        elem_size = 0;
                    }
                } else {
                    if (ast_reader_skip_tag(ast, base_tag) < 0) return false;
                }
                if (ast_read_u8(ast->reader, &index_tag) < 0) return false;
                if (ast_reader_skip_tag(ast, index_tag) < 0) return false;
                lvalue_is_16 = elem_size == 2;
            } else if (ltag == AST_TAG_IDENTIFIER) {
                const char* lvalue_name = NULL;
                if (codegen_stream_read_identifier(ast, &lvalue_name) < 0) return false;
                lvalue_is_16 = codegen_name_is_16(gen, lvalue_name);
            } else {
                if (ast_reader_skip_tag(ast, ltag) < 0) return false;
            }
            if (ast_read_u8(ast->reader, &rtag) < 0) return false;
            if (ast_reader_skip_tag(ast, rtag) < 0) return false;
            return lvalue_is_16;
        }
        case AST_TAG_STRING_LITERAL: {
            uint16_t value_index = 0;
            if (ast_read_u16(ast->reader, &value_index) < 0) return false;
            return true;
        }
        default:
            if (ast_reader_skip_tag(ast, tag) < 0) return false;
            return false;
    }
}

static bool codegen_stream_type_is_16bit(uint8_t base, uint8_t depth) {
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
    if (gen->function_count >= (sizeof(gen->function_return_flags) /
                                sizeof(gen->function_return_flags[0]))) {
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
            if (ast_read_i16(ast->reader, &value) < 0) return CC_ERROR_CODEGEN;
            if (g_expect_result_in_hl) {
                codegen_emit(gen, CG_STR_LD_HL);
                codegen_emit_u16_hex(gen, (uint16_t)value);
                codegen_emit(gen, CG_STR_NL);
                g_result_in_hl = true;
            } else {
                codegen_emit(gen, CG_STR_LD_A);
                codegen_emit_u8_hex(gen, (uint8_t)value);
                codegen_emit(gen, CG_STR_NL);
                g_result_in_hl = false;
            }
            return CC_OK;
        }
        case AST_TAG_IDENTIFIER: {
            uint16_t name_index = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
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
                    codegen_emit(gen, CG_STR_LD_L_A_H_ZERO);
                    g_result_in_hl = true;
                } else {
                    /* result stays in A */
                    g_result_in_hl = false;
                }
            }
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
                    g_result_in_hl = false;
                    return CC_OK;
                }
                ast_reader_skip_tag(ast, child_tag);
                cc_error("Unsupported dereference operand");
                return CC_ERROR_CODEGEN;
            }
            if (op == OP_ADDR && child_tag == AST_TAG_IDENTIFIER) {
                const char* name = NULL;
                if (codegen_stream_read_identifier(ast, &name) < 0) return CC_ERROR_CODEGEN;
                cc_error_t err = codegen_emit_address_of_identifier(gen, name);
                if (err != CC_OK) return err;
                g_result_in_hl = true;
                return CC_OK;
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
            bool is_compare = codegen_op_is_compare(op);
            bool force_16bit_compare = false;
            if (is_compare && !g_expect_result_in_hl) {
                uint32_t expr_pos = reader_tell(ast->reader);
                bool left_is_16 = codegen_expression_is_16bit_at(gen, ast, left_tag);
                uint8_t right_tag_peek = 0;
                if (ast_read_u8(ast->reader, &right_tag_peek) < 0) return CC_ERROR_CODEGEN;
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
            if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &arg_count) < 0) return CC_ERROR_CODEGEN;
            const char* name = ast_reader_string(ast, name_index);
            if (!name) return CC_ERROR_CODEGEN;
            (void)name;

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
                    /* If the expression left a 16-bit result in HL, push HL directly.
                       Otherwise widen A to HL and push as before. */
                    if (g_result_in_hl) {
                        codegen_emit(gen, "  push hl\n");
                    } else {
                        codegen_emit(gen, CG_STR_LD_L_A_H_ZERO_PUSH_HL);
                    }
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
            uint16_t value_index = 0;
            if (ast_read_u16(ast->reader, &value_index) < 0) return CC_ERROR_CODEGEN;
            cc_error("String literal used without index");
            return CC_ERROR_CODEGEN;
        }
        case AST_TAG_ARRAY_ACCESS: {
            uint8_t elem_size = 0;
            cc_error_t err = codegen_emit_array_address(gen, ast, &elem_size);
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
            }
            return CC_OK;
        }
        case AST_TAG_ASSIGN: {
            uint8_t ltag = 0;
            uint8_t rtag = 0;
            const char* lvalue_name = NULL;
            bool lvalue_deref = false;

            if (ast_read_u8(ast->reader, &ltag) < 0) return CC_ERROR_CODEGEN;
            if (ltag == AST_TAG_ARRAY_ACCESS) {
                uint8_t elem_size = 0;
                cc_error_t err = codegen_emit_array_address(gen, ast, &elem_size);
                if (err != CC_OK) return err;
                if (ast_read_u8(ast->reader, &rtag) < 0) return CC_ERROR_CODEGEN;
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

            if (lvalue_name && codegen_name_is_array(gen, lvalue_name)) {
                if (ast_reader_skip_tag(ast, rtag) < 0) return CC_ERROR_CODEGEN;
                cc_error("Unsupported assignment to array");
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
                    if (codegen_stream_read_string(ast, &rvalue_string) < 0) return CC_ERROR_CODEGEN;
                    const char* label = codegen_get_string_label(gen, rvalue_string);
                    if (!label) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_LD_HL);
                    codegen_emit(gen, label);
                    codegen_emit(gen, CG_STR_NL);
                    return codegen_store_pointer_from_hl(gen, lvalue_name);
                }
                if (rtag == AST_TAG_UNARY_OP) {
                    uint8_t op = 0;
                    if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
                    if (op == OP_ADDR) {
                        uint8_t operand_tag = 0;
                        if (ast_read_u8(ast->reader, &operand_tag) < 0) return CC_ERROR_CODEGEN;
                        if (operand_tag == AST_TAG_IDENTIFIER) {
                            const char* rvalue_name = NULL;
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
        { AST_TAG_VAR_DECL, codegen_statement_var_decl },
        { AST_TAG_COMPOUND_STMT, codegen_statement_compound },
        { AST_TAG_IF_STMT, codegen_statement_if },
        { AST_TAG_WHILE_STMT, codegen_statement_while },
        { AST_TAG_FOR_STMT, codegen_statement_for },
        { AST_TAG_ASSIGN, codegen_stream_expression_tag },
        { AST_TAG_CALL, codegen_stream_expression_tag },
    };
    uint8_t count = (uint8_t)(sizeof(handlers) / sizeof(handlers[0]));
    for (uint8_t i = 0; i < count; i++) {
        if (handlers[i].tag == tag) {
            return handlers[i].handler(gen, ast, tag);
        }
    }
    return CC_OK;
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
            uint16_t array_len = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return -1;
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            if (ast_read_u8(ast->reader, &has_init) < 0) return -1;
            const char* name = ast_reader_string(ast, name_index);
            if (!name) return -1;
            bool is_array = array_len > 0;
            bool is_pointer = (!is_array && depth > 0);
            uint8_t elem_size = 0;
            bool is_16bit = codegen_stream_type_is_16bit(base, depth);
            uint16_t size = 0;
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
            codegen_record_local(gen, name, size, is_16bit, is_pointer, is_array,
                                 array_len, elem_size);
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
    uint16_t array_len = 0;
    const char* name = NULL;
    uint8_t param_used = 0;

    if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
    if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
    if (ast_read_u8(ast->reader, &param_count) < 0) return CC_ERROR_CODEGEN;
    name = ast_reader_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;

    gen->function_return_is_16 = codegen_stream_type_is_16bit(base, depth);
    codegen_register_function_return(gen, name_index, gen->function_return_is_16);

    gen->local_var_count = 0;
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
        uint16_t param_array_len = 0;
        uint8_t has_init = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag != AST_TAG_VAR_DECL) return CC_ERROR_CODEGEN;
        if (ast_read_u16(ast->reader, &param_name_index) < 0) return CC_ERROR_CODEGEN;
        if (ast_reader_read_type_info(ast, &param_base, &param_depth,
                                      &param_array_len) < 0) return CC_ERROR_CODEGEN;
        if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
        if (has_init && ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
        (void)param_array_len;
        if (param_used < (uint8_t)(sizeof(g_param_names) / sizeof(g_param_names[0]))) {
            g_param_names[param_used] = ast_reader_string(ast, param_name_index);
            g_param_is_16[param_used] = codegen_stream_type_is_16bit(param_base, param_depth);
            g_param_is_pointer[param_used] = (param_depth > 0) || (param_array_len > 0);
            if (g_param_is_pointer[param_used]) {
                g_param_elem_size[param_used] = (param_depth > 0)
                    ? codegen_pointer_elem_size(param_base, param_depth)
                    : codegen_type_size(param_base, 0);
            } else {
                g_param_elem_size[param_used] = 0;
            }
            param_used++;
        }
    }

    codegen_emit_label(gen, name);

    uint32_t body_start = reader_tell(ast->reader);
    if (codegen_stream_collect_locals(gen, ast) < 0) return CC_ERROR_CODEGEN;

    for (uint8_t i = 0; i < param_used; i++) {
        gen->param_names[gen->param_count] = g_param_names[i];
        gen->param_offsets[gen->param_count] =
            (int16_t)(gen->stack_offset + 4 + (int16_t)(2 * gen->param_count));
        gen->param_is_16[gen->param_count] = g_param_is_16[i];
        gen->param_is_pointer[gen->param_count] = g_param_is_pointer[i];
        gen->param_elem_size[gen->param_count] = g_param_elem_size[i];
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
    bool last_was_return = false;
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
        bool preserve_hl = gen->function_return_is_16;
        codegen_emit_label(gen, gen->function_end_label);
        if (preserve_hl) {
            codegen_emit(gen, "  ld b, h\n  ld c, l\n");
        }
        if (gen->stack_offset > 0) {
            codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        }
        if (preserve_hl) {
            codegen_emit(gen, "  ld h, b\n  ld l, c\n");
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    } else if (!last_was_return) {
        if (gen->stack_offset > 0) {
            codegen_emit_stack_adjust(gen, gen->stack_offset, false);
        }
        codegen_emit(gen, CG_STR_POP_IX_RET);
    }

    if (gen->function_end_label) {
        cc_free(gen->function_end_label);
        gen->function_end_label = NULL;
    }
    codegen_emit(gen, CG_STR_NL);

    return CC_OK;
}

static cc_error_t codegen_stream_global_var(codegen_t* gen, ast_reader_t* ast) {
    uint16_t name_index = 0;
    uint8_t has_init = 0;
    uint8_t base = 0;
    uint8_t depth = 0;
    uint16_t array_len = 0;
    if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
    if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
    if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
    const char* name = ast_reader_string(ast, name_index);
    if (!name) return CC_ERROR_CODEGEN;
    bool is_array = array_len > 0;
    bool is_pointer = (!is_array && depth > 0);
    (void)name;
    codegen_emit_mangled_var(gen, name);

    if (is_array) {
        uint8_t elem_size = codegen_type_size(base, depth);
        uint16_t total_size = (uint16_t)(elem_size * array_len);
        codegen_emit(gen, CG_STR_COLON);
        if (elem_size == 0) {
            cc_error("Unsupported array element type");
            return CC_ERROR_CODEGEN;
        }
        if (has_init) {
            uint8_t tag = 0;
            if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
            if (tag == AST_TAG_STRING_LITERAL && base == AST_BASE_CHAR && depth == 0) {
                const char* init_str = NULL;
                uint16_t len = 0;
                if (codegen_stream_read_string(ast, &init_str) < 0) return CC_ERROR_CODEGEN;
                while (init_str[len]) len++;
                if ((uint16_t)(len + 1u) > array_len) {
                    cc_error("String literal too long for array");
                    return CC_ERROR_CODEGEN;
                }
                codegen_emit_string_literal(gen, init_str);
                codegen_emit(gen, ".db 0\n");
                if ((uint16_t)(len + 1u) < array_len) {
                    codegen_emit(gen, CG_STR_DS);
                    codegen_emit_int(gen, (int16_t)(array_len - (uint16_t)(len + 1u)));
                    codegen_emit(gen, CG_STR_NL);
                }
                return CC_OK;
            }
            if (ast_reader_skip_tag(ast, tag) < 0) return CC_ERROR_CODEGEN;
            cc_error("Array initialization not supported");
            return CC_ERROR_CODEGEN;
        }
        codegen_emit(gen, CG_STR_DS);
        codegen_emit_int(gen, (int16_t)total_size);
        codegen_emit(gen, CG_STR_NL);
        return CC_OK;
    }

    if (is_pointer) {
        if (has_init) {
            uint8_t tag = 0;
            if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
            if (tag == AST_TAG_STRING_LITERAL) {
                const char* init_str = NULL;
                if (codegen_stream_read_string(ast, &init_str) < 0) return CC_ERROR_CODEGEN;
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
                if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
                if (ast_read_u8(ast->reader, &operand_tag) < 0) return CC_ERROR_CODEGEN;
                if (op == OP_ADDR && operand_tag == AST_TAG_IDENTIFIER) {
                    const char* ident = NULL;
                    if (codegen_stream_read_identifier(ast, &ident) < 0) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_COLON);
                    codegen_emit(gen, CG_STR_DW);
                    codegen_emit_mangled_var(gen, ident);
                    codegen_emit(gen, CG_STR_NL);
                    return CC_OK;
                }
                ast_reader_skip_tag(ast, operand_tag);
            }
            if (tag == AST_TAG_CONSTANT) {
                int16_t value = 0;
                if (ast_read_i16(ast->reader, &value) < 0) return CC_ERROR_CODEGEN;
            } else {
                ast_reader_skip_tag(ast, tag);
            }
        }
        codegen_emit(gen, CG_STR_COLON);
        codegen_emit(gen, CG_STR_DW);
        codegen_emit_int(gen, 0);
        return CC_OK;
    }

    bool is_16bit = codegen_stream_type_is_16bit(base, depth);
    if (has_init) {
        uint8_t tag = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag == AST_TAG_CONSTANT) {
            int16_t value = 0;
            if (ast_read_i16(ast->reader, &value) < 0) return CC_ERROR_CODEGEN;
            codegen_emit(gen, CG_STR_COLON);
            codegen_emit(gen, is_16bit ? CG_STR_DW : CG_STR_DB);
            if (is_16bit) {
                codegen_emit_u16_hex(gen, (uint16_t)value);
            } else {
                codegen_emit_u8_hex(gen, (uint8_t)value);
            }
            codegen_emit(gen, CG_STR_NL);
            return CC_OK;
        }
        ast_reader_skip_tag(ast, tag);
    }
    codegen_emit(gen, CG_STR_COLON);
    codegen_emit(gen, is_16bit ? CG_STR_DW : CG_STR_DB);
    if (is_16bit) {
        codegen_emit_u16_hex(gen, 0);
    } else {
        codegen_emit_u8_hex(gen, 0);
    }
    return CC_OK;
}

void codegen_emit_runtime(codegen_t* gen) {
    codegen_emit_file(gen, "runtime/zeal8bit.asm");
    codegen_emit_file(gen, "runtime/math_8.asm");
    codegen_emit_file(gen, "runtime/math_16.asm");
}

void codegen_emit_strings(codegen_t* gen) {
    if (!gen || gen->string_count == 0) return;
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

void codegen_emit_preamble(codegen_t* gen) {
    if (!gen) return;
    codegen_emit_file(gen, "runtime/crt0.asm");
    codegen_emit(gen, "\n; Program code\n");
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
            uint16_t array_len = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
            const char* name = ast_reader_string(ast, name_index);
            if (name && codegen_global_index(gen, name) < 0) {
                if (gen->global_count < (sizeof(gen->global_names) / sizeof(gen->global_names[0]))) {
                    bool is_array = array_len > 0;
                    bool is_pointer = (!is_array && depth > 0);
                    bool is_16bit = codegen_stream_type_is_16bit(base, depth);
                    uint8_t elem_size = 0;
                    if (is_array) {
                        elem_size = codegen_type_size(base, depth);
                        is_16bit = false;
                    } else if (is_pointer) {
                        elem_size = codegen_pointer_elem_size(base, depth);
                    }
                    gen->global_names[gen->global_count] = name;
                    gen->global_is_16[gen->global_count] = is_16bit;
                    gen->global_is_pointer[gen->global_count] = is_pointer;
                    gen->global_is_array[gen->global_count] = is_array;
                    gen->global_array_len[gen->global_count] = array_len;
                    gen->global_elem_size[gen->global_count] = elem_size;
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
