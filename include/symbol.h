#ifndef SYMBOL_H
#define SYMBOL_H

#include "common.h"

/* Symbol types */
typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_TYPEDEF,
    SYM_STRUCT,
    SYM_ENUM,
    SYM_LABEL
} symbol_kind_t;

/* Storage classes */
typedef enum {
    STORAGE_AUTO,
    STORAGE_STATIC,
    STORAGE_EXTERN,
    STORAGE_REGISTER
} storage_class_t;

/* Type kinds */
typedef enum {
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_FUNCTION
} type_kind_t;

/* Type structure */
struct type {
    type_kind_t kind;
    bool is_signed;
    bool is_const;
    bool is_volatile;
    size_t size;
    
    union {
        struct {
            type_t* base_type;
        } pointer;
        
        struct {
            type_t* element_type;
            size_t length;
        } array;
        
        struct {
            char* name;
            symbol_t** members;
            size_t member_count;
        } struct_type;
        
        struct {
            type_t* return_type;
            type_t** param_types;
            size_t param_count;
        } function;
    } data;
};

/* Symbol structure */
struct symbol {
    char* name;
    symbol_kind_t kind;
    type_t* type;
    storage_class_t storage;
    int16_t scope_level;
    int16_t offset;  /* Stack offset for local variables */
    bool is_defined;
    
    union {
        struct {
            ast_node_t* body;
            symbol_t** params;
            size_t param_count;
        } function;
        
        struct {
            long long value;
        } constant;
    } data;
    
    symbol_t* next;
};

/* Type functions */
type_t* type_create(type_kind_t kind);
void type_destroy(type_t* type);
type_t* type_create_pointer(type_t* base);
type_t* type_create_array(type_t* element, size_t length);

#endif /* SYMBOL_H */
