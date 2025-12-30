#include "ast_format.h"
#include "ast_io.h"
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "symbol.h"
#include "target.h"
#include "cc_compat.h"

#define MAX_AST_STRINGS 512
#ifndef CC_POOL_SIZE
#define CC_POOL_SIZE 12288 /* 12 KB pool */
#endif

typedef struct {
    output_t out;
    uint16_t node_count;
    uint16_t decl_count;
    uint16_t string_count;
    bool strings_frozen;
    uint32_t header_node_count_offset;
    uint32_t header_string_count_offset;
    uint32_t header_string_table_offset;
    uint32_t program_decl_count_offset;
    const char* strings[MAX_AST_STRINGS];
} ast_writer_t;

char g_memory_pool[CC_POOL_SIZE];
static ast_writer_t* writer;
static parser_t* parser;
static lexer_t* lexer;
static reader_t* reader;

static int16_t ast_string_index(ast_writer_t* writer, const char* value) {
    if (!writer || !value) return -1;
    for (uint16_t i = 0; i < writer->string_count; i++) {
        if (str_cmp(writer->strings[i], value) == 0) {
            return (int16_t)i;
        }
    }
    if (writer->strings_frozen) {
        cc_error("AST string table missing value");
        return -1;
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
    uint16_t array_len = 0;
    if (cur && cur->kind == TYPE_ARRAY) {
        array_len = (uint16_t)cur->data.array.length;
        cur = cur->data.array.element_type;
    }
    uint8_t depth = 0;
    while (cur && cur->kind == TYPE_POINTER) {
        depth++;
        cur = cur->data.pointer.base_type;
    }
    if (!cur) return -1;
    if (cur->kind == TYPE_ARRAY) {
        cc_error("Unsupported array type in AST writer");
        return -1;
    }

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

    ast_write_u8(writer->out, base);
    ast_write_u8(writer->out, depth);
    ast_write_u16(writer->out, array_len);
    return 0;
}

static int8_t ast_write_node(ast_writer_t* writer, const ast_node_t* node) {
    if (!writer || !node) return -1;
    writer->node_count++;

    switch (node->type) {
        case AST_FUNCTION: {
            int16_t name_index = ast_string_index(writer, node->data.function.name);
            if (name_index < 0) return -1;
            ast_write_u8(writer->out, AST_TAG_FUNCTION);
            ast_write_u16(writer->out, (uint16_t)name_index);
            ast_write_type(writer, node->data.function.return_type);
            ast_write_u8(writer->out, (uint8_t)node->data.function.param_count);
            for (ast_param_count_t i = 0; i < node->data.function.param_count; i++) {
                ast_write_node(writer, node->data.function.params[i]);
            }
            ast_write_node(writer, node->data.function.body);
            return 0;
        }
        case AST_VAR_DECL: {
            int16_t name_index = ast_string_index(writer, node->data.var_decl.name);
            if (name_index < 0) return -1;
            ast_write_u8(writer->out, AST_TAG_VAR_DECL);
            ast_write_u16(writer->out, (uint16_t)name_index);
            if (ast_write_type(writer, node->data.var_decl.var_type) < 0) return -1;
            if (node->data.var_decl.initializer) {
                ast_write_u8(writer->out, 1);
                return ast_write_node(writer, node->data.var_decl.initializer);
            }
            ast_write_u8(writer->out, 0);
            return 0;
        }
        case AST_COMPOUND_STMT: {
            ast_write_u8(writer->out, AST_TAG_COMPOUND_STMT);
            ast_write_u16(writer->out, (uint16_t)node->data.compound.stmt_count);
            for (ast_stmt_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                ast_write_node(writer, node->data.compound.statements[i]);
            }
            return 0;
        }
        case AST_RETURN_STMT: {
            ast_write_u8(writer->out, AST_TAG_RETURN_STMT);
            if (node->data.return_stmt.expr) {
                ast_write_u8(writer->out, 1);
                ast_write_node(writer, node->data.return_stmt.expr);
                return 0;
            }
            ast_write_u8(writer->out, 0);
            return 0;
        }
        case AST_IF_STMT: {
            ast_write_u8(writer->out, AST_TAG_IF_STMT);
            ast_write_u8(writer->out, node->data.if_stmt.else_branch ? 1 : 0);
            ast_write_node(writer, node->data.if_stmt.condition);
            ast_write_node(writer, node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) {
                ast_write_node(writer, node->data.if_stmt.else_branch);
                return 0;
            }
            return 0;
        }
        case AST_WHILE_STMT: {
            ast_write_u8(writer->out, AST_TAG_WHILE_STMT);
            if (ast_write_node(writer, node->data.while_stmt.condition) < 0) return -1;
            return ast_write_node(writer, node->data.while_stmt.body);
        }
        case AST_FOR_STMT: {
            ast_write_u8(writer->out, AST_TAG_FOR_STMT);
            ast_write_u8(writer->out, node->data.for_stmt.init ? 1 : 0);
            ast_write_u8(writer->out, node->data.for_stmt.condition ? 1 : 0);
            ast_write_u8(writer->out, node->data.for_stmt.increment ? 1 : 0);
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
            ast_write_u8(writer->out, AST_TAG_ASSIGN);
            if (ast_write_node(writer, node->data.assign.lvalue) < 0) return -1;
            return ast_write_node(writer, node->data.assign.rvalue);
        }
        case AST_CALL: {
            int16_t name_index = ast_string_index(writer, node->data.call.name);
            if (name_index < 0) return -1;
            ast_write_u8(writer->out, AST_TAG_CALL);
            ast_write_u16(writer->out, (uint16_t)name_index);
            ast_write_u8(writer->out, (uint8_t)node->data.call.arg_count);
            for (ast_arg_count_t i = 0; i < node->data.call.arg_count; i++) {
                if (ast_write_node(writer, node->data.call.args[i]) < 0) return -1;
            }
            return 0;
        }
        case AST_BINARY_OP: {
            ast_write_u8(writer->out, AST_TAG_BINARY_OP);
            ast_write_u8(writer->out, (uint8_t)node->data.binary_op.op);
            if (ast_write_node(writer, node->data.binary_op.left) < 0) return -1;
            return ast_write_node(writer, node->data.binary_op.right);
        }
        case AST_UNARY_OP: {
            ast_write_u8(writer->out, AST_TAG_UNARY_OP);
            ast_write_u8(writer->out, (uint8_t)node->data.unary_op.op);
            return ast_write_node(writer, node->data.unary_op.operand);
        }
        case AST_IDENTIFIER: {
            int16_t name_index = ast_string_index(writer, node->data.identifier.name);
            if (name_index < 0) return -1;
            ast_write_u8(writer->out, AST_TAG_IDENTIFIER);
            ast_write_u16(writer->out, (uint16_t)name_index);
            return 0;
        }
        case AST_CONSTANT: {
            ast_write_u8(writer->out, AST_TAG_CONSTANT);
            ast_write_i16(writer->out, node->data.constant.int_value);
            return 0;
        }
        case AST_STRING_LITERAL: {
            int16_t value_index = ast_string_index(writer, node->data.string_literal.value);
            if (value_index < 0) return -1;
            ast_write_u8(writer->out, AST_TAG_STRING_LITERAL);
            ast_write_u16(writer->out, (uint16_t)value_index);
            return 0;
        }
        case AST_ARRAY_ACCESS: {
            ast_write_u8(writer->out, AST_TAG_ARRAY_ACCESS);
            if (ast_write_node(writer, node->data.array_access.base) < 0) return -1;
            return ast_write_node(writer, node->data.array_access.index);
        }
        default:
            cc_error("Unsupported AST node in writer");
            return -1;
    }
}

static int8_t ast_write_header_full(
    ast_writer_t* writer,
    uint16_t node_count,
    uint16_t string_count,
    uint32_t string_table_offset
) {
    if (output_write(writer->out, AST_MAGIC, 4) < 0) return -1;
    ast_write_u8(writer->out, AST_FORMAT_VERSION);
    ast_write_u8(writer->out, 0);
    ast_write_u16(writer->out, 0);
    ast_write_u16(writer->out, node_count);
    ast_write_u16(writer->out, string_count);
    ast_write_u32(writer->out, string_table_offset);
    return 0;
}

static int8_t ast_write_string_table(ast_writer_t* writer, uint32_t* out_offset) {
    if (!writer || !out_offset) return -1;
    *out_offset = output_tell(writer->out);
    for (uint16_t i = 0; i < writer->string_count; i++) {
        const char* str = writer->strings[i];
        uint16_t len = 0;
        while (str[len]) len++;
        ast_write_u16(writer->out, (uint16_t)len);
        if (len > 0) {
            if (output_write(writer->out, str, (uint16_t)len) < 0) return -1;
        }
    }
    return 0;
}

static int8_t ast_measure_node(ast_writer_t* writer, const ast_node_t* node, uint32_t* out_size) {
    if (!writer || !node || !out_size) return -1;
    writer->node_count++;
    switch (node->type) {
        case AST_FUNCTION: {
            int16_t name_index = ast_string_index(writer, node->data.function.name);
            if (name_index < 0) return -1;
            uint32_t size = 1 + 2 + 4 + 1;
            for (ast_param_count_t i = 0; i < node->data.function.param_count; i++) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.function.params[i], &child_size) < 0) return -1;
                size += child_size;
            }
            {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.function.body, &child_size) < 0) return -1;
                size += child_size;
            }
            *out_size = size;
            return 0;
        }
        case AST_VAR_DECL: {
            int16_t name_index = ast_string_index(writer, node->data.var_decl.name);
            if (name_index < 0) return -1;
            uint32_t size = 1 + 2 + 4 + 1;
            if (node->data.var_decl.initializer) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.var_decl.initializer, &child_size) < 0) return -1;
                size += child_size;
            }
            *out_size = size;
            return 0;
        }
        case AST_COMPOUND_STMT: {
            uint32_t size = 1 + 2;
            for (ast_stmt_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.compound.statements[i], &child_size) < 0) return -1;
                size += child_size;
            }
            *out_size = size;
            return 0;
        }
        case AST_RETURN_STMT: {
            uint32_t size = 1 + 1;
            if (node->data.return_stmt.expr) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.return_stmt.expr, &child_size) < 0) return -1;
                size += child_size;
            }
            *out_size = size;
            return 0;
        }
        case AST_IF_STMT: {
            uint32_t size = 1 + 1;
            uint32_t child_size = 0;
            if (ast_measure_node(writer, node->data.if_stmt.condition, &child_size) < 0) return -1;
            size += child_size;
            if (ast_measure_node(writer, node->data.if_stmt.then_branch, &child_size) < 0) return -1;
            size += child_size;
            if (node->data.if_stmt.else_branch) {
                if (ast_measure_node(writer, node->data.if_stmt.else_branch, &child_size) < 0) return -1;
                size += child_size;
            }
            *out_size = size;
            return 0;
        }
        case AST_WHILE_STMT: {
            uint32_t size = 1;
            uint32_t child_size = 0;
            if (ast_measure_node(writer, node->data.while_stmt.condition, &child_size) < 0) return -1;
            size += child_size;
            if (ast_measure_node(writer, node->data.while_stmt.body, &child_size) < 0) return -1;
            size += child_size;
            *out_size = size;
            return 0;
        }
        case AST_FOR_STMT: {
            uint32_t size = 1 + 3;
            if (node->data.for_stmt.init) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.for_stmt.init, &child_size) < 0) return -1;
                size += child_size;
            }
            if (node->data.for_stmt.condition) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.for_stmt.condition, &child_size) < 0) return -1;
                size += child_size;
            }
            if (node->data.for_stmt.increment) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.for_stmt.increment, &child_size) < 0) return -1;
                size += child_size;
            }
            {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.for_stmt.body, &child_size) < 0) return -1;
                size += child_size;
            }
            *out_size = size;
            return 0;
        }
        case AST_ASSIGN: {
            uint32_t size = 1;
            uint32_t child_size = 0;
            if (ast_measure_node(writer, node->data.assign.lvalue, &child_size) < 0) return -1;
            size += child_size;
            if (ast_measure_node(writer, node->data.assign.rvalue, &child_size) < 0) return -1;
            size += child_size;
            *out_size = size;
            return 0;
        }
        case AST_CALL: {
            int16_t name_index = ast_string_index(writer, node->data.call.name);
            if (name_index < 0) return -1;
            uint32_t size = 1 + 2 + 1;
            for (ast_arg_count_t i = 0; i < node->data.call.arg_count; i++) {
                uint32_t child_size = 0;
                if (ast_measure_node(writer, node->data.call.args[i], &child_size) < 0) return -1;
                size += child_size;
            }
            *out_size = size;
            return 0;
        }
        case AST_BINARY_OP: {
            uint32_t size = 1 + 1;
            uint32_t child_size = 0;
            if (ast_measure_node(writer, node->data.binary_op.left, &child_size) < 0) return -1;
            size += child_size;
            if (ast_measure_node(writer, node->data.binary_op.right, &child_size) < 0) return -1;
            size += child_size;
            *out_size = size;
            return 0;
        }
        case AST_UNARY_OP: {
            uint32_t size = 1 + 1;
            uint32_t child_size = 0;
            if (ast_measure_node(writer, node->data.unary_op.operand, &child_size) < 0) return -1;
            size += child_size;
            *out_size = size;
            return 0;
        }
        case AST_IDENTIFIER: {
            int16_t name_index = ast_string_index(writer, node->data.identifier.name);
            if (name_index < 0) return -1;
            *out_size = 1 + 2;
            return 0;
        }
        case AST_CONSTANT:
            *out_size = 1 + 2;
            return 0;
        case AST_STRING_LITERAL: {
            int16_t value_index = ast_string_index(writer, node->data.string_literal.value);
            if (value_index < 0) return -1;
            *out_size = 1 + 2;
            return 0;
        }
        case AST_ARRAY_ACCESS: {
            uint32_t size = 1;
            uint32_t child_size = 0;
            if (ast_measure_node(writer, node->data.array_access.base, &child_size) < 0) return -1;
            size += child_size;
            if (ast_measure_node(writer, node->data.array_access.index, &child_size) < 0) return -1;
            size += child_size;
            *out_size = size;
            return 0;
        }
        default:
            cc_error("Unsupported AST node in size pass");
            return -1;
    }
}

static void ast_writer_reset_counts(ast_writer_t* writer) {
    if (!writer) return;
    writer->node_count = 0;
    writer->decl_count = 0;
}

void cleanup(void) {
    if (writer) {
        if (writer->out) {
            output_close(writer->out);
        }
        ast_free_strings(writer);
    }
    if (parser) {
        parser_destroy(parser);
    }
    if (lexer) {
        lexer_destroy(lexer);
    }
    if (reader) {
        reader_close(reader);
    }
}

void handle_error(char* msg) {
    log_error(msg);
    cleanup();
    exit(1);
}

int main(int argc, char** argv) {
    int8_t err = 1;
    args_t args;
    ast_node_t* ast = NULL;
    uint32_t string_table_offset = 0;
    uint16_t total_nodes = 0;
    uint16_t total_strings = 0;
    uint16_t total_decls = 0;
    uint32_t program_bytes = 0;
    uint32_t nodes_bytes = 0;

    cc_init_pool(g_memory_pool, sizeof(g_memory_pool));
    writer = (ast_writer_t*)cc_malloc(sizeof(*writer));
    if (!writer) return 1;
    mem_set(writer, 0, sizeof(*writer));


    args = parse_args(argc, argv);
    if (args.error) {
        log_error("Usage: cc_parse <input.c> <output.ast>\n");
        return 1;
    }

    reader = reader_open(args.input_file);
    if (!reader) return 1;

    lexer = lexer_create(args.input_file, reader);
    if (!lexer) goto cleanup;

    parser = parser_create(lexer);
    if (!parser) goto cleanup;

    while (1) {
        ast = parser_parse_next(parser);
        if (!ast) break;
        if (ast->type != AST_FUNCTION && ast->type != AST_VAR_DECL) {
            ast_node_destroy(ast);
            ast = NULL;
            continue;
        }
        {
            uint32_t node_size = 0;
            if (ast_measure_node(writer, ast, &node_size) < 0) {
                ast_node_destroy(ast);
                ast = NULL;
                log_error("Failed to size AST node\n");
                goto cleanup;
            }
            nodes_bytes += node_size;
        }
        writer->decl_count++;
        ast_node_destroy(ast);
        ast = NULL;
    }

    if (parser->error_count > 0) {
        log_error("Parsing failed\n");
        goto cleanup;
    }

    total_nodes = (uint16_t)(writer->node_count + 1);
    total_strings = writer->string_count;
    total_decls = writer->decl_count;
    program_bytes = 1 + 2;
    string_table_offset = AST_HEADER_SIZE + program_bytes + nodes_bytes;

    parser_destroy(parser);
    parser = NULL;
    lexer_destroy(lexer);
    lexer = NULL;
    reader_close(reader);
    reader = NULL;

    reader = reader_open(args.input_file);
    if (!reader) goto cleanup;

    lexer = lexer_create(args.input_file, reader);
    if (!lexer) goto cleanup;

    parser = parser_create(lexer);
    if (!parser) goto cleanup;

    writer->out = output_open(args.output_file);
#ifdef __SDCC
    if (writer->out < 0) goto cleanup;
#else
    if (!writer->out) goto cleanup;
#endif

    writer->strings_frozen = true;
    ast_writer_reset_counts(writer);

    if (ast_write_header_full(writer, total_nodes, total_strings, string_table_offset) < 0) {
        log_error("Failed to write AST header\n");
        goto cleanup;
    }

    ast_write_handler(handle_error, "Failed to write AST program tag\n");
    ast_write_u8(writer->out, AST_TAG_PROGRAM);

    ast_write_handler(handle_error, "Failed to write AST program decl count\n");
    ast_write_u16(writer->out, total_decls);

    ast_write_handler(handle_error, "Failed to write AST node\n");
    while (1) {
        ast = parser_parse_next(parser);
        if (!ast) break;
        if (ast->type != AST_FUNCTION && ast->type != AST_VAR_DECL) {
            ast_node_destroy(ast);
            ast = NULL;
            continue;
        }
        ast_write_node(writer, ast);
        ast_node_destroy(ast);
        ast = NULL;
    }

    if (parser->error_count > 0) {
        log_error("Parsing failed\n");
        goto cleanup;
    }

    if (ast_write_string_table(writer, &string_table_offset) < 0) {
        log_error("Failed to write AST string table\n");
        goto cleanup;
    }

    log_msg(args.input_file);
    log_msg(" -> ");
    log_msg(args.output_file);
    log_msg("\n");

    err = 0;

cleanup:
    cleanup();
    return err;
}
