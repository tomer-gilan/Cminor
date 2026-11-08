//
// Created by tomer on 16-Oct-26.
//

#include "Type.h"

#include <assert.h>
#include <stddef.h>

const int CHAR_TYPE_SIZE = 1;       // the standard says so
const int INT_TYPE_SIZE = 4;        // x64, as stated in implementation-defined.md
const int POINTER_TYPE_SIZE = 8;    // x64, as stated in implementation-defined.md

static const Type TYPE_VOID = {.kind = TYPE_KIND_VOID};
static const Type TYPE_CHAR = {.kind = TYPE_KIND_CHAR};
static const Type TYPE_INT = {.kind = TYPE_KIND_INT};

static bool type_function_are_equal(const TypeFunction first, const TypeFunction second) {
    if (first.parameters_count != second.parameters_count ||
        !type_are_equal(first.return_type, second.return_type))
        return false;
    const int N = first.parameters_count;
    for (int i = 0; i < N; ++i)
        if (!type_are_equal(first.parameters_types[i], second.parameters_types[i]))
            return false;
    return true;
}
static bool type_pointer_are_equal(const TypePointer first, const TypePointer second) {
    return type_are_equal(first.type, second.type);
}
static bool type_array_are_equal(const TypeArray first, const TypeArray second) {
    return first.length == second.length && type_are_equal(first.type, second.type);
}

bool type_are_equal(const Type* first, const Type* second) {
    if (first->kind != second->kind)
        return false;
    const TypeKind kind = first->kind;
    switch (kind) {
        case TYPE_KIND_CHAR:
        case TYPE_KIND_INT:
        case TYPE_KIND_VOID:
            return true; // nothing further to check.
        case TYPE_KIND_ARRAY:
            return type_array_are_equal(first->as.array, second->as.array);
        case TYPE_KIND_POINTER:
            return type_pointer_are_equal(first->as.pointer, second->as.pointer);
        case TYPE_KIND_FUNCTION:
            return type_function_are_equal(first->as.function, second->as.function);
        case TYPE_KIND__COUNT:
        case TYPE_KIND_NONE:
        default:
            assert(false && "invalid type");
            return false;
    }
}

static int type_char_get_size(void) {
    return CHAR_TYPE_SIZE;
}
static int type_int_get_size(void) {
    return INT_TYPE_SIZE;
}
static int type_pointer_get_size(void) {
    return POINTER_TYPE_SIZE;
}
static int type_array_get_size(const TypeArray this) {
    return type_get_size(this.type) * this.length;
}
int type_get_size(const Type* const this) {
    switch (this->kind) {
        case TYPE_KIND_CHAR:
            return type_char_get_size();
        case TYPE_KIND_INT:
            return type_int_get_size();
        case TYPE_KIND_ARRAY:
            return type_array_get_size(this->as.array);
        case TYPE_KIND_POINTER:
            return type_pointer_get_size();
        case TYPE_KIND_FUNCTION:
        case TYPE_KIND_VOID:
        case TYPE_KIND__COUNT:
        case TYPE_KIND_NONE:
        default:
            assert(false && "this type has no size");
            return -1;
    }
}
const Type* type_get_singleton(const TypeKind kind) {
    switch (kind) {
        case TYPE_KIND_VOID:
            return &TYPE_VOID;
        case TYPE_KIND_CHAR:
            return &TYPE_CHAR;
        case TYPE_KIND_INT:
            return &TYPE_INT;
        case TYPE_KIND_FUNCTION:
        case TYPE_KIND_ARRAY:
        case TYPE_KIND_POINTER:
        case TYPE_KIND__COUNT:
        case TYPE_KIND_NONE:
        default:
            assert(false && "non-singleton type kind");
            return NULL;
    }
}
const Type* type_get_void(void) {
    return type_get_singleton(TYPE_KIND_VOID);
}
const Type* type_get_char(void) {
    return type_get_singleton(TYPE_KIND_CHAR);
}
const Type* type_get_int(void) {
    return type_get_singleton(TYPE_KIND_INT);
}

const Type* parser_type_create_function(
    Arena* const arena,
    const Type* const return_type,
    const Type* const* const parameters_types,
    const int parameters_count) {
    assert(arena != NULL);

    Type* const to_return = arena_malloc(arena, sizeof (Type));
    *to_return = (Type){
        .kind = TYPE_KIND_FUNCTION,
        .as.function = {
            .return_type = return_type,
            .parameters_types = parameters_types,
            .parameters_count = parameters_count
        }
    };
    return to_return;
}