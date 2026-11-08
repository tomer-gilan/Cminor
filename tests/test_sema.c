//
// Written by Claude on 19-Oct-26.
//
// Sema tests for slices 1 to 3. Each test lexes, parses and analyzes a source string. A valid program must give no
// diagnostics and a type for every expression. An invalid one must give exactly one diagnostic (sema stops at the first
// error, ADR-025), with the expected code and offset. Tests read sema's table directly, through each expression node's
// id (ADR-023), or compare the typed dump (check_typed_dump, since 31-Oct-26).
// Expected values come from SUBSET.md and the C standard, not from sema's output.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "AstSerializer.h"
#include "Context.h"
#include "Harness.h"
#include "Lexer.h"
#include "Parser.h"
#include "Sema.h"
#include "SourceText.h"
#include "Statement.h"
#include "Text.h"

/** The compiler's arguments for a test, as if run as `c_compiler <source> <output>`. Tests never write the output. */
static CompilationArguments test_compilation_arguments(void) {
    static const char* const argv[] = {"c_compiler", "<source>", "<unused>.s"};
    return compilation_flags_from_arguments((int)(sizeof argv / sizeof argv[0]), argv);
}

/**
 * Everything one analysis needs. The AST, the symbols and sema's table live in @c context 's arena, so they're valid
 * until the fixture is destroyed. If the source didn't parse, @c parsed is false, @c ast is @c NULL and sema didn't run.
 */
typedef struct {
    Context* context;
    List* tokens;
    bool parsed;
    const TranslationUnitNode* ast;
    int expression_count;   ///< the parser's count, which sema's table must match
    SemaResult result;
} SemaFixture;

/** Lexes, parses and (if the parse reported nothing) analyzes @b source . Ownership of @b source remains with the caller. */
static SemaFixture sema_fixture_create(const char* const source) {
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(source, (int)strlen(source)), "<source>"),
        test_compilation_arguments());
    Lexer* const lexer = lexer_create(context, 0);
    List* const tokens = lexer_lex_source_text(lexer);
    lexer_destroy(lexer);

    Parser* const parser = parser_create(tokens, context);
    const TranslationUnitNode* const ast = parser_parse(parser);
    const int expression_count = parser_get_expression_id_count(parser);
    parser_destroy(parser);

    SemaFixture fixture = {
        .context = context,
        .tokens = tokens,
        .parsed = ast != NULL && list_get_size(context->diagnostic_report_table) == 0,
        .ast = ast,
        .expression_count = expression_count,
        .result = { .expressions_semantics = NULL, .expressions_count = 0 }
    };
    if (fixture.parsed) {
        Sema* const sema = sema_create(context, ast, expression_count);
        fixture.result = sema_analyze(sema);
        sema_destroy(sema);
    }
    return fixture;
}
static void sema_fixture_destroy(const SemaFixture fixture) {
    list_destroy(fixture.tokens);
    context_destroy(fixture.context);
}

/** @return sema's entry for @b expression , found by its id. */
static const ExpressionSemantics* semantics_of(const SemaFixture* const fixture, const ExpressionNode* const expression) {
    return &fixture->result.expressions_semantics[expression->id];
}
/** @return the @b index 'th external definition's function, or @c NULL if there's no such definition. */
static const FunctionDefinitionNode* function_at(const TranslationUnitNode* const translation_unit, const int index) {
    const ExternalDeclarationItem* definition = translation_unit->head;
    for (int i = 0; i < index && definition != NULL; i++)
        definition = definition->next;
    if (definition == NULL || definition->kind != EXTERNAL_DECLARATION_ITEM_KIND_FUNCTION)
        return NULL;
    return definition->as.external_function_definition;
}
/** @return the expression of @b function 's first statement, if it's a @c return , @c NULL otherwise (also for a @c NULL @b function ). */
static const ExpressionNode* first_return_expression(const FunctionDefinitionNode* const function) {
    if (function == NULL)
        return NULL;
    const BlockItem* const item = function->body->as.compound_statement.head;
    if (item == NULL || item->kind != BLOCK_ITEM_KIND_STATEMENT || item->as.statement->kind != STATEMENT_KIND_RETURN)
        return NULL;
    return item->as.statement->as.return_statement.expression;
}

/**
 * Checks that @b fixture holds a program sema accepts: it parsed, sema reported no diagnostics, its table has one entry
 * per expression, and every entry has a type.
 * @return true iff all of that holds, so a test can go on to check single entries.
 */
static bool check_sema_passes(const SemaFixture* const fixture) {
    if (!CHECK(fixture->parsed))
        return false;
    bool passed = CHECK(list_get_size(fixture->context->diagnostic_report_table) == 0);
    passed = CHECK(fixture->result.expressions_count == fixture->expression_count) && passed;
    if (!CHECK(fixture->result.expressions_semantics != NULL || fixture->expression_count == 0))
        return false;
    for (int i = 0; i < fixture->result.expressions_count; i++)
        passed = CHECK(fixture->result.expressions_semantics[i].type != NULL) && passed;
    return passed;
}
/**
 * Checks that the FLAT, typed dump of @b fixture 's AST, with sema's table, is exactly @b expected (ast-dump-format.md,
 * Typed mode). On a mismatch, prints both dumps.
 */
static void check_typed_dump(const SemaFixture* const fixture, const char* const expected) {
    AstSerializer* const serializer =
        ast_serializer_create(fixture->context, AST_SERIALIZATION_MODE_FLAT, fixture->result.expressions_semantics);
    Text* const dump = ast_serializer_serialize(serializer, fixture->ast);
    const TextView actual = text_as_view(dump);
    if (!CHECK(text_view_equals(actual, text_view_make((int)strlen(expected), expected))))
        printf("    expected: %s\n    actual:   %.*s\n", expected, actual.length, actual.data);
    text_destroy(dump);
    ast_serializer_destroy(serializer);
}
/**
 * Checks that @b source parses, but sema rejects it: exactly one diagnostic, with code @b code at byte offset
 * @b offset .
 */
static void check_sema_fails(const char* const source, const DiagnosticCode code, const int offset) {
    const SemaFixture fixture = sema_fixture_create(source);

    if (CHECK(fixture.parsed)) {
        const List* const diagnostics = fixture.context->diagnostic_report_table;
        if (CHECK(list_get_size(diagnostics) == 1)) {
            const Diagnostic* const diagnostic = list_at_const(diagnostics, 0);
            CHECK(diagnostic->code == code);
            CHECK(diagnostic->range.position.offset == offset);
        }
    }

    sema_fixture_destroy(fixture);
}

// ---------------- Valid programs ---------------- //

/** "int main(void) { return 2; }": the slice 1 program. Its only expression, the constant 2, has type int. */
static void test_sema_slice_1_program(void) {
    const SemaFixture fixture = sema_fixture_create("int main(void) { return 2; }");

    if (check_sema_passes(&fixture)) {
        CHECK(fixture.result.expressions_count == 1);
        const ExpressionNode* const two = first_return_expression(function_at(fixture.ast, 0));
        if (CHECK(two != NULL)) {
            CHECK(semantics_of(&fixture, two)->type == type_get_int());
            CHECK(semantics_of(&fixture, two)->identifier_symbol == NULL);  // not an identifier
        }
    }

    sema_fixture_destroy(fixture);
}
/** The slice 1 program's typed dump: the constant gets ":int"; the declarator's type prints the same as untyped. */
static void test_sema_typed_dump_slice_1_program(void) {
    const SemaFixture fixture = sema_fixture_create("int main(void) { return 2; }");

    if (check_sema_passes(&fixture))
        check_typed_dump(&fixture,
            "(translation-unit (function-definition main:int(void) (parameters) (block (return 2:int))))");

    sema_fixture_destroy(fixture);
}
/** "int f(void){return 1;} int g(void){return 2;}": two functions with different names. */
static void test_sema_two_functions(void) {
    const SemaFixture fixture = sema_fixture_create("int f(void){return 1;} int g(void){return 2;}");

    if (check_sema_passes(&fixture)) {
        CHECK(fixture.result.expressions_count == 2);
        for (int i = 0; i < 2; i++) {
            const ExpressionNode* const constant = first_return_expression(function_at(fixture.ast, i));
            if (CHECK(constant != NULL))
                CHECK(semantics_of(&fixture, constant)->type == type_get_int());
        }
    }

    sema_fixture_destroy(fixture);
}
/** Everything slice 1 has, together: several functions, nested and empty blocks, and returns inside blocks. */
static void test_sema_whole_program(void) {
    const SemaFixture fixture = sema_fixture_create(
        "int helper(void) {\n"
        "    {}\n"
        "    return 7;\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    {\n"
        "        { return 1; }\n"
        "    }\n"
        "    return 0;\n"
        "}\n");

    if (check_sema_passes(&fixture))
        CHECK(fixture.result.expressions_count == 3);

    sema_fixture_destroy(fixture);
}

// ---------------- Errors ---------------- //
// The expected offsets were counted by hand from the input, not copied from sema's output.

/**
 * "int f(void) { return 1; } int f(void) { return 2; }": a function defined twice (§6.9p5: at most one external
 * definition per name). Reported at the second definition's name, the 'f' at offset 30.
 */
static void test_sema_error_function_defined_twice(void) {
    check_sema_fails("int f(void) { return 1; } int f(void) { return 2; }",
                     DIAGNOSTIC_CODE_SEMA_SYMBOL_ALREADY_DEFINED, 30);
}
/**
 * "int f(void){return 1;} int f(void){return 2;} int f(void){return 3;}": sema stops at the first error (ADR-025), so
 * the third definition isn't reported too. Reported at the second 'f', offset 27.
 */
static void test_sema_error_function_defined_three_times(void) {
    check_sema_fails("int f(void){return 1;} int f(void){return 2;} int f(void){return 3;}",
                     DIAGNOSTIC_CODE_SEMA_SYMBOL_ALREADY_DEFINED, 27);
}

// ---------------- Slice 2: unary operators and parentheses ---------------- //
// Written ahead by Claude on 21-Oct-26, before slice 2 exists. On since 31-Oct-26: they fail until sema handles
// slice 2.
// No error tests: every operand is an int until pointers (slice 11).

/**
 * Checks that sema accepts @b source , a single @c main whose first statement returns an expression, that the program
 * has @b expression_count expressions (parentheses produce no node), and that the returned expression has type @c int
 * (C11 6.5.3.3: @c - @c + @c ~ give the promoted operand's type, @c int here; @c ! always gives @c int ).
 */
static void check_returns_int(const char* const source, const int expression_count) {
    const SemaFixture fixture = sema_fixture_create(source);

    if (check_sema_passes(&fixture)) {
        CHECK(fixture.result.expressions_count == expression_count);
        const ExpressionNode* const returned = first_return_expression(function_at(fixture.ast, 0));
        if (CHECK(returned != NULL))
            CHECK(semantics_of(&fixture, returned)->type == type_get_int());
    }

    sema_fixture_destroy(fixture);
}

/** "-2", "+2", "~2", "!2": each operator on a constant is an int, and the program has two expressions. */
static void test_sema_slice_2_each_unary_operator(void) {
    check_returns_int("int main(void) { return -2; }", 2);
    check_returns_int("int main(void) { return +2; }", 2);
    check_returns_int("int main(void) { return ~2; }", 2);
    check_returns_int("int main(void) { return !2; }", 2);
}
/** "~-2", "!!1", "-(-(2))", "-(~2)": nested operators. Every node has a type (check_sema_passes), the outer one int. */
static void test_sema_slice_2_nested_unary(void) {
    check_returns_int("int main(void) { return ~-2; }", 3);
    check_returns_int("int main(void) { return !!1; }", 3);
    check_returns_int("int main(void) { return -(-(2)); }", 3);
    check_returns_int("int main(void) { return -(~2); }", 3);
}
/** "(((2)))": parentheses produce no node, so the program has one expression, the constant. */
static void test_sema_slice_2_parentheses(void) {
    check_returns_int("int main(void) { return (((2))); }", 1);
}

// ---------------- Slice 3: arithmetic ---------------- //
// Written by Claude on 06-Nov-26.
// No error tests: every operand is an int until pointers (slice 11).

/**
 * Checks that sema accepts "int main(void) { return <expression_source>; }", and that the returned expression's typed
 * dump is exactly @b expected_expression_dump (ast-dump-format.md, Typed mode). This pins every node's type, not only
 * the outermost one.
 */
static void check_returned_expression_typed_dump(
    const char* const expression_source,
    const char* const expected_expression_dump) {
    char source[128];
    const int source_length = snprintf(source, sizeof source, "int main(void) { return %s; }", expression_source);
    assert(0 < source_length && source_length < (int)sizeof source);
    char expected[256];
    const int expected_length = snprintf(expected, sizeof expected,
        "(translation-unit (function-definition main:int(void) (parameters) (block (return %s))))",
        expected_expression_dump);
    assert(0 < expected_length && expected_length < (int)sizeof expected);

    const SemaFixture fixture = sema_fixture_create(source);
    if (check_sema_passes(&fixture))
        check_typed_dump(&fixture, expected);
    sema_fixture_destroy(fixture);
}

/** "1 + 2", "1 - 2", "1 * 2", "1 / 2", "1 % 2": each operator on two ints is an int (C11 6.3.1.8), three expressions. */
static void test_sema_slice_3_each_binary_operator(void) {
    check_returns_int("int main(void) { return 1 + 2; }", 3);
    check_returns_int("int main(void) { return 1 - 2; }", 3);
    check_returns_int("int main(void) { return 1 * 2; }", 3);
    check_returns_int("int main(void) { return 1 / 2; }", 3);
    check_returns_int("int main(void) { return 1 % 2; }", 3);
}
/** Nested binary operators, with unary operators and parentheses. Parentheses add no expression. */
static void test_sema_slice_3_nested_expressions(void) {
    check_returns_int("int main(void) { return 1 + 2 * 3; }", 5);
    check_returns_int("int main(void) { return 1 - 2 - 3; }", 5);
    check_returns_int("int main(void) { return -1 * -2; }", 5);
    check_returns_int("int main(void) { return -(1 + 2); }", 4);
    check_returns_int("int main(void) { return (1 + 2) * 3; }", 5);
    check_returns_int("int main(void) { return ((1) + (2)); }", 3);
}
/** Every node of a binary expression gets a type: both operands and the operator, however deep. */
static void test_sema_slice_3_typed_dump(void) {
    check_returned_expression_typed_dump("1 + 2", "(+:int 1:int 2:int)");
    check_returned_expression_typed_dump("1 + 2 * 3", "(+:int 1:int (*:int 2:int 3:int))");
    check_returned_expression_typed_dump("1 - 2 - 3", "(-:int (-:int 1:int 2:int) 3:int)");
    check_returned_expression_typed_dump("-1 * -2", "(*:int (-:int 1:int) (-:int 2:int))");
    check_returned_expression_typed_dump("-(1 + 2)", "(-:int (+:int 1:int 2:int))");
    check_returned_expression_typed_dump("(1 + 2) * 3", "(*:int (+:int 1:int 2:int) 3:int)");
}
/** "1 / 0", "1 % 0": division by zero is undefined at run time, not a constraint violation, so sema accepts both. */
static void test_sema_slice_3_division_by_zero_is_accepted(void) {
    check_returns_int("int main(void) { return 1 / 0; }", 3);
    check_returns_int("int main(void) { return 1 % 0; }", 3);
}
/** Everything slice 3 has, together, as it would sit in a file. */
static void test_sema_slice_3_whole_program(void) {
    const SemaFixture fixture = sema_fixture_create(
        "int helper(void) {\n"
        "    return 10 - 3 - 2;\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    return -(1 + 2) * 3 % 4;\n"
        "}\n");

    if (check_sema_passes(&fixture)) {
        CHECK(fixture.result.expressions_count == 13);
        check_typed_dump(&fixture,
            "(translation-unit"
            " (function-definition helper:int(void) (parameters) (block (return (-:int (-:int 10:int 3:int) 2:int))))"
            " (function-definition main:int(void) (parameters)"
            " (block (return (%:int (*:int (-:int (+:int 1:int 2:int)) 3:int) 4:int)))))");
    }

    sema_fixture_destroy(fixture);
}

int main(const int argc, char** const argv) {
    harness_init(argc, argv);

    RUN_TEST(test_sema_slice_1_program);
    RUN_TEST(test_sema_typed_dump_slice_1_program);
    RUN_TEST(test_sema_two_functions);
    RUN_TEST(test_sema_whole_program);

    RUN_TEST(test_sema_error_function_defined_twice);
    RUN_TEST(test_sema_error_function_defined_three_times);

    RUN_TEST(test_sema_slice_2_each_unary_operator);
    RUN_TEST(test_sema_slice_2_nested_unary);
    RUN_TEST(test_sema_slice_2_parentheses);

    RUN_TEST(test_sema_slice_3_each_binary_operator);
    RUN_TEST(test_sema_slice_3_nested_expressions);
    RUN_TEST(test_sema_slice_3_typed_dump);
    RUN_TEST(test_sema_slice_3_division_by_zero_is_accepted);
    RUN_TEST(test_sema_slice_3_whole_program);

    return harness_finish();
}
