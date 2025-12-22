#include "symbol.h"

#include "common.h"

#define SYMBOL_TABLE_SIZE 128

symbol_table_t* symbol_table_create(symbol_table_t* parent) {
    symbol_table_t* table = (symbol_table_t*)cc_malloc(sizeof(symbol_table_t));
    if (!table) return NULL;

    table->bucket_count = SYMBOL_TABLE_SIZE;
    table->buckets = (symbol_t**)cc_malloc(sizeof(symbol_t*) * SYMBOL_TABLE_SIZE);
    if (!table->buckets) {
        cc_free(table);
        return NULL;
    }

    for (size_t i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        table->buckets[i] = NULL;
    }

    table->parent = parent;
    table->scope_level = parent ? (parent->scope_level + 1) : 0;

    return table;
}

void symbol_table_destroy(symbol_table_t* table) {
    if (!table) return;

    if (table->buckets) {
        cc_free(table->buckets);
    }
    cc_free(table);
}
