#include "ast_format.h"
#include "ast_io.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "symbol.h"
#include "target.h"
#include "cc_compat.h"

#define MAX_AST_STRINGS 512

typedef struct {
    output_t out;
    uint16_t node_count;
    uint16_t decl_count;
    uint16_t string_count;
    uint32_t header_node_count_offset;
    uint32_t header_string_count_offset;
    uint32_t header_string_table_offset;
    uint32_t program_decl_count_offset;
    const char* strings[MAX_AST_STRINGS];
} ast_writer_t;

static int16_t ast_string_index(ast_writer_t* writer, const char* value) {
    if (!writer || !value) return -1;
    for (uint16_t i = 0; i < writer->string_count; i++) {
        if (str_cmp(writer->strings[i], value) == 0) {
            return (int16_t)i;
        }
    }
    if (writer->string_count >= MAX_AST_STRINGS) {
        cc_error("AST string table overflow");
        return -1;
    }
    char* copy = cc_strdup(value);
    if (!copy) return -1;
    writer->strings[writer->string_count] = copy;
    return (int16_t)(writer->string_count++);
}

static void ast_free_strings(ast_writer_t* writer) {
    if (!writer) return;
    for (uint16_t i = 0; i < writer->string_count; i++) {
        cc_free((void*)writer->strings[i]);
        writer->strings[i] = NULL;
    }
    writer->string_count = 0;
}

static int8_t ast_write_type(ast_writer_t* writer, const type_t* type) {
    const type_t* cur = type;
    uint8_t depth = 0;
    while (cur && cur->kind == TYPE_POINTER) {
        depth++;
        cur = cur->data.pointer.base_type;
    }
    if (!cur) return -1;

    uint8_t base = 0;
    switch (cur->kind) {
        case TYPE_INT:
            base = AST_BASE_INT;
            break;
        case TYPE_CHAR:
            base = AST_BASE_CHAR;
            break;
        case TYPE_VOID:
            base = AST_BASE_VOID;
            break;
        default:
            cc_error("Unsupported type in AST writer");
            return -1;
    }

    if (ast_write_u8(writer->out, base) < 0) return -1;
    if (ast_write_u8(writer->out, depth) < 0) return -1;
    return 0;
}

static int8_t ast_write_node(ast_writer_t* writer, const ast_node_t* node) {
    if (!writer || !node) return -1;
    writer->node_count++;

    switch (node->type) {
        case AST_FUNCTION: {
            int16_t name_index = ast_string_index(writer, node->data.function.name);
            if (name_index < 0) return -1;
            if (ast_write_u8(writer->out, AST_TAG_FUNCTION) < 0) return -1;
            if (ast_write_u16(writer->out, (uint16_t)name_index) < 0) return -1;
            if (ast_write_type(writer, node->data.function.return_type) < 0) return -1;
            if (ast_write_u8(writer->out, (uint8_t)node->data.function.param_count) < 0) return -1;
            for (size_t i = 0; i < node->data.function.param_count; i++) {
                if (ast_write_node(writer, node->data.function.params[i]) < 0) return -1;
            }
            return ast_write_node(writer, node->data.function.body);
        }
        case AST_VAR_DECL: {
            int16_t name_index = ast_string_index(writer, node->data.var_decl.name);
            if (name_index < 0) return -1;
            if (ast_write_u8(writer->out, AST_TAG_VAR_DECL) < 0) return -1;
            if (ast_write_u16(writer->out, (uint16_t)name_index) < 0) return -1;
            if (ast_write_type(writer, node->data.var_decl.var_type) < 0) return -1;
            if (node->data.var_decl.initializer) {
                if (ast_write_u8(writer->out, 1) < 0) return -1;
                return ast_write_node(writer, node->data.var_decl.initializer);
            }
            return ast_write_u8(writer->out, 0);
        }
        case AST_COMPOUND_STMT: {
            if (ast_write_u8(writer->out, AST_TAG_COMPOUND_STMT) < 0) return -1;
            if (ast_write_u16(writer->out, (uint16_t)node->data.compound.stmt_count) < 0) return -1;
            for (size_t i = 0; i < node->data.compound.stmt_count; i++) {
                if (ast_write_node(writer, node->data.compound.statements[i]) < 0) return -1;
            }
            return 0;
        }
        case AST_RETURN_STMT: {
            if (ast_write_u8(writer->out, AST_TAG_RETURN_STMT) < 0) return -1;
            if (node->data.return_stmt.expr) {
                if (ast_write_u8(writer->out, 1) < 0) return -1;
                return ast_write_node(writer, node->data.return_stmt.expr);
            }
            return ast_write_u8(writer->out, 0);
        }
        case AST_IF_STMT: {
            if (ast_write_u8(writer->out, AST_TAG_IF_STMT) < 0) return -1;
            if (ast_write_u8(writer->out, node->data.if_stmt.else_branch ? 1 : 0) < 0) return -1;
            if (ast_write_node(writer, node->data.if_stmt.condition) < 0) return -1;
            if (ast_write_node(writer, node->data.if_stmt.then_branch) < 0) return -1;
            if (node->data.if_stmt.else_branch) {
                return ast_write_node(writer, node->data.if_stmt.else_branch);
            }
            return 0;
        }
        case AST_WHILE_STMT: {
            if (ast_write_u8(writer->out, AST_TAG_WHILE_STMT) < 0) return -1;
            if (ast_write_node(writer, node->data.while_stmt.condition) < 0) return -1;
            return ast_write_node(writer, node->data.while_stmt.body);
        }
        case AST_FOR_STMT: {
            if (ast_write_u8(writer->out, AST_TAG_FOR_STMT) < 0) return -1;
            if (ast_write_u8(writer->out, node->data.for_stmt.init ? 1 : 0) < 0) return -1;
            if (ast_write_u8(writer->out, node->data.for_stmt.condition ? 1 : 0) < 0) return -1;
            if (ast_write_u8(writer->out, node->data.for_stmt.increment ? 1 : 0) < 0) return -1;
            if (node->data.for_stmt.init) {
                if (ast_write_node(writer, node->data.for_stmt.init) < 0) return -1;
            }
            if (node->data.for_stmt.condition) {
                if (ast_write_node(writer, node->data.for_stmt.condition) < 0) return -1;
            }
            if (node->data.for_stmt.increment) {
                if (ast_write_node(writer, node->data.for_stmt.increment) < 0) return -1;
            }
            return ast_write_node(writer, node->data.for_stmt.body);
        }
        case AST_ASSIGN: {
            if (ast_write_u8(writer->out, AST_TAG_ASSIGN) < 0) return -1;
            if (ast_write_node(writer, node->data.assign.lvalue) < 0) return -1;
            return ast_write_node(writer, node->data.assign.rvalue);
        }
        case AST_CALL: {
            int16_t name_index = ast_string_index(writer, node->data.call.name);
            if (name_index < 0) return -1;
            if (ast_write_u8(writer->out, AST_TAG_CALL) < 0) return -1;
            if (ast_write_u16(writer->out, (uint16_t)name_index) < 0) return -1;
            if (ast_write_u8(writer->out, (uint8_t)node->data.call.arg_count) < 0) return -1;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (ast_write_node(writer, node->data.call.args[i]) < 0) return -1;
            }
            return 0;
        }
        case AST_BINARY_OP: {
            if (ast_write_u8(writer->out, AST_TAG_BINARY_OP) < 0) return -1;
            if (ast_write_u8(writer->out, (uint8_t)node->data.binary_op.op) < 0) return -1;
            if (ast_write_node(writer, node->data.binary_op.left) < 0) return -1;
            return ast_write_node(writer, node->data.binary_op.right);
        }
        case AST_UNARY_OP: {
            if (ast_write_u8(writer->out, AST_TAG_UNARY_OP) < 0) return -1;
            if (ast_write_u8(writer->out, (uint8_t)node->data.unary_op.op) < 0) return -1;
            return ast_write_node(writer, node->data.unary_op.operand);
        }
        case AST_IDENTIFIER: {
            int16_t name_index = ast_string_index(writer, node->data.identifier.name);
            if (name_index < 0) return -1;
            if (ast_write_u8(writer->out, AST_TAG_IDENTIFIER) < 0) return -1;
            return ast_write_u16(writer->out, (uint16_t)name_index);
        }
        case AST_CONSTANT: {
            if (ast_write_u8(writer->out, AST_TAG_CONSTANT) < 0) return -1;
            return ast_write_i16(writer->out, node->data.constant.int_value);
        }
        case AST_STRING_LITERAL: {
            int16_t value_index = ast_string_index(writer, node->data.string_literal.value);
            if (value_index < 0) return -1;
            if (ast_write_u8(writer->out, AST_TAG_STRING_LITERAL) < 0) return -1;
            return ast_write_u16(writer->out, (uint16_t)value_index);
        }
        case AST_ARRAY_ACCESS: {
            if (ast_write_u8(writer->out, AST_TAG_ARRAY_ACCESS) < 0) return -1;
            if (ast_write_node(writer, node->data.array_access.base) < 0) return -1;
            return ast_write_node(writer, node->data.array_access.index);
        }
        default:
            cc_error("Unsupported AST node in writer");
            return -1;
    }
}

static int8_t ast_write_header(ast_writer_t* writer) {
    if (output_write(writer->out, AST_MAGIC, 4) < 0) return -1;
    if (ast_write_u8(writer->out, AST_FORMAT_VERSION) < 0) return -1;
    if (ast_write_u8(writer->out, 0) < 0) return -1;
    if (ast_write_u16(writer->out, 0) < 0) return -1;
    writer->header_node_count_offset = output_tell(writer->out);
    if (ast_write_u16(writer->out, 0) < 0) return -1;
    writer->header_string_count_offset = output_tell(writer->out);
    if (ast_write_u16(writer->out, 0) < 0) return -1;
    writer->header_string_table_offset = output_tell(writer->out);
    if (ast_write_u32(writer->out, 0) < 0) return -1;
    return 0;
}

static int8_t ast_patch_u16(output_t out, uint32_t offset, uint16_t value) {
    if (output_seek(out, offset) < 0) return -1;
    return ast_write_u16(out, value);
}

static int8_t ast_patch_u32(output_t out, uint32_t offset, uint32_t value) {
    if (output_seek(out, offset) < 0) return -1;
    return ast_write_u32(out, value);
}

static int8_t ast_write_string_table(ast_writer_t* writer, uint32_t* out_offset) {
    if (!writer || !out_offset) return -1;
    *out_offset = output_tell(writer->out);
    for (uint16_t i = 0; i < writer->string_count; i++) {
        const char* str = writer->strings[i];
        uint16_t len = 0;
        while (str[len]) len++;
        if (ast_write_u16(writer->out, (uint16_t)len) < 0) return -1;
        if (len > 0) {
            if (output_write(writer->out, str, (uint16_t)len) < 0) return -1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    int8_t err = 1;
    args_t args;
    reader_t* reader = NULL;
    lexer_t* lexer = NULL;
    parser_t* parser = NULL;
    ast_node_t* ast = NULL;
    ast_writer_t writer = {0};
    uint32_t string_table_offset = 0;

    cc_reset_pool();

    g_ctx.verbose = false;
    g_ctx.optimize = false;

    args = parse_args(argc, argv);
    if (args.show_help) return 0;
    if (args.error) {
        log_error("Usage: cc_parse <input.c> <output.ast>\n");
        return 1;
    }

    reader = reader_open(args.input_file);
    if (!reader) return 1;

    lexer = lexer_create(args.input_file, reader);
    if (!lexer) goto cleanup_reader;

    parser = parser_create(lexer);
    if (!parser) goto cleanup_lexer;

    writer.out = output_open(args.output_file);
#ifdef __SDCC
    if (writer.out < 0) goto cleanup_parser;
#else
    if (!writer.out) goto cleanup_parser;
#endif

    if (ast_write_header(&writer) < 0) {
        log_error("Failed to write AST header\n");
        goto cleanup_output;
    }

    if (ast_write_u8(writer.out, AST_TAG_PROGRAM) < 0) {
        log_error("Failed to write AST program tag\n");
        goto cleanup_output;
    }
    writer.node_count++;
    writer.program_decl_count_offset = output_tell(writer.out);
    if (ast_write_u16(writer.out, 0) < 0) {
        log_error("Failed to write AST program decl count\n");
        goto cleanup_output;
    }

    while (1) {
        ast = parser_parse_next(parser);
        if (!ast) break;
        if (ast->type != AST_FUNCTION && ast->type != AST_VAR_DECL) {
            ast_node_destroy(ast);
            ast = NULL;
            continue;
        }
        if (ast_write_node(&writer, ast) < 0) {
            ast_node_destroy(ast);
            ast = NULL;
            log_error("Failed to write AST node\n");
            goto cleanup_output;
        }
        writer.decl_count++;
        ast_node_destroy(ast);
        ast = NULL;
    }

    if (parser->error_count > 0) {
        log_error("Parsing failed\n");
        goto cleanup_output;
    }

    if (ast_write_string_table(&writer, &string_table_offset) < 0) {
        log_error("Failed to write AST string table\n");
        goto cleanup_output;
    }

    if (ast_patch_u16(writer.out, writer.header_node_count_offset, writer.node_count) < 0) {
        log_error("Failed to patch AST node count\n");
        goto cleanup_output;
    }
    if (ast_patch_u16(writer.out, writer.header_string_count_offset, writer.string_count) < 0) {
        log_error("Failed to patch AST string count\n");
        goto cleanup_output;
    }
    if (ast_patch_u32(writer.out, writer.header_string_table_offset, string_table_offset) < 0) {
        log_error("Failed to patch AST string table offset\n");
        goto cleanup_output;
    }
    if (ast_patch_u16(writer.out, writer.program_decl_count_offset, writer.decl_count) < 0) {
        log_error("Failed to patch AST program decl count\n");
        goto cleanup_output;
    }

    log_msg(args.input_file);
    log_msg(" -> ");
    log_msg(args.output_file);
    log_msg("\n");

    err = 0;

cleanup_output:
    output_close(writer.out);
    ast_free_strings(&writer);
cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
cleanup_reader:
    reader_close(reader);
    return err;
}
