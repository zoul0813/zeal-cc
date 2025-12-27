#include "symbol.h"

#include "common.h"

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
        if (element && element->size > 0 && length > 0) {
            type->size = element->size * length;
        } else {
            type->size = 0;
        }
    }
    return type;
}
