#include "symbol.h"

#include "common.h"

typedef void (*type_destroy_fn)(type_t* type);

static void type_destroy_pointer(type_t* type) {
    if (type->data.pointer.base_type) {
        type_destroy(type->data.pointer.base_type);
    }
}

static void type_destroy_array(type_t* type) {
    if (type->data.array.element_type) {
        type_destroy(type->data.array.element_type);
    }
}

type_t* type_create(type_kind_t kind) {
    type_t* type = (type_t*)cc_malloc(sizeof(type_t));
    if (!type) return NULL;

    type->kind = kind;
    type->is_signed = true;
    type->is_const = false;
    type->is_volatile = false;
    type->size = 0;

    {
        #define TYPE_KIND_COUNT ((uint8_t)TYPE_FUNCTION + 1)
        static const uint8_t k_type_size[TYPE_KIND_COUNT] = {
            0, /* TYPE_VOID */
            1, /* TYPE_CHAR */
            2, /* TYPE_SHORT */
            2, /* TYPE_INT */
            4, /* TYPE_LONG */
            0, /* TYPE_FLOAT */
            0, /* TYPE_DOUBLE */
            2, /* TYPE_POINTER */
            0, /* TYPE_ARRAY */
            0, /* TYPE_STRUCT */
            0, /* TYPE_UNION */
            0, /* TYPE_ENUM */
            0  /* TYPE_FUNCTION */
        };
        if ((uint8_t)kind < TYPE_KIND_COUNT) {
            type->size = k_type_size[kind];
        }
        #undef TYPE_KIND_COUNT
    }

    return type;
}

void type_destroy(type_t* type) {
    if (!type) return;

    {
        #define TYPE_KIND_COUNT ((uint8_t)TYPE_FUNCTION + 1)
        static const type_destroy_fn k_type_destroy_handlers[TYPE_KIND_COUNT] = {
            NULL,                /* TYPE_VOID */
            NULL,                /* TYPE_CHAR */
            NULL,                /* TYPE_SHORT */
            NULL,                /* TYPE_INT */
            NULL,                /* TYPE_LONG */
            NULL,                /* TYPE_FLOAT */
            NULL,                /* TYPE_DOUBLE */
            type_destroy_pointer, /* TYPE_POINTER */
            type_destroy_array,   /* TYPE_ARRAY */
            NULL,                /* TYPE_STRUCT */
            NULL,                /* TYPE_UNION */
            NULL,                /* TYPE_ENUM */
            NULL                 /* TYPE_FUNCTION */
        };
        if ((uint8_t)type->kind < TYPE_KIND_COUNT) {
            type_destroy_fn fn = k_type_destroy_handlers[type->kind];
            if (fn) {
                fn(type);
            }
        }
        #undef TYPE_KIND_COUNT
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
        if (element && element->size > 0 && length > 0) {
            type->size = element->size * length;
        } else {
            type->size = 0;
        }
    }
    return type;
}
