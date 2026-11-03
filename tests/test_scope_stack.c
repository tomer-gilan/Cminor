//
// Written by Claude on 17-Oct-26.
//

#include <stddef.h>

#include "Harness.h"
#include "ScopeStack.h"

// The stack only stores and compares Symbol pointers, so the symbols are distinct static objects, zero-initialized and
// with no fields named: the tests don't break when Symbol's fields change. Each test's comment shows the Cminor code whose
// scopes it plays out.

enum {
    IDENTIFIER_COUNT = 4,
    X = 0,
    Y = 1,
    LAST = IDENTIFIER_COUNT - 1, ///< the last id: a scope that copies or zeroes only part of the array fails here
};

static const Symbol outer_x;
static const Symbol inner_x;
static const Symbol innermost_x;
static const Symbol y;
static const Symbol last;

// -------- file scope -------- //

static void test_scope_stack_starts_at_file_scope_with_nothing_declared(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);

    CHECK(scope_stack_is_at_file_scope(stack));
    for (int id = 0; id < IDENTIFIER_COUNT; id++) {
        CHECK(scope_stack_at(stack, id) == NULL);
        CHECK(!scope_stack_was_identifier_declared_in_current_scope(stack, id));
    }

    scope_stack_destroy(stack);
}
/** int x; int y; */
static void test_scope_stack_file_scope_declaration_is_found(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);

    scope_stack_set(stack, X, &outer_x);
    scope_stack_set(stack, LAST, &last);

    CHECK(scope_stack_at(stack, X) == &outer_x);
    CHECK(scope_stack_at(stack, LAST) == &last);
    CHECK(scope_stack_at(stack, Y) == NULL);
    CHECK(scope_stack_was_identifier_declared_in_current_scope(stack, X));
    CHECK(scope_stack_was_identifier_declared_in_current_scope(stack, LAST));
    CHECK(!scope_stack_was_identifier_declared_in_current_scope(stack, Y));

    scope_stack_destroy(stack);
}

// -------- push and pop -------- //

static void test_scope_stack_push_leaves_file_scope_and_pop_returns(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);

    scope_stack_push(stack);
    CHECK(!scope_stack_is_at_file_scope(stack));
    scope_stack_push(stack);
    CHECK(!scope_stack_is_at_file_scope(stack));
    scope_stack_pop(stack);
    CHECK(!scope_stack_is_at_file_scope(stack));
    scope_stack_pop(stack);
    CHECK(scope_stack_is_at_file_scope(stack));

    scope_stack_destroy(stack);
}
/** int x; int last; int f() { x; last; } : an inner scope sees every outer declaration, the last id included. */
static void test_scope_stack_inner_scope_sees_outer_declarations(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);
    scope_stack_set(stack, X, &outer_x);
    scope_stack_set(stack, LAST, &last);

    scope_stack_push(stack);

    CHECK(scope_stack_at(stack, X) == &outer_x);
    CHECK(scope_stack_at(stack, LAST) == &last);
    CHECK(scope_stack_at(stack, Y) == NULL);

    scope_stack_destroy(stack);
}
/** int x; int f() { int x; } : seen from the inner scope, the outer x isn't declared there, so declaring it shadows. */
static void test_scope_stack_outer_declaration_is_not_in_current_scope(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);
    scope_stack_set(stack, X, &outer_x);

    scope_stack_push(stack);

    CHECK(!scope_stack_was_identifier_declared_in_current_scope(stack, X));

    scope_stack_destroy(stack);
}

// -------- shadowing -------- //

/** int x; int f() { int x; x; } x; */
static void test_scope_stack_shadowing_hides_outer_until_pop(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);
    scope_stack_set(stack, X, &outer_x);

    scope_stack_push(stack);
    scope_stack_set(stack, X, &inner_x);
    CHECK(scope_stack_at(stack, X) == &inner_x);
    CHECK(scope_stack_was_identifier_declared_in_current_scope(stack, X));

    scope_stack_pop(stack);
    CHECK(scope_stack_at(stack, X) == &outer_x);
    CHECK(scope_stack_was_identifier_declared_in_current_scope(stack, X));

    scope_stack_destroy(stack);
}
/** int f() { int y; } y; : an inner declaration doesn't outlive its scope. */
static void test_scope_stack_inner_declaration_is_gone_after_pop(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);

    scope_stack_push(stack);
    scope_stack_set(stack, Y, &y);
    scope_stack_pop(stack);

    CHECK(scope_stack_at(stack, Y) == NULL);
    CHECK(!scope_stack_was_identifier_declared_in_current_scope(stack, Y));

    scope_stack_destroy(stack);
}
/** int f() { { int x; } { int x; } } : sibling blocks are separate scopes, so the second x is no redeclaration. */
static void test_scope_stack_sibling_scopes_are_separate(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);
    scope_stack_push(stack);

    scope_stack_push(stack);
    scope_stack_set(stack, X, &inner_x);
    scope_stack_pop(stack);

    scope_stack_push(stack);
    CHECK(scope_stack_at(stack, X) == NULL);
    CHECK(!scope_stack_was_identifier_declared_in_current_scope(stack, X));
    scope_stack_set(stack, X, &innermost_x);
    CHECK(scope_stack_at(stack, X) == &innermost_x);
    scope_stack_pop(stack);

    CHECK(scope_stack_at(stack, X) == NULL);

    scope_stack_destroy(stack);
}
/**
 * int x; int f() { int x; { { x; int x; } x; } x; } x;
 * Each level sees the nearest declaration, "declared in current scope" looks at the current level only, and each pop
 * brings back exactly the level below.
 */
static void test_scope_stack_nested_shadowing_restores_each_level(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);
    scope_stack_set(stack, X, &outer_x);

    scope_stack_push(stack);                  // f's body
    scope_stack_set(stack, X, &inner_x);
    scope_stack_push(stack);                  // {
    scope_stack_push(stack);                  // {
    CHECK(scope_stack_at(stack, X) == &inner_x);
    CHECK(!scope_stack_was_identifier_declared_in_current_scope(stack, X));
    scope_stack_set(stack, X, &innermost_x);
    CHECK(scope_stack_at(stack, X) == &innermost_x);
    CHECK(scope_stack_was_identifier_declared_in_current_scope(stack, X));

    scope_stack_pop(stack);                   // }
    CHECK(scope_stack_at(stack, X) == &inner_x);
    CHECK(!scope_stack_was_identifier_declared_in_current_scope(stack, X));
    scope_stack_pop(stack);                   // }
    CHECK(scope_stack_at(stack, X) == &inner_x);
    CHECK(scope_stack_was_identifier_declared_in_current_scope(stack, X));
    scope_stack_pop(stack);                   // f's }
    CHECK(scope_stack_at(stack, X) == &outer_x);

    scope_stack_destroy(stack);
}

// -------- destroy -------- //

/** Destroying with scopes still pushed frees all of them: LeakSanitizer fails the test otherwise. */
static void test_scope_stack_destroy_frees_pushed_scopes(void) {
    ScopeStack* const stack = scope_stack_create(IDENTIFIER_COUNT);
    scope_stack_push(stack);
    scope_stack_push(stack);
    scope_stack_set(stack, X, &inner_x);
    scope_stack_push(stack);

    scope_stack_destroy(stack);
}

int main(const int argc, char** const argv) {
    harness_init(argc, argv);

    RUN_TEST(test_scope_stack_starts_at_file_scope_with_nothing_declared);
    RUN_TEST(test_scope_stack_file_scope_declaration_is_found);

    RUN_TEST(test_scope_stack_push_leaves_file_scope_and_pop_returns);
    RUN_TEST(test_scope_stack_inner_scope_sees_outer_declarations);
    RUN_TEST(test_scope_stack_outer_declaration_is_not_in_current_scope);

    RUN_TEST(test_scope_stack_shadowing_hides_outer_until_pop);
    RUN_TEST(test_scope_stack_inner_declaration_is_gone_after_pop);
    RUN_TEST(test_scope_stack_sibling_scopes_are_separate);
    RUN_TEST(test_scope_stack_nested_shadowing_restores_each_level);

    RUN_TEST(test_scope_stack_destroy_frees_pushed_scopes);

    return harness_finish();
}
