//
// Written by Claude on 16-Oct-26.
//

#include <stddef.h>

#include "Harness.h"
#include "Type.h"

// The types are built by hand, the way the parser will build them (ADR-019). Every composite type a test compares is
// its own object, so equality must be structural: a pointer comparison would fail the "equal" checks.

static const Type int_type = {.kind = TYPE_KIND_INT};
static const Type char_type = {.kind = TYPE_KIND_CHAR};
static const Type void_type = {.kind = TYPE_KIND_VOID};

static Type pointer_to(const Type* const pointee) {
    return (Type){.kind = TYPE_KIND_POINTER, .as.pointer = {.type = pointee}};
}
static Type array_of(const Type* const element, const int length) {
    return (Type){.kind = TYPE_KIND_ARRAY, .as.array = {.type = element, .length = length}};
}
static Type function_of(const Type* const return_type, const Type* const* const parameters, const int count) {
    return (Type){.kind = TYPE_KIND_FUNCTION,
                  .as.function = {.return_type = return_type, .parameters_types = parameters, .parameters_count = count}};
}

// -------- type_get_size -------- //
// Sizes from implementation-defined.md: char 1, int 4, any pointer 8, an array N × its element size.
// void and function types have no size (type_get_size asserts), so they aren't tested here.

static void test_type_size_of_char_and_int(void) {
    CHECK(type_get_size(&char_type) == 1);
    CHECK(type_get_size(&int_type) == 4);
}
/** Every pointer is 8 bytes, whatever it points at. */
static void test_type_size_of_pointers(void) {
    const Type int_pointer = pointer_to(&int_type);
    const Type void_pointer = pointer_to(&void_type);
    const Type char_pointer = pointer_to(&char_type);
    const Type char_pointer_pointer = pointer_to(&char_pointer);

    CHECK(type_get_size(&int_pointer) == 8);
    CHECK(type_get_size(&void_pointer) == 8);
    CHECK(type_get_size(&char_pointer_pointer) == 8);
}
static void test_type_size_of_arrays(void) {
    const Type int_array_3 = array_of(&int_type, 3);
    const Type char_array_5 = array_of(&char_type, 5);
    const Type char_array_1 = array_of(&char_type, 1);

    CHECK(type_get_size(&int_array_3) == 12);
    CHECK(type_get_size(&char_array_5) == 5);
    CHECK(type_get_size(&char_array_1) == 1);
}
/** An array's size comes from its element's size, recursively: never a pointer's 8. */
static void test_type_size_of_nested_arrays(void) {
    const Type int_pointer = pointer_to(&int_type);
    const Type int_pointer_array_3 = array_of(&int_pointer, 3); // int *a[3]
    const Type int_array_3 = array_of(&int_type, 3);
    const Type int_array_2_3 = array_of(&int_array_3, 2);       // int a[2][3]

    CHECK(type_get_size(&int_pointer_array_3) == 24);
    CHECK(type_get_size(&int_array_2_3) == 24);
}

// -------- type_are_equal -------- //

static void test_type_basic_types_equal_only_themselves(void) {
    CHECK(type_are_equal(&int_type, &int_type));
    CHECK(type_are_equal(&char_type, &char_type));
    CHECK(type_are_equal(&void_type, &void_type));

    CHECK(!type_are_equal(&int_type, &char_type));
    CHECK(!type_are_equal(&int_type, &void_type));
    CHECK(!type_are_equal(&char_type, &void_type));
}
static void test_type_pointers_equal_when_pointees_equal(void) {
    const Type int_pointer = pointer_to(&int_type);
    const Type other_int_pointer = pointer_to(&int_type);

    CHECK(type_are_equal(&int_pointer, &other_int_pointer));
}
static void test_type_pointers_differ_when_pointees_differ(void) {
    const Type int_pointer = pointer_to(&int_type);
    const Type char_pointer = pointer_to(&char_type);
    const Type void_pointer = pointer_to(&void_type);
    const Type int_pointer_pointer = pointer_to(&int_pointer);

    CHECK(!type_are_equal(&int_pointer, &char_pointer));
    CHECK(!type_are_equal(&void_pointer, &char_pointer));
    CHECK(!type_are_equal(&int_pointer_pointer, &int_pointer));
    CHECK(!type_are_equal(&int_pointer, &int_type));
}
static void test_type_arrays_equal_when_element_and_length_equal(void) {
    const Type int_array_3 = array_of(&int_type, 3);
    const Type other_int_array_3 = array_of(&int_type, 3);

    CHECK(type_are_equal(&int_array_3, &other_int_array_3));
}
/** int[3] and int[4] are different types (C11 §6.7.6.2p6). */
static void test_type_arrays_differ_in_length(void) {
    const Type int_array_3 = array_of(&int_type, 3);
    const Type int_array_4 = array_of(&int_type, 4);

    CHECK(!type_are_equal(&int_array_3, &int_array_4));
}
static void test_type_arrays_differ_in_element(void) {
    const Type int_array_3 = array_of(&int_type, 3);
    const Type char_array_3 = array_of(&char_type, 3);
    const Type int_array_2 = array_of(&int_type, 2);
    const Type int_array_2_3 = array_of(&int_array_3, 2);       // int[2][3]
    const Type int_array_3_2 = array_of(&int_array_2, 3);       // int[3][2]: same size, different type

    CHECK(!type_are_equal(&int_array_3, &char_array_3));
    CHECK(!type_are_equal(&int_array_2_3, &int_array_3_2));
}
/** Decay belongs to expressions (ADR-024): the array type itself is never equal to a pointer to its element. */
static void test_type_array_is_not_pointer_to_element(void) {
    const Type int_array_3 = array_of(&int_type, 3);
    const Type int_pointer = pointer_to(&int_type);

    CHECK(!type_are_equal(&int_array_3, &int_pointer));
    CHECK(!type_are_equal(&int_pointer, &int_array_3));
}
static void test_type_functions_equal_when_signatures_equal(void) {
    const Type char_pointer = pointer_to(&char_type);
    const Type other_char_pointer = pointer_to(&char_type);
    const Type* const parameters[] = {&int_type, &char_pointer};
    const Type* const other_parameters[] = {&int_type, &other_char_pointer};
    const Type function = function_of(&int_type, parameters, 2);             // int (int, char *)
    const Type other_function = function_of(&int_type, other_parameters, 2); // int (int, char *)

    CHECK(type_are_equal(&function, &other_function));
}
/** int f(void) and int f() are the same type in Cminor (implementation-defined.md): no parameters at all. */
static void test_type_functions_without_parameters_are_equal(void) {
    const Type function = function_of(&int_type, NULL, 0);
    const Type other_function = function_of(&int_type, NULL, 0);

    CHECK(type_are_equal(&function, &other_function));
}
static void test_type_functions_differ_in_return_type(void) {
    const Type* const parameters[] = {&int_type};
    const Type int_function = function_of(&int_type, parameters, 1);   // int (int)
    const Type char_function = function_of(&char_type, parameters, 1); // char (int)
    const Type void_function = function_of(&void_type, parameters, 1); // void (int)

    CHECK(!type_are_equal(&int_function, &char_function));
    CHECK(!type_are_equal(&int_function, &void_function));
}
static void test_type_functions_differ_in_parameter_count(void) {
    const Type* const parameters[] = {&int_type, &int_type};
    const Type no_parameters = function_of(&int_type, NULL, 0);        // int (void)
    const Type one_parameter = function_of(&int_type, parameters, 1);  // int (int)
    const Type two_parameters = function_of(&int_type, parameters, 2); // int (int, int)

    CHECK(!type_are_equal(&no_parameters, &one_parameter));
    CHECK(!type_are_equal(&one_parameter, &no_parameters));
    CHECK(!type_are_equal(&one_parameter, &two_parameters));
}
/** Only the last parameter differs, so the comparison has to go through the whole list. */
static void test_type_functions_differ_in_last_parameter_type(void) {
    const Type int_pointer = pointer_to(&int_type);
    const Type* const parameters[] = {&int_type, &int_type};
    const Type* const other_parameters[] = {&int_type, &int_pointer};
    const Type function = function_of(&int_type, parameters, 2);             // int (int, int)
    const Type other_function = function_of(&int_type, other_parameters, 2); // int (int, int *)

    CHECK(!type_are_equal(&function, &other_function));
}
static void test_type_function_is_not_its_return_type(void) {
    const Type function = function_of(&int_type, NULL, 0);

    CHECK(!type_are_equal(&function, &int_type));
}

int main(const int argc, char** const argv) {
    harness_init(argc, argv);

    RUN_TEST(test_type_size_of_char_and_int);
    RUN_TEST(test_type_size_of_pointers);
    RUN_TEST(test_type_size_of_arrays);
    RUN_TEST(test_type_size_of_nested_arrays);

    RUN_TEST(test_type_basic_types_equal_only_themselves);
    RUN_TEST(test_type_pointers_equal_when_pointees_equal);
    RUN_TEST(test_type_pointers_differ_when_pointees_differ);
    RUN_TEST(test_type_arrays_equal_when_element_and_length_equal);
    RUN_TEST(test_type_arrays_differ_in_length);
    RUN_TEST(test_type_arrays_differ_in_element);
    RUN_TEST(test_type_array_is_not_pointer_to_element);
    RUN_TEST(test_type_functions_equal_when_signatures_equal);
    RUN_TEST(test_type_functions_without_parameters_are_equal);
    RUN_TEST(test_type_functions_differ_in_return_type);
    RUN_TEST(test_type_functions_differ_in_parameter_count);
    RUN_TEST(test_type_functions_differ_in_last_parameter_type);
    RUN_TEST(test_type_function_is_not_its_return_type);

    return harness_finish();
}
