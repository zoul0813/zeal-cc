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

/* Helpers */
static void codegen_emit_ix_offset(codegen_t* gen, int16_t offset);
static void codegen_emit_u16_hex(codegen_t* gen, uint16_t value);
static bool codegen_stream_type_is_16bit(uint8_t base, uint8_t depth);

static char codegen_to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
    return c;
}

static uint16_t codegen_label_hash(const char* name) {
    uint16_t hash = 0x811c;
    while (name && *name) {
        hash = (uint16_t)((hash * 33u) ^ (uint8_t)codegen_to_lower(*name++));
    }
    return hash;
}

static void codegen_append_hex4(char* buf, uint16_t* idx, uint16_t value) {
    static const char hex[] = "0123456789abcdef";
    for (int shift = 12; shift >= 0; shift -= 4) {
        buf[(*idx)++] = hex[(value >> shift) & 0xF];
    }
}

static void codegen_emit_label_name(codegen_t* gen, const char* name) {
    if (!gen || !name) return;
    if (str_len(name) <= CODEGEN_LABEL_MAX) {
        for (uint16_t i = 0; name[i]; i++) {
            char c[2] = { codegen_to_lower(name[i]), '\0' };
            codegen_emit(gen, c);
        }
        return;
    }
    char buf[CODEGEN_LABEL_MAX + 1];
    uint16_t hash = codegen_label_hash(name);
    uint16_t i = 0;
    uint16_t keep = CODEGEN_LABEL_MAX - 1 - CODEGEN_LABEL_HASH_LEN;
    while (name[i] && i < keep) {
        buf[i] = codegen_to_lower(name[i]);
        i++;
    }
    buf[i++] = '_';
    codegen_append_hex4(buf, &i, hash);
    buf[i] = '\0';
    codegen_emit(gen, buf);
}

static void codegen_emit_prefixed_label(codegen_t* gen, const char* prefix, const char* name) {
    if (!gen || !prefix || !name) return;
    uint16_t prefix_len = (uint16_t)str_len(prefix);
    uint16_t name_len = (uint16_t)str_len(name);
    if ((uint16_t)(prefix_len + name_len) <= CODEGEN_LABEL_MAX) {
        for (uint16_t i = 0; prefix[i]; i++) {
            char c[2] = { codegen_to_lower(prefix[i]), '\0' };
            codegen_emit(gen, c);
        }
        for (uint16_t i = 0; name[i]; i++) {
            char c[2] = { codegen_to_lower(name[i]), '\0' };
            codegen_emit(gen, c);
        }
        return;
    }
    char buf[CODEGEN_LABEL_MAX + 1];
    uint16_t hash = codegen_label_hash(name);
    uint16_t i = 0;
    for (uint16_t p = 0; p < prefix_len && i < CODEGEN_LABEL_MAX; p++) {
        buf[i++] = codegen_to_lower(prefix[p]);
    }
    uint16_t keep = 1;
    if (prefix_len + 1 + CODEGEN_LABEL_HASH_LEN < CODEGEN_LABEL_MAX) {
        keep = (uint16_t)(CODEGEN_LABEL_MAX - prefix_len - 1 - CODEGEN_LABEL_HASH_LEN);
    }
    for (uint16_t n = 0; n < keep && name[n] && i < CODEGEN_LABEL_MAX; n++) {
        buf[i++] = codegen_to_lower(name[n]);
    }
    if (i < CODEGEN_LABEL_MAX) {
        buf[i++] = '_';
    }
    codegen_append_hex4(buf, &i, hash);
    buf[i] = '\0';
    codegen_emit(gen, buf);
}

static void codegen_emit_mangled_var(codegen_t* gen, const char* name) {
    codegen_emit_prefixed_label(gen, "_v_", name);
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
    codegen_emit_u16_hex(gen, (uint16_t)value);
}

static void codegen_emit_u8_dec(codegen_t* gen, uint8_t value) {
    char buf[4];
    uint16_t i = 0;
    if (value == 0) {
        buf[i++] = '0';
    } else {
        char temp[4];
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

static void codegen_emit_u8_hex(codegen_t* gen, uint8_t value) {
    char buf[5];
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex[(value >> 4) & 0xF];
    buf[3] = hex[value & 0xF];
    buf[4] = '\0';
    codegen_emit(gen, buf);
}

static void codegen_emit_u16_hex(codegen_t* gen, uint16_t value) {
    char buf[7];
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex[(value >> 12) & 0xF];
    buf[3] = hex[(value >> 8) & 0xF];
    buf[4] = hex[(value >> 4) & 0xF];
    buf[5] = hex[value & 0xF];
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

static const char* codegen_stream_string(ast_reader_t* ast, uint16_t index) {
    return ast_reader_string(ast, index);
}

static bool codegen_stream_type_is_pointer(uint8_t depth) {
    return depth > 0;
}

static uint16_t codegen_stream_type_storage_size(uint8_t base, uint8_t depth) {
    return codegen_stream_type_is_16bit(base, depth) ? 2u : 1u;
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

static int16_t codegen_param_index_by_id(codegen_t* gen, uint16_t name_index) {
    if (!gen) return -1;
    for (size_t i = 0; i < gen->param_count; i++) {
        if (gen->param_name_indices[i] == name_index) {
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

static void codegen_record_local(codegen_t* gen, const char* name, uint16_t size,
                                 bool is_16bit) {
    if (!gen || !name) return;
    if (codegen_local_index(gen, name) >= 0) return;
    if (gen->local_var_count < (sizeof(gen->local_vars) / sizeof(gen->local_vars[0]))) {
        gen->local_vars[gen->local_var_count] = name;
        gen->local_offsets[gen->local_var_count] = gen->stack_offset;
        gen->local_is_16[gen->local_var_count] = is_16bit;
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

static uint8_t codegen_param_offset_by_id(codegen_t* gen, uint16_t name_index, int16_t* out_offset) {
    if (!gen || !out_offset) return 0;
    int16_t idx = codegen_param_index_by_id(gen, name_index);
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

static bool codegen_local_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_local_index(gen, name);
    return idx >= 0 && gen->local_is_16[idx];
}

static bool codegen_param_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_param_index(gen, name);
    return idx >= 0 && gen->param_is_16[idx];
}

static bool codegen_global_is_16(codegen_t* gen, const char* name) {
    int16_t idx = codegen_global_index(gen, name);
    return idx >= 0 && gen->global_is_16[idx];
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
    codegen_emit(gen, CG_STR_NL);
    return CC_OK;
}

static cc_error_t codegen_load_pointer_to_hl(codegen_t* gen, const char* name) {
    int16_t offset = 0;
    if (codegen_local_offset(gen, name, &offset) || codegen_param_offset(gen, name, &offset)) {
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
    if (codegen_local_offset(gen, name, &offset) || codegen_param_offset(gen, name, &offset)) {
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

codegen_t* codegen_create(const char* output_file) {
    codegen_t* gen = (codegen_t*)cc_malloc(sizeof(codegen_t));
    if (!gen) return NULL;

    gen->output_handle = output_open(output_file);
#ifdef __SDCC
    if (gen->output_handle < 0) {
#else
    if (!gen->output_handle) {
#endif
        cc_free(gen);
        return NULL;
    }

    gen->label_counter = 0;
    gen->stack_offset = 0;
    gen->local_var_count = 0;
    gen->param_count = 0;
    gen->global_count = 0;
    gen->function_end_label = NULL;
    gen->return_direct = false;
    gen->use_function_end_label = false;
    gen->function_return_is_16 = false;
    gen->function_count = 0;
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
    for (uint16_t pos = 0; pos < 6; pos++) {
        uint16_t digit = n % 10;
        label[i + 5 - pos] = (char)('0' + digit);
        n /= 10;
    }
    i += 6;
    label[i] = '\0';
    return label;
}

char* codegen_new_string_label(codegen_t* gen) {
    static char labels[8][16];
    static uint8_t slot = 0;
    char* label = labels[slot++ & 7];
    uint16_t n = (uint16_t)gen->string_count;
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
static cc_error_t codegen_stream_expression_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag);
static cc_error_t codegen_stream_statement_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag);
static int8_t codegen_stream_collect_locals(codegen_t* gen, ast_reader_t* ast);
static cc_error_t codegen_stream_function(codegen_t* gen, ast_reader_t* ast);
static cc_error_t codegen_stream_global_var(codegen_t* gen, ast_reader_t* ast);

static uint32_t g_arg_offsets[8];
static const char* g_param_names[8];
static bool g_param_is_16[8];
static uint16_t g_param_name_indices[8];
/* Indicates whether the last expression emitted left its result in HL (true)
   or in A (false). Used by call-site code to decide how to push arguments. */
static bool g_result_in_hl = false;
/* Forces expression emission to produce a 16-bit result in HL when true. */
static bool g_expect_result_in_hl = false;

static bool codegen_stream_type_is_16bit(uint8_t base, uint8_t depth) {
    if (depth > 0) return true;
    return base == AST_BASE_INT;
}

static bool codegen_function_return_is_16bit(codegen_t* gen, uint16_t name_index) {
    if (!gen) return false;
    for (size_t i = 0; i < gen->function_count; i++) {
        uint16_t flags = gen->function_return_flags[i];
        if ((uint16_t)(flags & 0x7FFFu) == name_index) {
            return (flags & 0x8000u) != 0;
        }
    }
    return false;
}

static void codegen_register_function_return(codegen_t* gen, uint16_t name_index, bool is_16bit) {
    if (!gen) return;
    for (size_t i = 0; i < gen->function_count; i++) {
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
            const char* name = codegen_stream_string(ast, name_index);
            if (!name) return CC_ERROR_CODEGEN;
            int16_t offset = 0;
            {
                bool is_16bit = codegen_local_is_16(gen, name) ||
                                codegen_param_is_16(gen, name) ||
                                codegen_global_is_16(gen, name);
                if (is_16bit) {
                    if (codegen_local_offset(gen, name, &offset)) {
                        codegen_emit(gen, "  ld a, (ix");
                        codegen_emit_ix_offset(gen, offset);
                        codegen_emit(gen, ")\n  ld l, a\n  ld h, (ix");
                        codegen_emit_ix_offset(gen, offset + 1);
                        codegen_emit(gen, ")\n");
                    } else if (codegen_param_offset_by_id(gen, name_index, &offset) ||
                               codegen_param_offset(gen, name, &offset)) {
                        codegen_emit(gen, "  ld a, (ix");
                        codegen_emit_ix_offset(gen, offset);
                        codegen_emit(gen, ")\n  ld l, a\n  ld h, (ix");
                        codegen_emit_ix_offset(gen, offset + 1);
                        codegen_emit(gen, ")\n");
                    } else {
                        codegen_emit(gen, CG_STR_LD_HL_PAREN);
                        codegen_emit_mangled_var(gen, name);
                        codegen_emit(gen, CG_STR_RPAREN_NL);
                        codegen_emit(gen, "  ld a, l\n");
                    }
                    g_result_in_hl = true;
                    return CC_OK;
                }
                if (codegen_local_offset(gen, name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, ")  ; local: ");
                } else if (codegen_param_offset_by_id(gen, name_index, &offset)) {
                    codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, ")  ; param: ");
                } else if (codegen_param_offset(gen, name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_A_IX_PREFIX);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, ")  ; param: ");
                } else {
                    codegen_emit(gen, CG_STR_LD_A_LPAREN);
                    codegen_emit_mangled_var(gen, name);
                    codegen_emit(gen, ")  ; variable: ");
                }
                codegen_emit(gen, name);
                codegen_emit(gen, CG_STR_NL);
                if (g_expect_result_in_hl) {
                    codegen_emit(gen, "  ld l, a\n  ld h, 0\n");
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
            ast_reader_skip_tag(ast, child_tag);
            cc_error("Address-of used without pointer assignment");
            return CC_ERROR_CODEGEN;
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = 0;
            if (ast_read_u8(ast->reader, &op) < 0) return CC_ERROR_CODEGEN;
            uint8_t left_tag = 0;
            if (ast_read_u8(ast->reader, &left_tag) < 0) return CC_ERROR_CODEGEN;
            if (g_expect_result_in_hl) {
                bool prev_expect = g_expect_result_in_hl;
                g_expect_result_in_hl = true;
                cc_error_t err = codegen_stream_expression_tag(gen, ast, left_tag);
                if (err != CC_OK) return err;
                if (!g_result_in_hl) {
                    codegen_emit(gen, "  ld l, a\n  ld h, 0\n");
                }
                codegen_emit(gen, CG_STR_PUSH_HL);
                uint8_t right_tag = 0;
                if (ast_read_u8(ast->reader, &right_tag) < 0) return CC_ERROR_CODEGEN;
                g_expect_result_in_hl = true;
                err = codegen_stream_expression_tag(gen, ast, right_tag);
                g_expect_result_in_hl = prev_expect;
                if (err != CC_OK) return err;
                if (!g_result_in_hl) {
                    codegen_emit(gen, "  ld l, a\n  ld h, 0\n");
                }
                codegen_emit(gen, "  pop de\n");
                switch (op) {
                    case OP_ADD:
                        codegen_emit(gen, "  add hl, de\n");
                        break;
                    case OP_SUB:
                        codegen_emit(gen, "  ex de, hl\n  or a\n  sbc hl, de\n");
                        break;
                    case OP_MUL:
                        codegen_emit(gen,
                            "; (DE * HL)\n"
                            "  ex de, hl\n"
                            "  call __mul_hl_de\n");
                        break;
                    case OP_DIV:
                        codegen_emit(gen,
                            "; (DE / HL)\n"
                            "  ex de, hl\n"
                            "  call __div_hl_de\n");
                        break;
                    case OP_MOD:
                        codegen_emit(gen,
                            "; (DE % HL)\n"
                            "  ex de, hl\n"
                            "  call __mod_hl_de\n");
                        break;
                    case OP_EQ: {
                        char* end = codegen_new_label(gen);
                        char* set = codegen_new_label(gen);
                        codegen_emit(gen, "  ex de, hl\n  or a\n  sbc hl, de\n");
                        codegen_emit(gen, "  ld hl, 0\n");
                        codegen_emit_jump(gen, CG_STR_JR_Z, set);
                        codegen_emit_jump(gen, CG_STR_JR, end);
                        codegen_emit_label(gen, set);
                        codegen_emit(gen, "  ld hl, 1\n");
                        codegen_emit_label(gen, end);
                        break;
                    }
                    case OP_NE: {
                        char* end = codegen_new_label(gen);
                        char* set = codegen_new_label(gen);
                        codegen_emit(gen, "  ex de, hl\n  or a\n  sbc hl, de\n");
                        codegen_emit(gen, "  ld hl, 0\n");
                        codegen_emit_jump(gen, CG_STR_JR_NZ, set);
                        codegen_emit_jump(gen, CG_STR_JR, end);
                        codegen_emit_label(gen, set);
                        codegen_emit(gen, "  ld hl, 1\n");
                        codegen_emit_label(gen, end);
                        break;
                    }
                    case OP_LT: {
                        char* end = codegen_new_label(gen);
                        char* set = codegen_new_label(gen);
                        codegen_emit(gen, "  ex de, hl\n  or a\n  sbc hl, de\n");
                        codegen_emit(gen, "  ld hl, 0\n");
                        codegen_emit_jump(gen, CG_STR_JR_C, set);
                        codegen_emit_jump(gen, CG_STR_JR, end);
                        codegen_emit_label(gen, set);
                        codegen_emit(gen, "  ld hl, 1\n");
                        codegen_emit_label(gen, end);
                        break;
                    }
                    case OP_GT: {
                        char* end = codegen_new_label(gen);
                        codegen_emit(gen, "  ex de, hl\n  or a\n  sbc hl, de\n");
                        codegen_emit(gen, "  ld hl, 0\n");
                        codegen_emit_jump(gen, CG_STR_JR_Z, end);
                        codegen_emit_jump(gen, CG_STR_JR_C, end);
                        codegen_emit(gen, "  ld hl, 1\n");
                        codegen_emit_label(gen, end);
                        break;
                    }
                    case OP_LE: {
                        char* end = codegen_new_label(gen);
                        char* set = codegen_new_label(gen);
                        codegen_emit(gen, "  ex de, hl\n  or a\n  sbc hl, de\n");
                        codegen_emit(gen, "  ld hl, 0\n");
                        codegen_emit_jump(gen, CG_STR_JR_Z, set);
                        codegen_emit_jump(gen, CG_STR_JR_C, set);
                        codegen_emit_jump(gen, CG_STR_JR, end);
                        codegen_emit_label(gen, set);
                        codegen_emit(gen, "  ld hl, 1\n");
                        codegen_emit_label(gen, end);
                        break;
                    }
                    case OP_GE: {
                        char* end = codegen_new_label(gen);
                        codegen_emit(gen, "  ex de, hl\n  or a\n  sbc hl, de\n");
                        codegen_emit(gen, "  ld hl, 0\n");
                        codegen_emit_jump(gen, CG_STR_JR_C, end);
                        codegen_emit(gen, "  ld hl, 1\n");
                        codegen_emit_label(gen, end);
                        break;
                    }
                    default:
                        return CC_ERROR_CODEGEN;
                }
                g_result_in_hl = true;
                return CC_OK;
            } else {
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
                            "; (A * L)\n"
                            "  call __mul_a_l\n");
                        break;
                    case OP_DIV:
                        codegen_emit(gen,
                            "; (A / L)\n"
                            "  call __div_a_l\n");
                        break;
                    case OP_MOD:
                        codegen_emit(gen,
                            "; (A % L)\n"
                            "  call __mod_a_l\n");
                        break;
                    case OP_EQ:
                        codegen_emit(gen,
                            "; (A == L)\n"
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
                            "; (A != L)\n"
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
                            "; (A < L)\n"
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
                            "; (A > L)\n"
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
                            "; (A <= L)\n"
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
                            "; (A >= L)\n"
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
                g_result_in_hl = false;
                return CC_OK;
            }
        }
        case AST_TAG_CALL: {
            uint16_t name_index = 0;
            uint8_t arg_count = 0;
            if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &arg_count) < 0) return CC_ERROR_CODEGEN;
            const char* name = codegen_stream_string(ast, name_index);
            if (!name) return CC_ERROR_CODEGEN;
            codegen_emit(gen, "; call: ");
            codegen_emit(gen, name);
            codegen_emit(gen, CG_STR_NL);

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
                codegen_emit(gen, "  ld a, l\n");
                g_result_in_hl = false;
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
            // Support both constant and variable index
            if (index_tag == AST_TAG_CONSTANT) {
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
                    g_result_in_hl = false;
                    return CC_OK;
                }
                if (base_name) {
                    if (!codegen_local_is_16(gen, base_name) &&
                        !codegen_param_is_16(gen, base_name) &&
                        !codegen_global_is_16(gen, base_name)) {
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
                    g_result_in_hl = false;
                    return CC_OK;
                }
                cc_error("Unsupported array access");
                return CC_ERROR_CODEGEN;
            } else {
                // Variable index: emit code for index expression, add to base pointer
                // Evaluate index expression, result in A
                cc_error_t err = codegen_stream_expression_tag(gen, ast, index_tag);
                if (err != CC_OK) return err;
                // Move index (A) to E, clear D
                codegen_emit(gen, "  ld e, a\n  ld d, 0\n");
                // Load base pointer to HL
                if (base_string) {
                    const char* label = codegen_get_string_label(gen, base_string);
                    if (!label) return CC_ERROR_CODEGEN;
                    codegen_emit(gen, CG_STR_LD_HL);
                    codegen_emit(gen, label);
                    codegen_emit(gen, CG_STR_NL);
                } else if (base_name) {
                    if (!codegen_local_is_16(gen, base_name) &&
                        !codegen_param_is_16(gen, base_name) &&
                        !codegen_global_is_16(gen, base_name)) {
                        cc_error("Unsupported array access");
                        return CC_ERROR_CODEGEN;
                    }
                    err = codegen_load_pointer_to_hl(gen, base_name);
                    if (err != CC_OK) return err;
                } else {
                    cc_error("Unsupported array access");
                    return CC_ERROR_CODEGEN;
                }
                // Add index (DE) to base pointer (HL)
                codegen_emit(gen, CG_STR_ADD_HL_DE);
                // Load value at HL into A
                codegen_emit(gen, CG_STR_LD_A_HL);
                g_result_in_hl = false;
                return CC_OK;
            }
        }
        case AST_TAG_ASSIGN: {
            uint8_t ltag = 0;
            uint8_t rtag = 0;
            uint8_t op = 0;
            const char* lvalue_name = NULL;
            const char* rvalue_name = NULL;
            const char* rvalue_string = NULL;
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
                (codegen_local_is_16(gen, lvalue_name) ||
                 codegen_param_is_16(gen, lvalue_name) ||
                 codegen_global_is_16(gen, lvalue_name))) {
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
            }

            bool lvalue_is_16 = lvalue_name &&
                                (codegen_local_is_16(gen, lvalue_name) ||
                                 codegen_param_is_16(gen, lvalue_name) ||
                                 codegen_global_is_16(gen, lvalue_name));
            bool prev_expect = g_expect_result_in_hl;
            bool expect_hl = lvalue_is_16 &&
                             (rtag == AST_TAG_CONSTANT ||
                              rtag == AST_TAG_IDENTIFIER ||
                              rtag == AST_TAG_CALL ||
                              rtag == AST_TAG_BINARY_OP);
            g_expect_result_in_hl = expect_hl;
            cc_error_t err = codegen_stream_expression_tag(gen, ast, rtag);
            g_expect_result_in_hl = prev_expect;
            if (err != CC_OK) return err;
            if (lvalue_name) {
                int16_t offset = 0;
                if (lvalue_is_16) {
                    if (!g_result_in_hl) {
                        codegen_emit(gen, "  ld l, a\n  ld h, 0\n");
                    }
                    if (codegen_local_offset(gen, lvalue_name, &offset)) {
                        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                        codegen_emit_ix_offset(gen, offset);
                        codegen_emit(gen, CG_STR_RPAREN_L);
                        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                        codegen_emit_ix_offset(gen, offset + 1);
                        codegen_emit(gen, CG_STR_RPAREN_H);
                    } else if (codegen_param_offset(gen, lvalue_name, &offset)) {
                        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                        codegen_emit_ix_offset(gen, offset);
                        codegen_emit(gen, CG_STR_RPAREN_L);
                        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                        codegen_emit_ix_offset(gen, offset + 1);
                        codegen_emit(gen, CG_STR_RPAREN_H);
                    } else {
                        codegen_emit(gen, CG_STR_LD_LPAREN);
                        codegen_emit_mangled_var(gen, lvalue_name);
                        codegen_emit(gen, CG_STR_RPAREN_HL);
                    }
                } else if (codegen_local_offset(gen, lvalue_name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                    codegen_emit_ix_offset(gen, offset);
                    codegen_emit(gen, CG_STR_RPAREN_A);
                } else if (codegen_param_offset(gen, lvalue_name, &offset)) {
                    codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                    codegen_emit_ix_offset(gen, offset);
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

static cc_error_t codegen_stream_statement_tag(codegen_t* gen, ast_reader_t* ast, uint8_t tag) {
    switch (tag) {
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = 0;
            if (ast_read_u8(ast->reader, &has_expr) < 0) return CC_ERROR_CODEGEN;
            if (has_expr) {
                uint8_t expr_tag = 0;
                if (ast_read_u8(ast->reader, &expr_tag) < 0) return CC_ERROR_CODEGEN;
                bool prev_expect = g_expect_result_in_hl;
                bool expect_hl = gen->function_return_is_16 &&
                                 (expr_tag == AST_TAG_CONSTANT ||
                                  expr_tag == AST_TAG_IDENTIFIER ||
                                  expr_tag == AST_TAG_CALL ||
                                  expr_tag == AST_TAG_BINARY_OP);
                g_expect_result_in_hl = expect_hl;
                cc_error_t err = codegen_stream_expression_tag(gen, ast, expr_tag);
                g_expect_result_in_hl = prev_expect;
                if (err != CC_OK) return err;
                if (gen->function_return_is_16) {
                    if (!g_result_in_hl) {
                        codegen_emit(gen, "  ld l, a\n  ld h, 0\n");
                        g_result_in_hl = true;
                    }
                } else if (g_result_in_hl) {
                    codegen_emit(gen, "  ld a, l\n");
                    g_result_in_hl = false;
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
            codegen_emit(gen, "; var: ");
            codegen_emit(gen, name);
            codegen_emit(gen, CG_STR_NL);
            if (has_init) {
                uint8_t init_tag = 0;
                if (ast_read_u8(ast->reader, &init_tag) < 0) return CC_ERROR_CODEGEN;
                bool is_pointer = codegen_stream_type_is_pointer(depth);
                bool is_16bit = codegen_stream_type_is_16bit(base, depth);
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
                        if (codegen_local_is_16(gen, ident) ||
                            codegen_param_is_16(gen, ident) ||
                            codegen_global_is_16(gen, ident)) {
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
                bool prev_expect = g_expect_result_in_hl;
                bool expect_hl = is_16bit &&
                                 (init_tag == AST_TAG_CONSTANT ||
                                  init_tag == AST_TAG_IDENTIFIER ||
                                  init_tag == AST_TAG_CALL);
                g_expect_result_in_hl = expect_hl;
                cc_error_t err = codegen_stream_expression_tag(gen, ast, init_tag);
                g_expect_result_in_hl = prev_expect;
                if (err != CC_OK) return err;
                int16_t offset = 0;
                if (is_16bit) {
                    if (!g_result_in_hl) {
                        codegen_emit(gen, "  ld l, a\n  ld h, 0\n");
                    }
                    if (codegen_local_offset(gen, name, &offset)) {
                        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                        codegen_emit_ix_offset(gen, offset);
                        codegen_emit(gen, CG_STR_RPAREN_L);
                        codegen_emit(gen, CG_STR_LD_IX_PREFIX);
                        codegen_emit_ix_offset(gen, offset + 1);
                        codegen_emit(gen, CG_STR_RPAREN_H);
                    } else {
                        codegen_emit(gen, CG_STR_LD_LPAREN);
                        codegen_emit_mangled_var(gen, name);
                        codegen_emit(gen, CG_STR_RPAREN_HL);
                    }
                } else {
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
            char* else_label = NULL;
            char* end_label = NULL;
            if (ast_read_u8(ast->reader, &has_else) < 0) return CC_ERROR_CODEGEN;
            uint8_t cond_tag = 0;
            if (ast_read_u8(ast->reader, &cond_tag) < 0) return CC_ERROR_CODEGEN;
            cc_error_t err = codegen_stream_expression_tag(gen, ast, cond_tag);
            if (err != CC_OK) return err;
            if (has_else) {
                else_label = codegen_new_label_persist(gen);
                end_label = codegen_new_label_persist(gen);
                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, else_label);
                uint8_t then_tag = 0;
                if (ast_read_u8(ast->reader, &then_tag) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto if_cleanup;
                }
                err = codegen_stream_statement_tag(gen, ast, then_tag);
                if (err != CC_OK) {
                    goto if_cleanup;
                }
                codegen_emit_jump(gen, CG_STR_JP, end_label);
                codegen_emit_label(gen, else_label);
                uint8_t else_tag = 0;
                if (ast_read_u8(ast->reader, &else_tag) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto if_cleanup;
                }
                err = codegen_stream_statement_tag(gen, ast, else_tag);
                if (err != CC_OK) {
                    goto if_cleanup;
                }
                codegen_emit_label(gen, end_label);
            } else {
                end_label = codegen_new_label_persist(gen);
                codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
                uint8_t then_tag = 0;
                if (ast_read_u8(ast->reader, &then_tag) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto if_cleanup;
                }
                err = codegen_stream_statement_tag(gen, ast, then_tag);
                if (err != CC_OK) {
                    goto if_cleanup;
                }
                codegen_emit_label(gen, end_label);
            }
            err = CC_OK;
if_cleanup:
            if (else_label) cc_free(else_label);
            if (end_label) cc_free(end_label);
            return err;
        }
        case AST_TAG_WHILE_STMT: {
            char* loop_label = codegen_new_label_persist(gen);
            char* end_label = codegen_new_label_persist(gen);
            cc_error_t err = CC_OK;
            codegen_emit_label(gen, loop_label);
            uint8_t cond_tag = 0;
            if (ast_read_u8(ast->reader, &cond_tag) < 0) {
                err = CC_ERROR_CODEGEN;
                goto while_cleanup;
            }
            err = codegen_stream_expression_tag(gen, ast, cond_tag);
            if (err != CC_OK) {
                goto while_cleanup;
            }
            codegen_emit_jump(gen, CG_STR_OR_A_JP_Z, end_label);
            uint8_t body_tag = 0;
            if (ast_read_u8(ast->reader, &body_tag) < 0) {
                err = CC_ERROR_CODEGEN;
                goto while_cleanup;
            }
            err = codegen_stream_statement_tag(gen, ast, body_tag);
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
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = 0;
            uint8_t has_cond = 0;
            uint8_t has_inc = 0;
            uint32_t inc_offset = 0;
            uint32_t body_end = 0;
            if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &has_cond) < 0) return CC_ERROR_CODEGEN;
            if (ast_read_u8(ast->reader, &has_inc) < 0) return CC_ERROR_CODEGEN;
            char* loop_label = codegen_new_label_persist(gen);
            char* end_label = codegen_new_label_persist(gen);
            cc_error_t err;
            if (has_init) {
                uint8_t init_tag = 0;
                if (ast_read_u8(ast->reader, &init_tag) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto for_cleanup;
                }
                err = codegen_stream_statement_tag(gen, ast, init_tag);
                if (err != CC_OK) {
                    goto for_cleanup;
                }
            }
            codegen_emit_label(gen, loop_label);
            if (has_cond) {
                uint8_t cond_tag = 0;
                if (ast_read_u8(ast->reader, &cond_tag) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto for_cleanup;
                }
                err = codegen_stream_expression_tag(gen, ast, cond_tag);
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
            {
                uint8_t body_tag = 0;
                if (ast_read_u8(ast->reader, &body_tag) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto for_cleanup;
                }
                err = codegen_stream_statement_tag(gen, ast, body_tag);
            }
            if (err != CC_OK) {
                goto for_cleanup;
            }
            body_end = reader_tell(ast->reader);
            if (has_inc) {
                if (reader_seek(ast->reader, inc_offset) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto for_cleanup;
                }
                uint8_t inc_tag = 0;
                if (ast_read_u8(ast->reader, &inc_tag) < 0) {
                    err = CC_ERROR_CODEGEN;
                    goto for_cleanup;
                }
                err = codegen_stream_expression_tag(gen, ast, inc_tag);
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
            bool is_16bit = codegen_stream_type_is_16bit(base, depth);
            codegen_record_local(gen, name,
                                 codegen_stream_type_storage_size(base, depth),
                                 is_16bit);
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
    bool* param_is_16 = g_param_is_16;
    uint16_t* param_name_indices = g_param_name_indices;
    uint8_t param_used = 0;
    uint32_t body_start = 0;
    uint32_t body_end = 0;
    bool last_was_return = false;

    if (ast_read_u16(ast->reader, &name_index) < 0) return CC_ERROR_CODEGEN;
    if (ast_reader_read_type_info(ast, &base, &depth) < 0) return CC_ERROR_CODEGEN;
    if (ast_read_u8(ast->reader, &param_count) < 0) return CC_ERROR_CODEGEN;
    name = codegen_stream_string(ast, name_index);
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
        uint8_t has_init = 0;
        if (ast_read_u8(ast->reader, &tag) < 0) return CC_ERROR_CODEGEN;
        if (tag != AST_TAG_VAR_DECL) return CC_ERROR_CODEGEN;
        if (ast_read_u16(ast->reader, &param_name_index) < 0) return CC_ERROR_CODEGEN;
        if (ast_reader_read_type_info(ast, &param_base, &param_depth) < 0) return CC_ERROR_CODEGEN;
        if (ast_read_u8(ast->reader, &has_init) < 0) return CC_ERROR_CODEGEN;
        if (has_init && ast_reader_skip_node(ast) < 0) return CC_ERROR_CODEGEN;
        if (param_used < (uint8_t)(sizeof(g_param_names) / sizeof(g_param_names[0]))) {
            param_names[param_used] = codegen_stream_string(ast, param_name_index);
        param_is_16[param_used] = codegen_stream_type_is_16bit(param_base, param_depth);
        param_name_indices[param_used] = param_name_index;
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
        gen->param_is_16[gen->param_count] = param_is_16[i];
        gen->param_name_indices[gen->param_count] = param_name_indices[i];
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
    bool is_16bit = codegen_stream_type_is_16bit(base, depth);
    codegen_emit(gen, "; glob: ");
    codegen_emit(gen, name);
    codegen_emit(gen, CG_STR_NL);
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
                codegen_emit(gen, CG_STR_COLON);
                codegen_emit(gen, CG_STR_DW);
                codegen_emit_int(gen, 0);
                return CC_OK;
            }
            if (tag == AST_TAG_CONSTANT) {
                int16_t value = 0;
                if (ast_read_i16(ast->reader, &value) < 0) return CC_ERROR_CODEGEN;
                if (value == 0) {
                    codegen_emit(gen, CG_STR_COLON);
                    codegen_emit(gen, CG_STR_DW);
                    codegen_emit_int(gen, 0);
                    return CC_OK;
                }
            } else {
                ast_reader_skip_tag(ast, tag);
            }
        }
        codegen_emit(gen, CG_STR_COLON);
        codegen_emit(gen, CG_STR_DW);
        codegen_emit_int(gen, 0);
        return CC_OK;
    }

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
    codegen_emit(gen, "; Run-time Library\n");
    codegen_emit_file(gen, "runtime/putchar.asm");
    codegen_emit_file(gen, "runtime/math_8.asm");
    codegen_emit_file(gen, "runtime/math_16.asm");
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
        // Emit .dm "String"
        codegen_emit(gen, ".dm \"");
        // Output string contents, escaping as needed
        for (const char* p = value; *p; ++p) {
            if (*p == '"' || *p == '\\' || *p == '\n') {
                codegen_emit(gen, "\\");
            }
            char c;
            switch(*p) {
                case '\n': c = 'n'; break;
                default: c = *p;
            }
            char buf[2] = {c, 0};
            codegen_emit(gen, buf);
        }
        codegen_emit(gen, "\"\n");
        // Emit .db 0
        codegen_emit(gen, ".db 0\n");
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
                    gen->global_is_16[gen->global_count] = codegen_stream_type_is_16bit(base, depth);
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
