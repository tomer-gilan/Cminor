//
// Written by Claude on 16-Oct-26. Moved to parser_parse on 28-Oct-26. AST dump checks added on 31-Oct-26.
//
// Parser tests for slices 1 to 4: expressions, statements, function definitions, the translation unit, whole programs,
// and inputs that must be rejected. parser_parse is the parser's only parse function outside Parser.c, so every test parses
// a whole program: expressions and statements are tested inside main's body ("int main(void) { ... }"), and their
// offsets count from the start of that program. Each valid-input test checks the nodes and that no diagnostic was
// reported. Each error test checks that parser_parse returns NULL and reports exactly one diagnostic, with the expected
// code and offset.
// Expected values come from SUBSET.md, not from the parser's output. Expected dumps come from ast-dump-format.md.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "AstSerializer.h"
#include "Harness.h"
#include "Lexer.h"
#include "Parser.h"
#include "SourceText.h"
#include "Statement.h"
#include "Text.h"

/** The compiler's arguments for a test, as if run as `c_compiler <source> <output>`. Tests never write the output. */
static CompilationArguments test_compilation_arguments(void) {
    static const char* const argv[] = {"c_compiler", "<source>", "<unused>.s"};
    return compilation_flags_from_arguments((int)(sizeof argv / sizeof argv[0]), argv);
}

/** Everything one parse needs. The AST lives in @c context 's arena, so it's valid until the fixture is destroyed. */
typedef struct {
    Context* context;
    List* tokens;
    Parser* parser;
} ParseFixture;

/** Lexes @b source and creates a @c Parser on its tokens. Ownership of @b source remains with the caller. */
static ParseFixture parse_fixture_create(const char* const source) {
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(source, (int)strlen(source)), "<source>"),
        test_compilation_arguments());
    Lexer* const lexer = lexer_create(context, 0);
    List* const tokens = lexer_lex_source_text(lexer);
    lexer_destroy(lexer);

    return (ParseFixture){
        .context = context,
        .tokens = tokens,
        .parser = parser_create(tokens, context)
    };
}
static void parse_fixture_destroy(const ParseFixture fixture) {
    parser_destroy(fixture.parser);
    list_destroy(fixture.tokens);
    context_destroy(fixture.context);
}

/** The program text before main's first block item. */
#define MAIN_BODY_PREFIX "int main(void) { "
/** The program text before the expression that main's first statement returns. */
#define RETURN_PREFIX MAIN_BODY_PREFIX "return "
/** The offset of main's first block item: 17. */
static const int MAIN_BODY_OFFSET = (int)sizeof MAIN_BODY_PREFIX - 1;
/** The offset of the expression that main's first statement returns: 24. */
static const int RETURNED_EXPRESSION_OFFSET = (int)sizeof RETURN_PREFIX - 1;

/** Creates a fixture for "int main(void) { return <expression_source>; }". */
static ParseFixture parse_fixture_create_returning(const char* const expression_source) {
    char source[256];
    const int length = snprintf(source, sizeof source, RETURN_PREFIX "%s; }", expression_source);
    assert(0 < length && length < (int)sizeof source);
    return parse_fixture_create(source);
}

static bool has_no_diagnostics(const ParseFixture* const fixture) {
    return list_get_size(fixture->context->diagnostic_report_table) == 0;
}
static bool range_is(const Range range, const int offset, const int length) {
    return range.position.offset == offset && range.length == length;
}
static int block_item_count(const StatementNode* const compound) {
    int count = 0;
    for (const BlockItem* item = compound->as.compound_statement.head; item != NULL; item = item->next)
        count++;
    return count;
}
/** @return the statement of the @b index 'th block item, or @c NULL if there's no such item or it isn't a statement. */
static const StatementNode* block_item_statement_at(const StatementNode* const compound, const int index) {
    const BlockItem* item = compound->as.compound_statement.head;
    for (int i = 0; i < index && item != NULL; i++)
        item = item->next;
    if (item == NULL || item->kind != BLOCK_ITEM_KIND_STATEMENT)
        return NULL;
    return item->as.statement;
}
/** Checks that @b statement is @c return @b value; (an integer constant). */
static void check_return_of_int_constant(const StatementNode* const statement, const int value) {
    if (!CHECK(statement != NULL))
        return;
    if (!CHECK(statement->kind == STATEMENT_KIND_RETURN))
        return;
    const ExpressionNode* const expression = statement->as.return_statement.expression;
    if (!CHECK(expression != NULL))
        return;
    CHECK(expression->kind == EXPRESSION_KIND_INT_CONSTANT);
    CHECK(expression->as.int_constant.value == value);
}

static int external_declaration_count(const TranslationUnitNode* const translation_unit) {
    int count = 0;
    for (const ExternalDeclarationItem* item = translation_unit->head; item != NULL; item = item->next)
        count++;
    return count;
}
/** @return the @b index 'th external declaration, or @c NULL if there's no such declaration. */
static const ExternalDeclarationItem* external_declaration_at(const TranslationUnitNode* const translation_unit, const int index) {
    const ExternalDeclarationItem* item = translation_unit->head;
    for (int i = 0; i < index && item != NULL; i++)
        item = item->next;
    return item;
}
/** @return true iff the interned identifier @b id is spelled @b name . */
static bool identifier_is(const ParseFixture* const fixture, const int id, const char* const name) {
    const TextView* const spelling = list_at_const(fixture->context->identifiers_table, id);
    return text_view_equals(*spelling, text_view_make((int)strlen(name), name));
}
/** @return the function definition in @b item if it's named @b name and has a block body, @c NULL otherwise. */
static const FunctionDefinitionNode* check_function_definition(
    const ParseFixture* const fixture,
    const ExternalDeclarationItem* const item,
    const char* const name) {
    if (!CHECK(item != NULL) || !CHECK(item->kind == EXTERNAL_DECLARATION_ITEM_KIND_FUNCTION))
        return NULL;
    const FunctionDefinitionNode* const function = item->as.external_function_definition;
    if (!CHECK(function != NULL) || !CHECK(function->declarator != NULL))
        return NULL;
    CHECK(identifier_is(fixture, function->declarator->identifier.as.id, name));
    if (!CHECK(function->body != NULL) || !CHECK(function->body->kind == STATEMENT_KIND_COMPOUND))
        return NULL;
    return function;
}
/**
 * Parses the fixture's source, which must be one function, main, with no diagnostics.
 * @return main's body, or @c NULL (after a failed CHECK) if the program isn't that.
 */
static const StatementNode* parse_main_body(const ParseFixture* const fixture) {
    const TranslationUnitNode* const translation_unit = parser_parse(fixture->parser);
    CHECK(has_no_diagnostics(fixture));
    if (!CHECK(translation_unit != NULL) || !CHECK(external_declaration_count(translation_unit) == 1))
        return NULL;
    const FunctionDefinitionNode* const main_function =
        check_function_definition(fixture, external_declaration_at(translation_unit, 0), "main");
    return main_function == NULL ? NULL : main_function->body;
}
/** @return the expression returned by @b body 's first block item, or @c NULL (after a failed CHECK) if it isn't a return. */
static const ExpressionNode* first_returned_expression(const StatementNode* const body) {
    if (body == NULL)
        return NULL;
    const StatementNode* const statement = block_item_statement_at(body, 0);
    if (!CHECK(statement != NULL) || !CHECK(statement->kind == STATEMENT_KIND_RETURN))
        return NULL;
    return statement->as.return_statement.expression;
}
/**
 * Checks that the untyped dump of @b translation_unit in @b mode is exactly @b expected (ast-dump-format.md). On a
 * mismatch, prints both dumps.
 */
static void check_dump_in_mode(
    const ParseFixture* const fixture,
    const TranslationUnitNode* const translation_unit,
    const AstSerializationMode mode,
    const char* const expected) {
    if (!CHECK(translation_unit != NULL))
        return;
    AstSerializer* const serializer = ast_serializer_create(fixture->context, mode, NULL);
    Text* const dump = ast_serializer_serialize(serializer, translation_unit);
    const TextView actual = text_as_view(dump);
    if (!CHECK(text_view_equals(actual, text_view_make((int)strlen(expected), expected))))
        printf("    expected: %s\n    actual:   %.*s\n", expected, actual.length, actual.data);
    text_destroy(dump);
    ast_serializer_destroy(serializer);
}
/** Checks that the FLAT, untyped dump of @b translation_unit is exactly @b expected . */
static void check_dump(
    const ParseFixture* const fixture,
    const TranslationUnitNode* const translation_unit,
    const char* const expected) {
    check_dump_in_mode(fixture, translation_unit, AST_SERIALIZATION_MODE_FLAT, expected);
}

// ---------------- Expressions ---------------- //

/** "return 42;": an integer constant. The ';' isn't part of the expression. */
static void test_parser_expression_42(void) {
    const ParseFixture fixture = parse_fixture_create_returning("42");

    const ExpressionNode* const expression = first_returned_expression(parse_main_body(&fixture));
    if (CHECK(expression != NULL)) {
        CHECK(expression->kind == EXPRESSION_KIND_INT_CONSTANT);
        CHECK(expression->as.int_constant.value == 42);
        CHECK(range_is(expression->range, RETURNED_EXPRESSION_OFFSET, 2));
    }

    parse_fixture_destroy(fixture);
}
/** "return 0;": the smallest constant. */
static void test_parser_expression_0(void) {
    const ParseFixture fixture = parse_fixture_create_returning("0");

    const ExpressionNode* const expression = first_returned_expression(parse_main_body(&fixture));
    if (CHECK(expression != NULL)) {
        CHECK(expression->kind == EXPRESSION_KIND_INT_CONSTANT);
        CHECK(expression->as.int_constant.value == 0);
    }

    parse_fixture_destroy(fixture);
}

// ---------------- Statements (in main's body) ---------------- //

/** "return 42;": the statement spans from 'return' to the ';'. */
static void test_parser_statement_return_42_sc(void) {
    const ParseFixture fixture = parse_fixture_create(MAIN_BODY_PREFIX "return 42; }");

    const StatementNode* const body = parse_main_body(&fixture);
    if (body != NULL) {
        CHECK(block_item_count(body) == 1);
        const StatementNode* const statement = block_item_statement_at(body, 0);
        check_return_of_int_constant(statement, 42);
        if (statement != NULL)
            CHECK(range_is(statement->range, MAIN_BODY_OFFSET, 10));
    }

    parse_fixture_destroy(fixture);
}
/** "{}": an empty block. Its range spans both braces. */
static void test_parser_statement_empty_block(void) {
    const ParseFixture fixture = parse_fixture_create(MAIN_BODY_PREFIX "{} }");

    const StatementNode* const body = parse_main_body(&fixture);
    if (body != NULL) {
        const StatementNode* const block = block_item_statement_at(body, 0);
        if (CHECK(block != NULL) && CHECK(block->kind == STATEMENT_KIND_COMPOUND)) {
            CHECK(block_item_count(block) == 0);
            CHECK(range_is(block->range, MAIN_BODY_OFFSET, 2));
        }
    }

    parse_fixture_destroy(fixture);
}
/** "{{}}": a block whose only item is an empty block. */
static void test_parser_statement_nested_empty_block(void) {
    const ParseFixture fixture = parse_fixture_create(MAIN_BODY_PREFIX "{{}} }");

    const StatementNode* const body = parse_main_body(&fixture);
    if (body != NULL) {
        const StatementNode* const outer = block_item_statement_at(body, 0);
        if (CHECK(outer != NULL) && CHECK(outer->kind == STATEMENT_KIND_COMPOUND)) {
            CHECK(range_is(outer->range, MAIN_BODY_OFFSET, 4));
            CHECK(block_item_count(outer) == 1);

            const StatementNode* const inner = block_item_statement_at(outer, 0);
            if (CHECK(inner != NULL) && CHECK(inner->kind == STATEMENT_KIND_COMPOUND)) {
                CHECK(block_item_count(inner) == 0);
                CHECK(range_is(inner->range, MAIN_BODY_OFFSET + 1, 2));
            }
        }
    }

    parse_fixture_destroy(fixture);
}
/** "{}{}": two blocks in a row are two block items, each with its own range. */
static void test_parser_statement_two_blocks(void) {
    const ParseFixture fixture = parse_fixture_create(MAIN_BODY_PREFIX "{}{} }");

    const StatementNode* const body = parse_main_body(&fixture);
    if (body != NULL) {
        CHECK(block_item_count(body) == 2);

        const StatementNode* const first = block_item_statement_at(body, 0);
        if (CHECK(first != NULL) && CHECK(first->kind == STATEMENT_KIND_COMPOUND)) {
            CHECK(block_item_count(first) == 0);
            CHECK(range_is(first->range, MAIN_BODY_OFFSET, 2));
        }
        const StatementNode* const second = block_item_statement_at(body, 1);
        if (CHECK(second != NULL) && CHECK(second->kind == STATEMENT_KIND_COMPOUND)) {
            CHECK(block_item_count(second) == 0);
            CHECK(range_is(second->range, MAIN_BODY_OFFSET + 2, 2));
        }
    }

    parse_fixture_destroy(fixture);
}
/** "{return 42;}": a block with one statement. */
static void test_parser_statement_block_with_return(void) {
    const ParseFixture fixture = parse_fixture_create(MAIN_BODY_PREFIX "{return 42;} }");

    const StatementNode* const body = parse_main_body(&fixture);
    if (body != NULL) {
        const StatementNode* const block = block_item_statement_at(body, 0);
        if (CHECK(block != NULL) && CHECK(block->kind == STATEMENT_KIND_COMPOUND)) {
            CHECK(block_item_count(block) == 1);
            check_return_of_int_constant(block_item_statement_at(block, 0), 42);
            CHECK(range_is(block->range, MAIN_BODY_OFFSET, 12));
        }
    }

    parse_fixture_destroy(fixture);
}
/** "{{return 1;} return 2;}": block items keep their source order. */
static void test_parser_statement_block_items_in_order(void) {
    const ParseFixture fixture = parse_fixture_create(MAIN_BODY_PREFIX "{{return 1;} return 2;} }");

    const StatementNode* const body = parse_main_body(&fixture);
    if (body != NULL) {
        const StatementNode* const block = block_item_statement_at(body, 0);
        if (CHECK(block != NULL) && CHECK(block->kind == STATEMENT_KIND_COMPOUND)) {
            CHECK(block_item_count(block) == 2);

            const StatementNode* const inner = block_item_statement_at(block, 0);
            if (CHECK(inner != NULL) && CHECK(inner->kind == STATEMENT_KIND_COMPOUND)) {
                CHECK(block_item_count(inner) == 1);
                check_return_of_int_constant(block_item_statement_at(inner, 0), 1);
            }
            check_return_of_int_constant(block_item_statement_at(block, 1), 2);
        }
    }

    parse_fixture_destroy(fixture);
}

// ---------------- Function definitions and the translation unit ---------------- //

/** "int main(void) { return 2; }": the slice 1 program. The definition spans all of it. */
static void test_parser_function_definition_main(void) {
    const ParseFixture fixture = parse_fixture_create("int main(void) { return 2; }");

    const TranslationUnitNode* const translation_unit = parser_parse(fixture.parser);
    if (CHECK(translation_unit != NULL)) {
        CHECK(external_declaration_count(translation_unit) == 1);
        const FunctionDefinitionNode* const function =
            check_function_definition(&fixture, external_declaration_at(translation_unit, 0), "main");
        if (function != NULL) {
            CHECK(block_item_count(function->body) == 1);
            check_return_of_int_constant(block_item_statement_at(function->body, 0), 2);
            CHECK(range_is(function->body->range, 15, 13));
            CHECK(range_is(function->range, 0, 28));
        }
    }
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** "int foo(void){return 0;}": the name is any identifier, not just main. */
static void test_parser_function_definition_any_name(void) {
    const ParseFixture fixture = parse_fixture_create("int foo(void){return 0;}");

    const TranslationUnitNode* const translation_unit = parser_parse(fixture.parser);
    if (CHECK(translation_unit != NULL)) {
        CHECK(external_declaration_count(translation_unit) == 1);
        const FunctionDefinitionNode* const function =
            check_function_definition(&fixture, external_declaration_at(translation_unit, 0), "foo");
        if (function != NULL) {
            CHECK(block_item_count(function->body) == 1);
            check_return_of_int_constant(block_item_statement_at(function->body, 0), 0);
        }
    }
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** "int main(void){}": an empty body is still a block. */
static void test_parser_function_definition_empty_body(void) {
    const ParseFixture fixture = parse_fixture_create("int main(void){}");

    const StatementNode* const body = parse_main_body(&fixture);
    if (body != NULL)
        CHECK(block_item_count(body) == 0);

    parse_fixture_destroy(fixture);
}
/**
 * "int f(void){return 1;} int g(void){return 2;}": definitions keep their source order, and the translation unit spans
 * from the first one's start to EOF (here, right after the last one's end).
 */
static void test_parser_translation_unit_two_functions(void) {
    const ParseFixture fixture = parse_fixture_create("int f(void){return 1;} int g(void){return 2;}");

    const TranslationUnitNode* const translation_unit = parser_parse(fixture.parser);
    if (CHECK(translation_unit != NULL)) {
        CHECK(external_declaration_count(translation_unit) == 2);
        CHECK(range_is(translation_unit->range, 0, 45));

        const FunctionDefinitionNode* const first =
            check_function_definition(&fixture, external_declaration_at(translation_unit, 0), "f");
        if (first != NULL) {
            check_return_of_int_constant(block_item_statement_at(first->body, 0), 1);
            CHECK(range_is(first->range, 0, 22));
        }
        const FunctionDefinitionNode* const second =
            check_function_definition(&fixture, external_declaration_at(translation_unit, 1), "g");
        if (second != NULL) {
            check_return_of_int_constant(block_item_statement_at(second->body, 0), 2);
            CHECK(range_is(second->range, 23, 22));
        }
    }
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** "int f(void){return 1;} int f(void){return 2;}": two definitions with the same name parse; rejecting them is sema's job. */
static void test_parser_translation_unit_same_name_twice(void) {
    const ParseFixture fixture = parse_fixture_create("int f(void){return 1;} int f(void){return 2;}");

    const TranslationUnitNode* const translation_unit = parser_parse(fixture.parser);
    if (CHECK(translation_unit != NULL)) {
        CHECK(external_declaration_count(translation_unit) == 2);
        check_function_definition(&fixture, external_declaration_at(translation_unit, 0), "f");
        check_function_definition(&fixture, external_declaration_at(translation_unit, 1), "f");
    }
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}

// ---------------- Whole programs ---------------- //

/** The slice 1 program as it would sit in a file: newlines, indentation and a trailing newline. */
static void test_parser_parse_slice_1_program(void) {
    const ParseFixture fixture = parse_fixture_create(
        "int main(void) {\n"
        "    return 2;\n"
        "}\n");

    const TranslationUnitNode* const translation_unit = parser_parse(fixture.parser);
    if (CHECK(translation_unit != NULL)) {
        CHECK(external_declaration_count(translation_unit) == 1);
        const FunctionDefinitionNode* const function =
            check_function_definition(&fixture, external_declaration_at(translation_unit, 0), "main");
        if (function != NULL) {
            CHECK(block_item_count(function->body) == 1);
            check_return_of_int_constant(block_item_statement_at(function->body, 0), 2);
        }
        CHECK(range_is(translation_unit->range, 0, 33)); // up to EOF, so the trailing newline is included
    }
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** The slice 1 program's dump: pins slice 1's output before slice 2 adds nodes. */
static void test_parser_dump_slice_1_program(void) {
    const ParseFixture fixture = parse_fixture_create(
        "int main(void) {\n"
        "    return 2;\n"
        "}\n");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit (function-definition main:int(void) (parameters) (block (return 2))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/**
 * Blocks in the dump: an empty one is "(block)", and every child is separated by exactly one space, so neither an empty
 * block, a nested one nor two in a row leaves a stray or a missing space. Two functions check the same between
 * definitions.
 */
static void test_parser_dump_blocks_and_two_functions(void) {
    const ParseFixture fixture = parse_fixture_create(
        "int f(void) {}\n"
        "int main(void) { {} {{}} {return 1;} return 2; }\n");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit"
        " (function-definition f:int(void) (parameters) (block))"
        " (function-definition main:int(void) (parameters) (block (block) (block (block)) (block (return 1)) (return 2))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/**
 * The NESTED dump: each function definition and statement on its own line, 2 spaces deeper than its parent, closing
 * parentheses at the end of the last child's line, and one newline at the end (ast-dump-format.md, Whitespace).
 */
static void test_parser_dump_nested(void) {
    const ParseFixture fixture = parse_fixture_create(
        "int f(void) {}\n"
        "int main(void) { {} {return 1;} return 2; }\n");

    check_dump_in_mode(&fixture, parser_parse(fixture.parser), AST_SERIALIZATION_MODE_NESTED,
        "(translation-unit\n"
        "  (function-definition f:int(void) (parameters)\n"
        "    (block))\n"
        "  (function-definition main:int(void) (parameters)\n"
        "    (block\n"
        "      (block)\n"
        "      (block\n"
        "        (return 1))\n"
        "      (return 2))))\n");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** Everything slice 1 has, together: several functions, nested and empty blocks, and statements after a block. */
static void test_parser_parse_whole_program(void) {
    const ParseFixture fixture = parse_fixture_create(
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

    const TranslationUnitNode* const translation_unit = parser_parse(fixture.parser);
    if (CHECK(translation_unit != NULL)) {
        CHECK(external_declaration_count(translation_unit) == 2);

        const FunctionDefinitionNode* const helper =
            check_function_definition(&fixture, external_declaration_at(translation_unit, 0), "helper");
        if (helper != NULL) {
            CHECK(block_item_count(helper->body) == 2);
            const StatementNode* const empty_block = block_item_statement_at(helper->body, 0);
            if (CHECK(empty_block != NULL) && CHECK(empty_block->kind == STATEMENT_KIND_COMPOUND))
                CHECK(block_item_count(empty_block) == 0);
            check_return_of_int_constant(block_item_statement_at(helper->body, 1), 7);
        }

        const FunctionDefinitionNode* const main_function =
            check_function_definition(&fixture, external_declaration_at(translation_unit, 1), "main");
        if (main_function != NULL) {
            CHECK(block_item_count(main_function->body) == 2);
            const StatementNode* const outer = block_item_statement_at(main_function->body, 0);
            if (CHECK(outer != NULL) && CHECK(outer->kind == STATEMENT_KIND_COMPOUND)) {
                CHECK(block_item_count(outer) == 1);
                const StatementNode* const inner = block_item_statement_at(outer, 0);
                if (CHECK(inner != NULL) && CHECK(inner->kind == STATEMENT_KIND_COMPOUND)) {
                    CHECK(block_item_count(inner) == 1);
                    check_return_of_int_constant(block_item_statement_at(inner, 0), 1);
                }
            }
            check_return_of_int_constant(block_item_statement_at(main_function->body, 1), 0);
        }
    }
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}

// ---------------- Errors ---------------- //
// The parser stops at the first syntax error (ADR-020): parser_parse returns NULL and exactly one diagnostic is
// reported. The expected code and offset of each one were worked out by hand from the input (the offset of the token
// the parser found instead; EOF is at the end of the input), not copied from the parser's output.

/**
 * Checks that parsing @b source as a whole program fails: @c NULL result, and exactly one diagnostic, with code
 * @b code at byte offset @b offset .
 */
static void check_parse_fails(const char* const source, const DiagnosticCode code, const int offset) {
    const ParseFixture fixture = parse_fixture_create(source);

    CHECK(parser_parse(fixture.parser) == NULL);

    const List* const diagnostics = fixture.context->diagnostic_report_table;
    if (CHECK(list_get_size(diagnostics) == 1)) {
        const Diagnostic* const diagnostic = list_at_const(diagnostics, 0);
        CHECK(diagnostic->code == code);
        CHECK(diagnostic->range.position.offset == offset);
    }

    parse_fixture_destroy(fixture);
}

/** "": SUBSET.md A.2.4: a translation unit has at least one external definition. Reported at EOF. */
static void test_parser_error_empty_file(void) {
    check_parse_fails("", DIAGNOSTIC_CODE_PARSER_MISSING_EXTERNAL_DEFINITION, 0);
}
/** "   ": whitespace only is an empty translation unit too. EOF is after the 3 spaces. */
static void test_parser_error_whitespace_only(void) {
    check_parse_fails("   ", DIAGNOSTIC_CODE_PARSER_MISSING_EXTERNAL_DEFINITION, 3);
}
/** "int main(void) { return 42 }": the ';' after the return expression is missing. Reported at the '}'. */
static void test_parser_error_missing_semicolon(void) {
    check_parse_fails("int main(void) { return 42 }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 27);
}
/** "int main(void) { return 2;": the closing '}' is missing, so the block asks for another statement at EOF. */
static void test_parser_error_missing_right_brace(void) {
    check_parse_fails("int main(void) { return 2;", DIAGNOSTIC_CODE_PARSER_INVALID_STATEMENT, 26);
}
/** "int (void) { return 2; }": the function has no name. Reported at the '('. */
static void test_parser_error_missing_function_name(void) {
    check_parse_fails("int (void) { return 2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 4);
}
/**
 * "int main(void) return 2;": the body isn't a block, and 'return' can't continue an external definition either (a '{'
 * or a ';' could). Reported at 'return'.
 */
static void test_parser_error_missing_left_brace(void) {
    check_parse_fails("int main(void) return 2;", DIAGNOSTIC_CODE_PARSER_INVALID_EXTERNAL_DEFINITION, 15);
}
/** "int main(void": EOF in the middle of the parameter list, where the ')' should be. */
static void test_parser_error_eof_in_parameter_list(void) {
    check_parse_fails("int main(void", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 13);
}
/**
 * "int main(int) { return 2; }": parameters come in slice 9, and an unnamed one stays an error in a definition.
 * Reported at 'int', where the 'void' should be; the ')' that's then also missing must not be reported too.
 */
static void test_parser_error_unnamed_parameter(void) {
    check_parse_fails("int main(int) { return 2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 9);
}
/** "int main(void) { return 2; } }": a stray '}' after the last definition, where a type specifier should be. */
static void test_parser_error_stray_right_brace(void) {
    check_parse_fails("int main(void) { return 2; } }", DIAGNOSTIC_CODE_PARSER_MISSING_TYPE_SPECIFIER, 29);
}
/** "int main(void) { return 2; } 5": a token that can't start an external declaration, where a type specifier should be. */
static void test_parser_error_trailing_token(void) {
    check_parse_fails("int main(void) { return 2; } 5", DIAGNOSTIC_CODE_PARSER_MISSING_TYPE_SPECIFIER, 29);
}

// ---------------- Slice 2: unary operators and parentheses ---------------- //
// Written ahead by Claude on 21-Oct-26, before slice 2 exists. Moved to dump strings on 31-Oct-26: the dump needs no
// unary node fields, so these compile against slice 1's AST. On since 31-Oct-26: they fail until the parser handles
// slice 2 (sema's and the e2e slice 2 tests stay off until then).

/**
 * Checks that "int main(void) { return <expression_source>; }" parses with no diagnostics, and that the returned
 * expression dumps as @b expected_expression_dump (ast-dump-format.md). The whole program is dumped, so a mismatch
 * prints the context around the expression too.
 */
static void check_returned_expression_dump(
    const char* const expression_source,
    const char* const expected_expression_dump) {
    char expected[256];
    const int length = snprintf(expected, sizeof expected,
        "(translation-unit (function-definition main:int(void) (parameters) (block (return %s))))",
        expected_expression_dump);
    assert(0 < length && length < (int)sizeof expected);

    const ParseFixture fixture = parse_fixture_create_returning(expression_source);
    check_dump(&fixture, parser_parse(fixture.parser), expected);
    CHECK(has_no_diagnostics(&fixture));
    parse_fixture_destroy(fixture);
}
/**
 * Checks that "int main(void) { return <expression_source>; }" parses with no diagnostics, and that the returned
 * expression spans @b length bytes from its start: its first token through its last, parentheses included (ADR-016).
 */
static void check_returned_expression_range(const char* const expression_source, const int length) {
    const ParseFixture fixture = parse_fixture_create_returning(expression_source);

    const ExpressionNode* const expression = first_returned_expression(parse_main_body(&fixture));
    if (CHECK(expression != NULL))
        CHECK(range_is(expression->range, RETURNED_EXPRESSION_OFFSET, length));

    parse_fixture_destroy(fixture);
}

/** "-2", "+2", "~2", "!2": each prefix operator of slice 2 on a constant is a node with one operand. */
static void test_parser_slice_2_each_unary_operator(void) {
    check_returned_expression_dump("-2", "(- 2)");
    check_returned_expression_dump("+2", "(+ 2)");
    check_returned_expression_dump("~2", "(~ 2)");
    check_returned_expression_dump("!2", "(! 2)");
}
/**
 * "~-2", "!!1", "- -2", "-+2", "+-2", "-~!+2": unary operators nest, the rightmost innermost. "- -2" and "-+2" are
 * allowed (SUBSET.md A.2.1): the two signs don't touch, or differ.
 */
static void test_parser_slice_2_nested_unary(void) {
    check_returned_expression_dump("~-2", "(~ (- 2))");
    check_returned_expression_dump("!!1", "(! (! 1))");
    check_returned_expression_dump("- -2", "(- (- 2))");
    check_returned_expression_dump("-+2", "(- (+ 2))");
    check_returned_expression_dump("+-2", "(+ (- 2))");
    check_returned_expression_dump("-~!+2", "(- (~ (! (+ 2))))");
}
/** "(2)", "(((2)))", "( 2 )": parentheses produce no node, so each is just the constant. */
static void test_parser_slice_2_parentheses(void) {
    check_returned_expression_dump("(2)", "2");
    check_returned_expression_dump("(((2)))", "2");
    check_returned_expression_dump("( 2 )", "2");
}
/**
 * "-(-(2))", "-(2)", "(-2)", "~(!(+(1)))", "((-(~2)))": parentheses around an operand, or around a whole unary
 * expression, leave the same tree as without them.
 */
static void test_parser_slice_2_unary_and_parentheses(void) {
    check_returned_expression_dump("-(-(2))", "(- (- 2))");
    check_returned_expression_dump("-(2)", "(- 2)");
    check_returned_expression_dump("(-2)", "(- 2)");
    check_returned_expression_dump("~(!(+(1)))", "(~ (! (+ 1)))");
    check_returned_expression_dump("((-(~2)))", "(- (~ 2))");
}
/**
 * "-2", "~-2", "- -2", "-~!+2": a unary expression's range spans its operator and its operand, the space in "- -2"
 * included.
 */
static void test_parser_slice_2_unary_ranges(void) {
    check_returned_expression_range("-2", 2);
    check_returned_expression_range("~-2", 3);
    check_returned_expression_range("- -2", 4);
    check_returned_expression_range("-~!+2", 5);
}
/**
 * "(2)", "( 2 )", "((2))", "(-2)", "-(2)", "-( 2 )": parentheses produce no node, so the inner node's range is widened
 * to include them (ADR-016), and an operator's range reaches its operand's closing ')'.
 */
static void test_parser_slice_2_parentheses_ranges(void) {
    check_returned_expression_range("(2)", 3);
    check_returned_expression_range("( 2 )", 5);
    check_returned_expression_range("((2))", 5);
    check_returned_expression_range("(-2)", 4);
    check_returned_expression_range("-(2)", 4);
    check_returned_expression_range("-( 2 )", 6);
}
/** "int main(void) { return -(~2); }": slice 2's example program. */
static void test_parser_slice_2_program(void) {
    const ParseFixture fixture = parse_fixture_create("int main(void) { return -(~2); }");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit (function-definition main:int(void) (parameters) (block (return (- (~ 2))))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** Everything slice 2 has, together, as it would sit in a file: two functions, unary operators, parentheses. */
static void test_parser_slice_2_whole_program(void) {
    const ParseFixture fixture = parse_fixture_create(
        "int helper(void) {\n"
        "    return !(0);\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    return - -(~(+1));\n"
        "}\n");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit"
        " (function-definition helper:int(void) (parameters) (block (return (! 0))))"
        " (function-definition main:int(void) (parameters) (block (return (- (- (~ (+ 1))))))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}

// Errors. "int main(void) { return " is 24 bytes, so the returned expression starts at offset 24.

/** "int main(void) { return (); }": empty parentheses. An expression is expected at the ')', offset 25. */
static void test_parser_slice_2_error_empty_parentheses(void) {
    check_parse_fails("int main(void) { return (); }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 25);
}
/** "int main(void) { return (2; }": the ')' is missing. Reported at the ';', offset 26. */
static void test_parser_slice_2_error_unclosed_parenthesis(void) {
    check_parse_fails("int main(void) { return (2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 26);
}
/** "int main(void) { return 2); }": a ')' with no '('. A ';' is expected at the ')', offset 25. */
static void test_parser_slice_2_error_stray_right_parenthesis(void) {
    check_parse_fails("int main(void) { return 2); }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 25);
}
/** "int main(void) { return -; }" and "... !; }": an operator with no operand. Reported at the ';', offset 25. */
static void test_parser_slice_2_error_missing_operand(void) {
    check_parse_fails("int main(void) { return -; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 25);
    check_parse_fails("int main(void) { return !; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 25);
}
/**
 * "int main(void) { return --2; }" and "... ++2; }": SUBSET.md A.2.1, no -- or ++ spelled without a space. The lexer
 * gives one -- (or ++) token, which can't start an expression. Reported at it, offset 24.
 */
static void test_parser_slice_2_error_minus_minus_and_plus_plus(void) {
    check_parse_fails("int main(void) { return --2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
    check_parse_fails("int main(void) { return ++2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
}
/** "int main(void) { return - --2; }" and "... -(--2); }": a -- after a valid '-' or '('. Reported at the --. */
static void test_parser_slice_2_error_nested_minus_minus(void) {
    check_parse_fails("int main(void) { return - --2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 26);
    check_parse_fails("int main(void) { return -(--2); }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 26);
}
/** "int main(void) { return (-); }": an operator with no operand, in parentheses. Reported at the ')', offset 26. */
static void test_parser_slice_2_error_missing_operand_in_parentheses(void) {
    check_parse_fails("int main(void) { return (-); }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 26);
}
/** "int main(void) { return -(2; }": the ')' after an operator's operand is missing. Reported at the ';', offset 27. */
static void test_parser_slice_2_error_unclosed_parenthesis_after_operator(void) {
    check_parse_fails("int main(void) { return -(2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 27);
}
/**
 * "int main(void) { return 2 ~; }": '~' is prefix only, and never binary, so it can't follow an operand. A ';' is
 * expected at the '~', offset 26.
 */
static void test_parser_slice_2_error_operator_after_operand(void) {
    check_parse_fails("int main(void) { return 2 ~; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 26);
}

// ---------------- Slice 3: arithmetic ---------------- //
// Written by Claude on 06-Nov-26.

/** "1 + 2", "1 - 2", "1 * 2", "1 / 2", "1 % 2": each binary operator of slice 3 is a node with two operands. */
static void test_parser_slice_3_each_binary_operator(void) {
    check_returned_expression_dump("1 + 2", "(+ 1 2)");
    check_returned_expression_dump("1 - 2", "(- 1 2)");
    check_returned_expression_dump("1 * 2", "(* 1 2)");
    check_returned_expression_dump("1 / 2", "(/ 1 2)");
    check_returned_expression_dump("1 % 2", "(% 1 2)");
}
/** Whitespace doesn't matter between a binary operator and its operands, unlike between two signs (slice 2). */
static void test_parser_slice_3_no_whitespace(void) {
    check_returned_expression_dump("1+2*3", "(+ 1 (* 2 3))");
    check_returned_expression_dump("1-2", "(- 1 2)");
}
/** "* / %" bind tighter than "+ -", whichever comes first. */
static void test_parser_slice_3_precedence(void) {
    check_returned_expression_dump("1 + 2 * 3", "(+ 1 (* 2 3))");
    check_returned_expression_dump("2 * 3 + 4", "(+ (* 2 3) 4)");
    check_returned_expression_dump("1 - 6 / 3", "(- 1 (/ 6 3))");
    check_returned_expression_dump("8 % 3 + 1", "(+ (% 8 3) 1)");
    check_returned_expression_dump("1 * 2 + 3 * 4", "(+ (* 1 2) (* 3 4))");
}
/**
 * A looser operator after a tighter one returns to the level that accepts it. In "1 - 2 * 3 - 4", the second '-' must
 * not become part of the first '-'s right side: that would be 1 - ((2 * 3) - 4) = -1 instead of -9.
 */
static void test_parser_slice_3_looser_operator_after_tighter(void) {
    check_returned_expression_dump("1 - 2 * 3 - 4", "(- (- 1 (* 2 3)) 4)");
    check_returned_expression_dump("1 + 2 * 3 / 4 - 5", "(- (+ 1 (/ (* 2 3) 4)) 5)");
}
/** Operators of the same precedence group to the left, the mixed ones included. */
static void test_parser_slice_3_left_associativity(void) {
    check_returned_expression_dump("1 - 2 - 3", "(- (- 1 2) 3)");
    check_returned_expression_dump("1 - 2 + 3", "(+ (- 1 2) 3)");
    check_returned_expression_dump("1 + 2 - 3", "(- (+ 1 2) 3)");
    check_returned_expression_dump("8 / 4 / 2", "(/ (/ 8 4) 2)");
    check_returned_expression_dump("8 / 4 * 2", "(* (/ 8 4) 2)");
    check_returned_expression_dump("7 % 4 % 2", "(% (% 7 4) 2)");
    check_returned_expression_dump("1 - 2 - 3 - 4", "(- (- (- 1 2) 3) 4)");
}
/** A unary operator takes one operand only, on either side of a binary operator: "-1 + 2" is not -(1 + 2). */
static void test_parser_slice_3_unary_binds_tighter(void) {
    check_returned_expression_dump("-1 + 2", "(+ (- 1) 2)");
    check_returned_expression_dump("1 + -2", "(+ 1 (- 2))");
    check_returned_expression_dump("-1 * -2", "(* (- 1) (- 2))");
    check_returned_expression_dump("1 - -2", "(- 1 (- 2))");
    check_returned_expression_dump("1 + +2", "(+ 1 (+ 2))");
    check_returned_expression_dump("!1 + 2", "(+ (! 1) 2)");
    check_returned_expression_dump("~1 * 2", "(* (~ 1) 2)");
    check_returned_expression_dump("- -1 + 2", "(+ (- (- 1)) 2)");
}
/** Parentheses override precedence and associativity, and produce no node. */
static void test_parser_slice_3_parentheses(void) {
    check_returned_expression_dump("-(1 + 2)", "(- (+ 1 2))");
    check_returned_expression_dump("(1 + 2) * 3", "(* (+ 1 2) 3)");
    check_returned_expression_dump("3 * (1 + 2)", "(* 3 (+ 1 2))");
    check_returned_expression_dump("1 - (2 - 3)", "(- 1 (- 2 3))");
    check_returned_expression_dump("((1 + 2))", "(+ 1 2)");
    check_returned_expression_dump("(-1 + 2) * -(3)", "(* (+ (- 1) 2) (- 3))");
}
/** "1 / 0", "1 % 0": a zero divisor isn't a syntax error. */
static void test_parser_slice_3_division_by_zero_parses(void) {
    check_returned_expression_dump("1 / 0", "(/ 1 0)");
    check_returned_expression_dump("1 % 0", "(% 1 0)");
}
/** A binary expression's range spans its left operand's start to its right operand's end, parentheses included. */
static void test_parser_slice_3_ranges(void) {
    check_returned_expression_range("1 + 2", 5);
    check_returned_expression_range("1 + 2 * 3", 9);
    check_returned_expression_range("1 - 2 - 3", 9);
    check_returned_expression_range("-1 + 2", 6);
    check_returned_expression_range("(1 + 2) * 3", 11);
    check_returned_expression_range("3 * (1 + 2)", 11);
    check_returned_expression_range("-(1 + 2)", 8);
}
/** "int main(void) { return 1 + 2 * 3; }": slice 3's example program. */
static void test_parser_slice_3_program(void) {
    const ParseFixture fixture = parse_fixture_create("int main(void) { return 1 + 2 * 3; }");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit (function-definition main:int(void) (parameters) (block (return (+ 1 (* 2 3))))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** Everything slice 3 has, together, as it would sit in a file. */
static void test_parser_slice_3_whole_program(void) {
    const ParseFixture fixture = parse_fixture_create(
        "int helper(void) {\n"
        "    return 10 - 3 - 2;\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    return -(1 + 2) * 3 % 4;\n"
        "}\n");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit"
        " (function-definition helper:int(void) (parameters) (block (return (- (- 10 3) 2))))"
        " (function-definition main:int(void) (parameters) (block (return (% (* (- (+ 1 2)) 3) 4)))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}

// Errors. "* 2" stays out: it is valid from slice 11 (dereference), so its expectation would flip.

/** "int main(void) { return 1 +; }" and "... 1 *; }": no right operand. Reported at the ';', offset 27. */
static void test_parser_slice_3_error_missing_right_operand(void) {
    check_parse_fails("int main(void) { return 1 +; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 27);
    check_parse_fails("int main(void) { return 1 *; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 27);
}
/** "int main(void) { return / 2; }" and "... % 2; }": '/' and '%' are never prefix. Reported at them, offset 24. */
static void test_parser_slice_3_error_missing_left_operand(void) {
    check_parse_fails("int main(void) { return / 2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
    check_parse_fails("int main(void) { return % 2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
}
/** "int main(void) { return 1 / / 2; }": two binary operators in a row. Reported at the second, offset 28. */
static void test_parser_slice_3_error_two_binary_operators(void) {
    check_parse_fails("int main(void) { return 1 / / 2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 28);
}
/** "int main(void) { return 1 2; }" and "... 1 + 2 3; }": no operator between two operands. A ';' is expected there. */
static void test_parser_slice_3_error_missing_operator(void) {
    check_parse_fails("int main(void) { return 1 2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 26);
    check_parse_fails("int main(void) { return 1 + 2 3; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 30);
}
/** "int main(void) { return 1 + ); }": a ')' where the right operand should be. Reported at it, offset 28. */
static void test_parser_slice_3_error_right_parenthesis_as_operand(void) {
    check_parse_fails("int main(void) { return 1 + ); }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 28);
}
/** "int main(void) { return (1 + 2; }": the ')' is missing. Reported at the ';', offset 30. */
static void test_parser_slice_3_error_unclosed_parenthesis(void) {
    check_parse_fails("int main(void) { return (1 + 2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 30);
}
/**
 * "int main(void) { return 1--2; }" and "... 1++2; }": SUBSET.md A.2.1, a -- or ++ token isn't a binary operator, so
 * a ';' is expected at it, offset 25. "1 - --2" reports the -- as the right operand, offset 28.
 */
static void test_parser_slice_3_error_minus_minus_and_plus_plus(void) {
    check_parse_fails("int main(void) { return 1--2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 25);
    check_parse_fails("int main(void) { return 1++2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 25);
    check_parse_fails("int main(void) { return 1 - --2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 28);
}

// ---------------- Slice 4: bitwise and shift ---------------- //
// Written by Claude on 08-Nov-26. Precedence, tightest first (C11 §6.5.5-6.5.12): "* / %", "+ -", "<< >>", "&", "^",
// "|". All of them group to the left.

/** "1 << 2", "1 >> 2", "1 & 2", "1 ^ 2", "1 | 2": each binary operator of slice 4 is a node with two operands. */
static void test_parser_slice_4_each_binary_operator(void) {
    check_returned_expression_dump("1 << 2", "(<< 1 2)");
    check_returned_expression_dump("1 >> 2", "(>> 1 2)");
    check_returned_expression_dump("1 & 2", "(& 1 2)");
    check_returned_expression_dump("1 ^ 2", "(^ 1 2)");
    check_returned_expression_dump("1 | 2", "(| 1 2)");
}
/** "1<<2+1", "8>>1", "1&2|3": no whitespace needed around the new operators. */
static void test_parser_slice_4_no_whitespace(void) {
    check_returned_expression_dump("1<<2+1", "(<< 1 (+ 2 1))");
    check_returned_expression_dump("8>>1", "(>> 8 1)");
    check_returned_expression_dump("1&2|3", "(| (& 1 2) 3)");
}
/** Shifts bind looser than "+ -" and "* / %", whichever comes first: "1 << 2 + 1" is 1 << 3, not (1 << 2) + 1. */
static void test_parser_slice_4_shift_looser_than_arithmetic(void) {
    check_returned_expression_dump("1 << 2 + 1", "(<< 1 (+ 2 1))");
    check_returned_expression_dump("1 + 2 << 3", "(<< (+ 1 2) 3)");
    check_returned_expression_dump("16 >> 1 * 2", "(>> 16 (* 1 2))");
    check_returned_expression_dump("2 * 3 >> 1", "(>> (* 2 3) 1)");
    check_returned_expression_dump("1 - 1 << 2 - 1", "(<< (- 1 1) (- 2 1))");
}
/** "&" binds tighter than "^", which binds tighter than "|", whichever comes first. */
static void test_parser_slice_4_bitwise_precedence(void) {
    check_returned_expression_dump("1 | 2 ^ 3 & 4", "(| 1 (^ 2 (& 3 4)))");
    check_returned_expression_dump("1 & 2 ^ 3 | 4", "(| (^ (& 1 2) 3) 4)");
    check_returned_expression_dump("1 ^ 2 & 3", "(^ 1 (& 2 3))");
    check_returned_expression_dump("1 & 2 ^ 3", "(^ (& 1 2) 3)");
    check_returned_expression_dump("1 | 2 & 3", "(| 1 (& 2 3))");
    check_returned_expression_dump("1 | 2 ^ 3", "(| 1 (^ 2 3))");
}
/** "& ^ |" bind looser than shifts, and so looser than arithmetic too. */
static void test_parser_slice_4_bitwise_looser_than_shift(void) {
    check_returned_expression_dump("1 & 2 << 3", "(& 1 (<< 2 3))");
    check_returned_expression_dump("1 << 2 | 3", "(| (<< 1 2) 3)");
    check_returned_expression_dump("6 ^ 1 >> 1", "(^ 6 (>> 1 1))");
    check_returned_expression_dump("1 + 2 & 3", "(& (+ 1 2) 3)");
    check_returned_expression_dump("1 | 2 * 3", "(| 1 (* 2 3))");
}
/** Operators of the same precedence group to the left, "<<" and ">>" mixed included. */
static void test_parser_slice_4_left_associativity(void) {
    check_returned_expression_dump("1 << 2 << 3", "(<< (<< 1 2) 3)");
    check_returned_expression_dump("64 >> 2 >> 1", "(>> (>> 64 2) 1)");
    check_returned_expression_dump("16 >> 2 << 1", "(<< (>> 16 2) 1)");
    check_returned_expression_dump("1 << 4 >> 2", "(>> (<< 1 4) 2)");
    check_returned_expression_dump("7 & 6 & 4", "(& (& 7 6) 4)");
    check_returned_expression_dump("1 ^ 2 ^ 3", "(^ (^ 1 2) 3)");
    check_returned_expression_dump("1 | 2 | 4", "(| (| 1 2) 4)");
}
/**
 * A looser operator after a tighter one returns to the level that accepts it, across several levels: the last '|' in
 * "1 | 2 & 3 | 4" belongs to the outer '|', not to the '&'s right side.
 */
static void test_parser_slice_4_looser_operator_after_tighter(void) {
    check_returned_expression_dump("1 | 2 & 3 | 4", "(| (| 1 (& 2 3)) 4)");
    check_returned_expression_dump("1 + 2 << 3 + 4 | 5", "(| (<< (+ 1 2) (+ 3 4)) 5)");
    check_returned_expression_dump("1 | 2 << 3 * 4 & 5", "(| 1 (& (<< 2 (* 3 4)) 5))");
    check_returned_expression_dump("1 * 2 | 3 ^ 4 & 5 << 6 + 7", "(| (* 1 2) (^ 3 (& 4 (<< 5 (+ 6 7)))))");
    check_returned_expression_dump("1 + 2 << 3 & 4 ^ 5 | 6", "(| (^ (& (<< (+ 1 2) 3) 4) 5) 6)");
}
/**
 * A unary operator takes one operand only. "1 << -2" and "-8 >> 1" parse: a negative count is undefined and a
 * negative left operand of '>>' implementation-defined, but neither is a syntax error.
 */
static void test_parser_slice_4_unary_binds_tighter(void) {
    check_returned_expression_dump("~1 & 2", "(& (~ 1) 2)");
    check_returned_expression_dump("1 & ~2", "(& 1 (~ 2))");
    check_returned_expression_dump("!1 | 2", "(| (! 1) 2)");
    check_returned_expression_dump("-8 >> 1", "(>> (- 8) 1)");
    check_returned_expression_dump("1 << -2", "(<< 1 (- 2))");
    check_returned_expression_dump("~1 ^ -2", "(^ (~ 1) (- 2))");
}
/** Parentheses override precedence and associativity, and produce no node. */
static void test_parser_slice_4_parentheses(void) {
    check_returned_expression_dump("(1 | 2) & 3", "(& (| 1 2) 3)");
    check_returned_expression_dump("1 << (2 << 3)", "(<< 1 (<< 2 3))");
    check_returned_expression_dump("(1 << 2) + 1", "(+ (<< 1 2) 1)");
    check_returned_expression_dump("~(1 ^ 2)", "(~ (^ 1 2))");
    check_returned_expression_dump("(1 | 2) ^ (3 & 4)", "(^ (| 1 2) (& 3 4))");
}
/** A binary expression's range spans its left operand's start to its right operand's end, parentheses included. */
static void test_parser_slice_4_ranges(void) {
    check_returned_expression_range("1 << 2", 6);
    check_returned_expression_range("1 | 2 & 3", 9);
    check_returned_expression_range("1 << 2 + 1", 10);
    check_returned_expression_range("(1 | 2) & 3", 11);
    check_returned_expression_range("~(1 ^ 2)", 8);
}
/** "int main(void) { return 1 << 2 + 1; }": slice 4's example program. */
static void test_parser_slice_4_program(void) {
    const ParseFixture fixture = parse_fixture_create("int main(void) { return 1 << 2 + 1; }");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit (function-definition main:int(void) (parameters) (block (return (<< 1 (+ 2 1))))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}
/** Everything slice 4 has, together, as it would sit in a file. */
static void test_parser_slice_4_whole_program(void) {
    const ParseFixture fixture = parse_fixture_create(
        "int helper(void) {\n"
        "    return ~(1 << 4) & 255;\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    return (6 ^ 3) | -8 >> 1 % 3;\n"
        "}\n");

    check_dump(&fixture, parser_parse(fixture.parser),
        "(translation-unit"
        " (function-definition helper:int(void) (parameters) (block (return (& (~ (<< 1 4)) 255))))"
        " (function-definition main:int(void) (parameters) (block (return (| (^ 6 3) (>> (- 8) (% 1 3)))))))");
    CHECK(has_no_diagnostics(&fixture));

    parse_fixture_destroy(fixture);
}

// Errors. Left out, because their expectation would flip: "1 & & 2" and "& 2" (unary '&' from slice 11), and anything
// with '<', '>', "&&" or "||" (slice 5).

/** "int main(void) { return 1 <<; }", "... 1 |; }": no right operand. Reported at the ';'. */
static void test_parser_slice_4_error_missing_right_operand(void) {
    check_parse_fails("int main(void) { return 1 <<; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 28);
    check_parse_fails("int main(void) { return 1 >>; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 28);
    check_parse_fails("int main(void) { return 1 |; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 27);
}
/** "int main(void) { return << 1; }", "... ^ 1; }", "... | 1; }": these are never prefix. Reported at them, offset 24. */
static void test_parser_slice_4_error_missing_left_operand(void) {
    check_parse_fails("int main(void) { return << 1; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
    check_parse_fails("int main(void) { return >> 1; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
    check_parse_fails("int main(void) { return ^ 1; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
    check_parse_fails("int main(void) { return | 1; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 24);
}
/** "int main(void) { return 1 & | 2; }", "... 1 << >> 2; }": two binary operators in a row. Reported at the second. */
static void test_parser_slice_4_error_two_binary_operators(void) {
    check_parse_fails("int main(void) { return 1 & | 2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 28);
    check_parse_fails("int main(void) { return 1 << >> 2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 29);
    check_parse_fails("int main(void) { return 1 ^ ^ 2; }", DIAGNOSTIC_CODE_PARSER_INVALID_EXPRESSION, 28);
}
/** "int main(void) { return (1 | 2; }": the ')' is missing. Reported at the ';', offset 30. */
static void test_parser_slice_4_error_unclosed_parenthesis(void) {
    check_parse_fails("int main(void) { return (1 | 2; }", DIAGNOSTIC_CODE_PARSER_EXPECTED_TOKEN, 30);
}

int main(const int argc, char** const argv) {
    harness_init(argc, argv);

    RUN_TEST(test_parser_expression_42);
    RUN_TEST(test_parser_expression_0);

    RUN_TEST(test_parser_statement_return_42_sc);
    RUN_TEST(test_parser_statement_empty_block);
    RUN_TEST(test_parser_statement_nested_empty_block);
    RUN_TEST(test_parser_statement_two_blocks);
    RUN_TEST(test_parser_statement_block_with_return);
    RUN_TEST(test_parser_statement_block_items_in_order);

    RUN_TEST(test_parser_function_definition_main);
    RUN_TEST(test_parser_function_definition_any_name);
    RUN_TEST(test_parser_function_definition_empty_body);
    RUN_TEST(test_parser_translation_unit_two_functions);
    RUN_TEST(test_parser_translation_unit_same_name_twice);

    RUN_TEST(test_parser_parse_slice_1_program);
    RUN_TEST(test_parser_parse_whole_program);
    RUN_TEST(test_parser_dump_slice_1_program);
    RUN_TEST(test_parser_dump_blocks_and_two_functions);
    RUN_TEST(test_parser_dump_nested);

    RUN_TEST(test_parser_error_empty_file);
    RUN_TEST(test_parser_error_whitespace_only);
    RUN_TEST(test_parser_error_missing_semicolon);
    RUN_TEST(test_parser_error_missing_right_brace);
    RUN_TEST(test_parser_error_missing_function_name);
    RUN_TEST(test_parser_error_missing_left_brace);
    RUN_TEST(test_parser_error_eof_in_parameter_list);
    RUN_TEST(test_parser_error_unnamed_parameter);
    RUN_TEST(test_parser_error_stray_right_brace);
    RUN_TEST(test_parser_error_trailing_token);

    RUN_TEST(test_parser_slice_2_each_unary_operator);
    RUN_TEST(test_parser_slice_2_nested_unary);
    RUN_TEST(test_parser_slice_2_parentheses);
    RUN_TEST(test_parser_slice_2_unary_and_parentheses);
    RUN_TEST(test_parser_slice_2_unary_ranges);
    RUN_TEST(test_parser_slice_2_parentheses_ranges);
    RUN_TEST(test_parser_slice_2_program);
    RUN_TEST(test_parser_slice_2_whole_program);

    RUN_TEST(test_parser_slice_2_error_empty_parentheses);
    RUN_TEST(test_parser_slice_2_error_unclosed_parenthesis);
    RUN_TEST(test_parser_slice_2_error_stray_right_parenthesis);
    RUN_TEST(test_parser_slice_2_error_missing_operand);
    RUN_TEST(test_parser_slice_2_error_minus_minus_and_plus_plus);
    RUN_TEST(test_parser_slice_2_error_nested_minus_minus);
    RUN_TEST(test_parser_slice_2_error_missing_operand_in_parentheses);
    RUN_TEST(test_parser_slice_2_error_unclosed_parenthesis_after_operator);
    RUN_TEST(test_parser_slice_2_error_operator_after_operand);

    RUN_TEST(test_parser_slice_3_each_binary_operator);
    RUN_TEST(test_parser_slice_3_no_whitespace);
    RUN_TEST(test_parser_slice_3_precedence);
    RUN_TEST(test_parser_slice_3_looser_operator_after_tighter);
    RUN_TEST(test_parser_slice_3_left_associativity);
    RUN_TEST(test_parser_slice_3_unary_binds_tighter);
    RUN_TEST(test_parser_slice_3_parentheses);
    RUN_TEST(test_parser_slice_3_division_by_zero_parses);
    RUN_TEST(test_parser_slice_3_ranges);
    RUN_TEST(test_parser_slice_3_program);
    RUN_TEST(test_parser_slice_3_whole_program);

    RUN_TEST(test_parser_slice_3_error_missing_right_operand);
    RUN_TEST(test_parser_slice_3_error_missing_left_operand);
    RUN_TEST(test_parser_slice_3_error_two_binary_operators);
    RUN_TEST(test_parser_slice_3_error_missing_operator);
    RUN_TEST(test_parser_slice_3_error_right_parenthesis_as_operand);
    RUN_TEST(test_parser_slice_3_error_unclosed_parenthesis);
    RUN_TEST(test_parser_slice_3_error_minus_minus_and_plus_plus);

    RUN_TEST(test_parser_slice_4_each_binary_operator);
    RUN_TEST(test_parser_slice_4_no_whitespace);
    RUN_TEST(test_parser_slice_4_shift_looser_than_arithmetic);
    RUN_TEST(test_parser_slice_4_bitwise_precedence);
    RUN_TEST(test_parser_slice_4_bitwise_looser_than_shift);
    RUN_TEST(test_parser_slice_4_left_associativity);
    RUN_TEST(test_parser_slice_4_looser_operator_after_tighter);
    RUN_TEST(test_parser_slice_4_unary_binds_tighter);
    RUN_TEST(test_parser_slice_4_parentheses);
    RUN_TEST(test_parser_slice_4_ranges);
    RUN_TEST(test_parser_slice_4_program);
    RUN_TEST(test_parser_slice_4_whole_program);

    RUN_TEST(test_parser_slice_4_error_missing_right_operand);
    RUN_TEST(test_parser_slice_4_error_missing_left_operand);
    RUN_TEST(test_parser_slice_4_error_two_binary_operators);
    RUN_TEST(test_parser_slice_4_error_unclosed_parenthesis);

    return harness_finish();
}
