/* Invalid nested nodes/types must propagate to the declaration writer. */
#include <assert.h>
#define main parser_entry
#include "../../src/parser/main.c"
#undef main

int main(int argc, char** argv) {
    assert(argc == 2);
    cc_init_pool_default();
    ast_writer_t storage;
    mem_set(&storage, 0, sizeof(storage));
    writer = &storage;
    writer->out = output_open(argv[1]);
    assert(OUTPUT_VALID(writer->out));
    ast_node_t bad, parent, leaf;
    type_t type;
    mem_set(&bad, 0, sizeof(bad));
    mem_set(&parent, 0, sizeof(parent));
    mem_set(&leaf, 0, sizeof(leaf));
    mem_set(&type, 0, sizeof(type));
    bad.type = AST_ARRAY_ACCESS + 1;
    leaf.type = AST_CONSTANT;
    type.kind = TYPE_INT;

    parent.type = AST_FUNCTION;
    parent.data.function.name = "helper";
    parent.data.function.return_type = &type;
    parent.data.function.body = &bad;
    assert(ast_write_node(&parent) < 0);
    parent.data.function.body = &leaf;
    type.kind = TYPE_LONG;
    assert(ast_write_node(&parent) < 0);
    type.kind = TYPE_INT;
    ast_node_t* children[] = {&bad};
    parent.data.function.params = children;
    parent.data.function.param_count = 1;
    assert(ast_write_node(&parent) < 0);

    mem_set(&parent, 0, sizeof(parent));
    parent.type = AST_COMPOUND_STMT;
    parent.data.compound.statements = children;
    parent.data.compound.stmt_count = 1;
    assert(ast_write_node(&parent) < 0);
    parent.type = AST_RETURN_STMT;
    parent.data.return_stmt.expr = &bad;
    assert(ast_write_node(&parent) < 0);
    parent.type = AST_IF_STMT;
    parent.data.if_stmt.condition = &bad;
    parent.data.if_stmt.then_branch = &leaf;
    parent.data.if_stmt.else_branch = NULL;
    assert(ast_write_node(&parent) < 0);
    parent.data.if_stmt.condition = &leaf;
    parent.data.if_stmt.then_branch = &bad;
    assert(ast_write_node(&parent) < 0);
    parent.data.if_stmt.then_branch = &leaf;
    parent.data.if_stmt.else_branch = &bad;
    assert(ast_write_node(&parent) < 0);
    assert(output_close(writer->out) == 0);
    ast_free_strings();
    return 0;
}
