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

    /* Free all symbols in buckets */
    for (size_t i = 0; i < table->bucket_count; i++) {
        symbol_t* sym = table->buckets[i];
        while (sym) {
            symbol_t* next = sym->next;
            symbol_destroy(sym);
            sym = next;
        }
    }

    if (table->buckets) {
        cc_free(table->buckets);
    }
    cc_free(table);
}

static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int16_t c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

symbol_t* symbol_table_insert(symbol_table_t* table, const char* name, symbol_kind_t kind) {
    if (!table || !name) return NULL;

    size_t index = hash_string(name) % table->bucket_count;

    /* Check if symbol already exists */
    symbol_t* existing = table->buckets[index];
    while (existing) {
        bool match = true;
        const char* p1 = existing->name;
        const char* p2 = name;
        while (*p1 && *p2) {
            if (*p1++ != *p2++) {
                match = false;
                break;
            }
        }
        if (match && *p1 == *p2) {
            return existing; /* Already exists */
        }
        existing = existing->next;
    }

    /* Create new symbol */
    symbol_t* sym = symbol_create(name, kind);
    if (!sym) return NULL;

    sym->scope_level = table->scope_level;
    sym->next = table->buckets[index];
    table->buckets[index] = sym;

    return sym;
}

symbol_t* symbol_table_lookup(symbol_table_t* table, const char* name) {
    while (table) {
        symbol_t* sym = symbol_table_lookup_current(table, name);
        if (sym) return sym;
        table = table->parent;
    }
    return NULL;
}

symbol_t* symbol_table_lookup_current(symbol_table_t* table, const char* name) {
    if (!table || !name) return NULL;

    size_t index = hash_string(name) % table->bucket_count;
    symbol_t* sym = table->buckets[index];

    while (sym) {
        bool match = true;
        const char* p1 = sym->name;
        const char* p2 = name;
        while (*p1 && *p2) {
            if (*p1++ != *p2++) {
                match = false;
                break;
            }
        }
        if (match && *p1 == *p2) {
            return sym;
        }
        sym = sym->next;
    }

    return NULL;
}

symbol_t* symbol_create(const char* name, symbol_kind_t kind) {
    symbol_t* sym = (symbol_t*)cc_malloc(sizeof(symbol_t));
    if (!sym) return NULL;

    sym->name = cc_strdup(name);
    sym->kind = kind;
    sym->type = NULL;
    sym->storage = STORAGE_AUTO;
    sym->scope_level = 0;
    sym->offset = 0;
    sym->is_defined = false;
    sym->next = NULL;

    return sym;
}

void symbol_destroy(symbol_t* symbol) {
    if (!symbol) return;

    if (symbol->name) {
        cc_free(symbol->name);
    }

    if (symbol->type) {
        type_destroy(symbol->type);
    }

    cc_free(symbol);
}

type_t* type_create(type_kind_t kind) {
    type_t* type = (type_t*)cc_malloc(sizeof(type_t));
    if (!type) return NULL;

    type->kind = kind;
    type->is_signed = true;
    type->is_const = false;
    type->is_volatile = false;
    type->size = 0;

    switch (kind) {
        case TYPE_VOID:
            type->size = 0;
            break;
        case TYPE_CHAR:
            type->size = 1;
            break;
        case TYPE_SHORT:
            type->size = 2;
            break;
        case TYPE_INT:
            type->size = 2;
            break;
        case TYPE_LONG:
            type->size = 4;
            break;
        case TYPE_POINTER:
            type->size = 2;
            break;
        default:
            break;
    }

    return type;
}

void type_destroy(type_t* type) {
    if (!type) return;

    switch (type->kind) {
        case TYPE_POINTER:
            if (type->data.pointer.base_type) {
                type_destroy(type->data.pointer.base_type);
            }
            break;
        case TYPE_ARRAY:
            if (type->data.array.element_type) {
                type_destroy(type->data.array.element_type);
            }
            break;
        default:
            break;
    }

    cc_free(type);
}

type_t* type_create_pointer(type_t* base) {
    type_t* type = type_create(TYPE_POINTER);
    if (type) {
        type->data.pointer.base_type = base;
        type->size = 2; /* Pointers are 2 bytes on Z80 */
    }
    return type;
}

type_t* type_create_array(type_t* element, size_t length) {
    type_t* type = type_create(TYPE_ARRAY);
    if (type) {
        type->data.array.element_type = element;
        type->data.array.length = length;
        type->size = element->size * length;
    }
    return type;
}

bool type_equals(const type_t* a, const type_t* b) {
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;

    switch (a->kind) {
        case TYPE_POINTER:
            return type_equals(a->data.pointer.base_type, b->data.pointer.base_type);
        case TYPE_ARRAY:
            return (a->data.array.length == b->data.array.length) &&
                   type_equals(a->data.array.element_type, b->data.array.element_type);
        default:
            return true;
    }
}

size_t type_sizeof(const type_t* type) {
    return type ? type->size : 0;
}

const char* type_to_string(const type_t* type) {
    if (!type) return "unknown";

    switch (type->kind) {
        case TYPE_VOID:
            return "void";
        case TYPE_CHAR:
            return "char";
        case TYPE_SHORT:
            return "short";
        case TYPE_INT:
            return "int";
        case TYPE_LONG:
            return "long";
        case TYPE_POINTER:
            return "pointer";
        case TYPE_ARRAY:
            return "array";
        default:
            return "unknown";
    }
}
