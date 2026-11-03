//
// Written by tomer on 02-Oct-26.
//

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Harness.h"
#include "Lexer.h"
#include "Token.h"
#include "Utils.h"
#include "Text.h"
#include "TextView.h"

/** The compiler's arguments for a test, as if run as `c_compiler <source> <output>`. Tests never write the output. */
static CompilationArguments test_compilation_arguments(void) {
    static const char* const argv[] = {"c_compiler", "<source>", "<unused>.s"};
    return compilation_flags_from_arguments((int)(sizeof argv / sizeof argv[0]), argv);
}

/**
 * returns the @c Token @c List which was lexed from @b buffer.
 * @attention ownership of @b buffer remains with the caller.
 */
static List* test_lexer_lex_buffer(const char* const buffer, const int length) {
    Context* context = context_create(
        source_text_create(
            text_create_from_buffer(
                buffer,
                length),
            "<source>"),
        test_compilation_arguments());
    Lexer* lexer = lexer_create(context, 0);
    List* tokens = lexer_lex_source_text(lexer);
    lexer_destroy(lexer);
    context_destroy(context);
    return tokens;
}
static bool are_tokens_equal(const Token* first, const Token* second, const Context* const context_first, const Context* const context_second) {
    assert(first != NULL && second != NULL);
    assert(first->kind != TOKEN_UNKNOWN && second->kind != TOKEN_UNKNOWN);

    if (!range_equals(first->range, second->range))
        return false;   /* note: range_equals assumes the underling Context-s are the same.
                         * They are not in this case. However, it is valid since range_equals
                         * only checks the position offset and length, and this is what we
                         * want to find out.
                         */
    if (first->kind == TOKEN_EOF && second->kind == TOKEN_EOF)
        return true;
    if (first->kind != second->kind)
        return false;

    const TokenKind kind = first->kind; // could've been second->kind as well.
    if (kind == TOKEN_CONST_INT)
        return first->as.value == second->as.value;
    if (kind == TOKEN_IDENT) {
        const TextView* first_spelling = list_at_const(context_first->identifiers_table, first->as.id);
        const TextView* second_spelling = list_at_const(context_second->identifiers_table, second->as.id);
        return text_view_equals(*first_spelling, *second_spelling);
    }
    if (kind == TOKEN_STRING_LITERAL) {
        return text_equals(
            list_at_const(context_first->string_literals_table, first->as.id),
            list_at_const(context_second->string_literals_table, second->as.id));
    }
    if (token_kind_is_keyword(kind) || token_kind_is_punctuator(kind)) {
        return true; // because all other conditions has already been verified.
    }

    assert(false && "Unknown token kind");
    return false; // must return some value, should not get here.
}
static bool are_token_lists_equal(
    const List* const first,
    const List* const second,
    const Context* const context_first,
    const Context* const context_second) {
    if (list_get_size(first) != list_get_size(second))
        return false;
    const int N = list_get_size(first);
    for (int i = 0; i < N; ++i)
        if (!are_tokens_equal(
            list_at_const(first, i),
            list_at_const(second, i),
            context_first,
            context_second))
            return false;
    return true;
}
static Token test_token_punc_keyword_create(
    const int range__source_text_index,
    const int range__offset,
    const int range__length,
    const TokenKind kind) {
    return (Token){
        .kind = kind,
        .range = {
            .position = {
                .source_text_index = range__source_text_index,
                .offset = range__offset
            },
            .length = range__length
        }
    };
}
static Token test_token_const_int_create(
    const int range__source_text_index,
    const int range__offset,
    const int range__length,
    const int value) {
    return (Token){
        .kind = TOKEN_CONST_INT,
        .range = {
            .position = {
                .source_text_index = range__source_text_index,
                .offset = range__offset
            },
            .length = range__length
        },
        .as.value = value
    };
}
static Token test_token_ident_create(
    const int range__source_text_index,
    const int range__offset,
    const int range__length,
    Context* const context,
    const TextView spelling) {
    return (Token){
        .kind = TOKEN_IDENT,
        .range = {
            .position = {
                .source_text_index = range__source_text_index,
                .offset = range__offset
            },
            .length = range__length
        },
        .as.id = list_find_or_push(context->identifiers_table, &spelling, text_view_list_equals,
                                   text_view_list_duplicate)
    };
}
static Token test_token_string_literal_create(
    const int range__source_text_index,
    const int range__offset,
    const int range__length,
    Context* const context,
    const TextView text_view) {
    Text* text = text_create_from_view(text_view);
    const Token to_return = (Token){
        .kind = TOKEN_STRING_LITERAL,
        .range = {
            .position = {
                .source_text_index = range__source_text_index,
                .offset = range__offset
            },
            .length = range__length
        },
        .as.id = list_find_or_push(context->string_literals_table, text, text_list_equals, text_list_duplicate)
    };
    text_destroy(text);
    return to_return;
}
static Token* test_token_create_ptr(Token token) {
    Token* to_return = utils_xmalloc(sizeof (Token));
    *to_return = token;
    return to_return;
}

static void test_lexer_empty_buffer(void) {
    List* tokens = test_lexer_lex_buffer("", 0);

    CHECK(list_get_size(tokens) == 1);
    CHECK(((Token*)list_at(tokens, 0))->kind == TOKEN_EOF);

    list_destroy(tokens);
}
static void test_lexer_single_punctuator(void) {
    for (TokenKind kind = TOKEN__PUNC_FIRST; kind <= TOKEN__PUNC_LAST; ++kind) {
        const TextView view = token_kind_spelling_table[kind];
        List* tokens = test_lexer_lex_buffer(view.data, view.length);

        CHECK(list_get_size(tokens) == 2);
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == kind);
        CHECK(second->kind == TOKEN_EOF);

        list_destroy(tokens);
    }
}
static void test_lexer_single_keyword(void) {
    for (TokenKind kind = TOKEN__KW_FIRST; kind <= TOKEN__KW_LAST; ++kind) {
        const TextView view = token_kind_spelling_table[kind];
        List* tokens = test_lexer_lex_buffer(view.data, view.length);

        CHECK(list_get_size(tokens) == 2);
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == kind);
        CHECK(second->kind == TOKEN_EOF);

        list_destroy(tokens);
    }
}
static void test_lexer_single_numeric(void) {
    typedef struct {
        TextView spelling;
        int number;
    } test_numeric;
    test_numeric arr[] = {
        {.spelling = text_view_value_of_literal("0"), .number = 0},
        {.spelling = text_view_value_of_literal("1"), .number = 1},
        {.spelling = text_view_value_of_literal("2"), .number = 2},
        {.spelling = text_view_value_of_literal("3"), .number = 3},
        {.spelling = text_view_value_of_literal("4"), .number = 4},
        {.spelling = text_view_value_of_literal("5"), .number = 5},
        {.spelling = text_view_value_of_literal("6"), .number = 6},
        {.spelling = text_view_value_of_literal("7"), .number = 7},
        {.spelling = text_view_value_of_literal("8"), .number = 8},
        {.spelling = text_view_value_of_literal("9"), .number = 9},
        {.spelling = text_view_value_of_literal("10"), .number = 10},
        {.spelling = text_view_value_of_literal("2147483647"), .number = 2147483647},
        {.spelling = text_view_value_of_literal("4953728"), .number = 4953728},
        {.spelling = text_view_value_of_literal("111111111"), .number = 111111111},
        {.spelling = text_view_value_of_literal("245800002"), .number = 245800002}
    };
    const int N = sizeof(arr) / sizeof(test_numeric);

    for (int i = 0; i < N; ++i) {
        const test_numeric test = arr[i];
        List* tokens = test_lexer_lex_buffer(test.spelling.data, test.spelling.length);

        CHECK(list_get_size(tokens) == 2);
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == TOKEN_CONST_INT);
        CHECK(first->as.value == test.number);
        CHECK(second->kind == TOKEN_EOF);

        list_destroy(tokens);
    }
}
static void test_lexer_single_char(void) {
    typedef struct {
        TextView spelling;
        int number;
    } test_char;
    test_char arr[] = {
        {.spelling = text_view_value_of_literal("'0'"), .number = '0'},
        {.spelling = text_view_value_of_literal("'1'"), .number = '1'},
        {.spelling = text_view_value_of_literal("'2'"), .number = '2'},
        {.spelling = text_view_value_of_literal("'3'"), .number = '3'},
        {.spelling = text_view_value_of_literal("'4'"), .number = '4'},
        {.spelling = text_view_value_of_literal("'5'"), .number = '5'},
        {.spelling = text_view_value_of_literal("'6'"), .number = '6'},
        {.spelling = text_view_value_of_literal("'7'"), .number = '7'},
        {.spelling = text_view_value_of_literal("'8'"), .number = '8'},
        {.spelling = text_view_value_of_literal("'9'"), .number = '9'},
        {.spelling = text_view_value_of_literal("'\\0'"), .number = '\0'},
        {.spelling = text_view_value_of_literal("'\\n'"), .number = '\n'},
        {.spelling = text_view_value_of_literal("'\\r'"), .number = '\r'},
        {.spelling = text_view_value_of_literal("'\\f'"), .number = '\f'},
        {.spelling = text_view_value_of_literal("'\\a'"), .number = '\a'},
        {.spelling = text_view_value_of_literal("'\\t'"), .number = '\t'},
    };
    const int N = sizeof(arr) / sizeof(test_char);

    for (int i = 0; i < N; ++i) {
        const test_char test = arr[i];
        List* tokens = test_lexer_lex_buffer(test.spelling.data, test.spelling.length);

        CHECK(list_get_size(tokens) == 2);
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == TOKEN_CONST_INT);
        CHECK(first->as.value == test.number);
        CHECK(second->kind == TOKEN_EOF);

        list_destroy(tokens);
    }
}
static void test_lexer_single_string(void) {
    typedef struct {
        TextView source;
        TextView check_against;
    } test_string;
    test_string arr[] = {
        {.source = text_view_value_of_literal("\"\""),                                                  .check_against = text_view_value_of_literal("")},
        {.source = text_view_value_of_literal("\"'\""),                                                 .check_against = text_view_value_of_literal("'")},
        {.source = text_view_value_of_literal("\"''\""),                                                .check_against = text_view_value_of_literal("''")},
        {.source = text_view_value_of_literal("\"'''\""),                                               .check_against = text_view_value_of_literal("'''")},
        {.source = text_view_value_of_literal("\"        \""),                                          .check_against = text_view_value_of_literal("        ")},
        {.source = text_view_value_of_literal("\"\\0\""),                                               .check_against = text_view_value_of_literal("\0")},
        {.source = text_view_value_of_literal("\"a\""),                                                 .check_against = text_view_value_of_literal("a")},
        {.source = text_view_value_of_literal("\"ab\""),                                                .check_against = text_view_value_of_literal("ab")},
        {.source = text_view_value_of_literal("\"abc\""),                                               .check_against = text_view_value_of_literal("abc")},
        {.source = text_view_value_of_literal("\"a51fdg2g78\""),                                        .check_against = text_view_value_of_literal("a51fdg2g78")},
        {.source = text_view_value_of_literal("\"]\""),                                                 .check_against = text_view_value_of_literal("]")},
        {.source = text_view_value_of_literal("\"]%S&@BVBSH@*(SG@*(S*#^@\""),                           .check_against = text_view_value_of_literal("]%S&@BVBSH@*(SG@*(S*#^@")},
        {.source = text_view_value_of_literal("\"]%S&@\\nBVBSH@\\n*(SG\\r\\f\\n@*(S*#^@\""),            .check_against = text_view_value_of_literal("]%S&@\nBVBSH@\n*(SG\r\f\n@*(S*#^@")},
        {.source = text_view_value_of_literal("\"\\0]%S&@\\nBVB\\0SH@\\n*(SG\\r\\f\\n@*(S*#^@\\0\""),   .check_against = text_view_value_of_literal("\0]%S&@\nBVB\0SH@\n*(SG\r\f\n@*(S*#^@\0")},
    };
    const int N = sizeof(arr) / sizeof(test_string);

    for (int i = 0; i < N; ++i) {
        const test_string test = arr[i];

        Context* context = context_create(
            source_text_create(text_create_from_buffer(test.source.data, test.source.length), "<source>"),
            test_compilation_arguments());
        Lexer* lexer = lexer_create(context, 0);
        List* tokens = lexer_lex_source_text(lexer);

        CHECK(list_get_size(tokens) == 2);
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == TOKEN_STRING_LITERAL);
        Text* string_literal_text = text_create_from_view(test.check_against);
        CHECK(list_contains(context->string_literals_table, string_literal_text, text_view_list_equals));
        text_destroy(string_literal_text);
        CHECK(second->kind == TOKEN_EOF);

        list_destroy(tokens);
        lexer_destroy(lexer);
        context_destroy(context);
    }
}
static void test_lexer_single_identifier(void) {
    typedef struct {
        TextView spelling;
        TextView identifier;
    } test_identifier;
    test_identifier arr[] = {
        {.spelling = text_view_value_of_literal("_"), .identifier = text_view_value_of_literal("_")},
        {.spelling = text_view_value_of_literal("a"), .identifier = text_view_value_of_literal("a")},
        {.spelling = text_view_value_of_literal("abcdefg"), .identifier = text_view_value_of_literal("abcdefg")},
        {.spelling = text_view_value_of_literal("int_but_not_quite"), .identifier = text_view_value_of_literal("int_but_not_quite")},
        {.spelling = text_view_value_of_literal("is_char_or_is"), .identifier = text_view_value_of_literal("is_char_or_is")},
        {.spelling = text_view_value_of_literal("not_if_is_but_else"), .identifier = text_view_value_of_literal("not_if_is_but_else")},
        {.spelling = text_view_value_of_literal("a1_1234"), .identifier = text_view_value_of_literal("a1_1234")},
        {.spelling = text_view_value_of_literal("_122345678"), .identifier = text_view_value_of_literal("_122345678")},
    };
    const int N = sizeof(arr) / sizeof(test_identifier);

    for (int i = 0; i < N; ++i) {
        const test_identifier test = arr[i];

        Context* context = context_create(
            source_text_create(text_create_from_buffer(test.spelling.data, test.spelling.length), "<source>"),
            test_compilation_arguments());
        Lexer* lexer = lexer_create(context, 0);
        List* tokens = lexer_lex_source_text(lexer);

        CHECK(list_get_size(tokens) == 2);
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == TOKEN_IDENT);

        const TextView token_identifier_spelling = *(TextView*)list_at(context->identifiers_table, first->as.id);
        CHECK(text_view_equals(test.identifier, token_identifier_spelling));
        CHECK(second->kind == TOKEN_EOF);

        list_destroy(tokens);
        lexer_destroy(lexer);
        context_destroy(context);
    }
}
// full lexer tests:
static void test_lexer_int_x_eq_5_sc(void) {
    const TextView source = text_view_value_of_literal("int x = 5;");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    { // builds the list
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 0, 3, TOKEN_KW_INT)));
        list_push_last(expected_tokens,
                  test_token_create_ptr(
                      test_token_ident_create(0, 4, 1, expected_context, text_view_value_of_literal("x"))));
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 6, 1, TOKEN_EQUALS)));
        list_push_last(expected_tokens, test_token_create_ptr(test_token_const_int_create(   0, 8, 1, 5)));
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 9, 1, TOKEN_SEMICOLON)));
        list_push_last(expected_tokens, test_token_create_ptr((Token){.kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 10}, .length = 0}}));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}
static void test_lexer_if_num_int_eq_eq_1234_printf_hello_sc(void) {
    const TextView source = text_view_value_of_literal("if (num_int == 1234) printf(\"hello\");");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    { // builds the list
        // if
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 0, 2, TOKEN_KW_IF)));
        // (
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 3, 1, TOKEN_PARENTHESES_LEFT)));
        // num_int
        list_push_last(expected_tokens, test_token_create_ptr(test_token_ident_create(       0, 4, 7, expected_context, text_view_value_of_literal("num_int"))));
        // ==
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 12, 2, TOKEN_EQUALS_EQUALS)));
        // 1234
        list_push_last(expected_tokens, test_token_create_ptr(test_token_const_int_create(   0, 15, 4, 1234)));
        // )
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 19, 1, TOKEN_PARENTHESES_RIGHT)));
        // printf
        list_push_last(expected_tokens, test_token_create_ptr(test_token_ident_create(       0, 21, 6, expected_context, text_view_value_of_literal("printf"))));
        // (
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 27, 1, TOKEN_PARENTHESES_LEFT)));
        // "hello"
        list_push_last(expected_tokens, test_token_create_ptr(test_token_string_literal_create(0, 28, 7, expected_context, text_view_value_of_literal("hello"))));
        // )
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 35, 1, TOKEN_PARENTHESES_RIGHT)));
        // ;
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 36, 1, TOKEN_SEMICOLON)));
        // EOF
        list_push_last(expected_tokens, test_token_create_ptr((Token){.kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 37}, .length = 0}}));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

// ---------------- Step 4c regression tests ---------------- //
// One test per input from the regression list in roadmaps/testing-roadmap.md, Step 4c.
// The error cases (empty and multi-character constants, 123abc) only check the tokens and that lexing recovers.
// Their diagnostics are checked in the test_lexer_diag_* tests.

/**
 * Regression: leading whitespace before the first token (" x").
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_space_x(void) {
    const TextView source = text_view_value_of_literal(" x");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens,
                  test_token_create_ptr(
                      test_token_ident_create(0, 1, 1, expected_context, text_view_value_of_literal("x"))));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 2}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

/**
 * Regression: trailing whitespace after the last token ("x\n"). EOF sits after the newline.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_x_newline(void) {
    const TextView source = text_view_value_of_literal("x\n");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens,
                  test_token_create_ptr(
                      test_token_ident_create(0, 0, 1, expected_context, text_view_value_of_literal("x"))));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 2}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

/**
 * Regression: a file that is just "+", i.e. a punctuator that could grow (++ isn't in Cminor, but the lexer peeks
 * past it) ending exactly at EOF.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_plus(void) {
    const TextView source = text_view_value_of_literal("+");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 0, 1, TOKEN_PLUS)));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 1}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

/**
 * Regression: "==;" must split as == then ;, since "==;" isn't a punctuator (maximal munch, ADR-014).
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_eq_eq_sc(void) {
    const TextView source = text_view_value_of_literal("==;");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 0, 2, TOKEN_EQUALS_EQUALS)));
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 2, 1, TOKEN_SEMICOLON)));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 3}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

/**
 * Regression: an empty character constant ('') is one CONST_INT token with value 0, and lexing continues after it.
 * The diagnostic (empty character constant) is checked in the test_lexer_diag_* tests.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_empty_char(void) {
    const TextView source = text_view_value_of_literal("'' x");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens, test_token_create_ptr(test_token_const_int_create(0, 0, 2, 0)));
        list_push_last(expected_tokens,
                  test_token_create_ptr(
                      test_token_ident_create(0, 3, 1, expected_context, text_view_value_of_literal("x"))));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 4}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

/**
 * Regression: a multi-character constant ('ab') is one CONST_INT token, valued as its first character, and lexing
 * continues after it. The diagnostic (multi-character constant) is checked in the test_lexer_diag_* tests.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_multi_char(void) {
    const TextView source = text_view_value_of_literal("'ab' x");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens, test_token_create_ptr(test_token_const_int_create(0, 0, 4, 'a')));
        list_push_last(expected_tokens,
                  test_token_create_ptr(
                      test_token_ident_create(0, 5, 1, expected_context, text_view_value_of_literal("x"))));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 6}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

/**
 * Regression: "123abc" is ONE invalid CONST_INT token (value 0), not 123 followed by the identifier abc, and lexing
 * continues after it. The diagnostic (invalid numerical sequence) is checked in the test_lexer_diag_* tests.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_123abc(void) {
    const TextView source = text_view_value_of_literal("123abc;");
    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<expected>"),
        test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens, test_token_create_ptr(test_token_const_int_create(0, 0, 6, 0)));
        list_push_last(expected_tokens, test_token_create_ptr(test_token_punc_keyword_create(0, 6, 1, TOKEN_SEMICOLON)));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = 7}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
}

/**
 * Regression: a 4095-byte string literal (the old fixed decode buffer's limit, now gone) is one token holding all
 * 4095 bytes, not truncated.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_string_4095(void) {
    enum { CONTENT_LENGTH = 4095, SOURCE_LENGTH = CONTENT_LENGTH + 2 }; // + 2 for the quotes
    char* const buffer = utils_xmalloc(SOURCE_LENGTH);
    buffer[0] = '\"';
    memset(buffer + 1, 'a', CONTENT_LENGTH);
    buffer[SOURCE_LENGTH - 1] = '\"';

    Context* const lexer_context = context_create(
        source_text_create(text_create_from_buffer(buffer, SOURCE_LENGTH), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(lexer_context, 0);
    List* lexer_tokens = lexer_lex_source_text(lexer);

    Context* const expected_context = context_create(
        source_text_create(text_create_from_buffer(buffer, SOURCE_LENGTH), "<expected>"), test_compilation_arguments());
    List* expected_tokens = list_create(free);
    {
        // builds the list
        list_push_last(expected_tokens,
                  test_token_create_ptr(test_token_string_literal_create(
                      0, 0, SOURCE_LENGTH, expected_context, text_view_make(CONTENT_LENGTH, buffer + 1))));
        list_push_last(expected_tokens, test_token_create_ptr((Token)
        {
            .kind = TOKEN_EOF, .range = {.position = {.source_text_index = 0, .offset = SOURCE_LENGTH}, .length = 0}
        }));
    }

    CHECK(are_token_lists_equal(lexer_tokens, expected_tokens, lexer_context, expected_context));

    list_destroy(lexer_tokens);
    lexer_destroy(lexer);
    context_destroy(lexer_context);

    list_destroy(expected_tokens);
    context_destroy(expected_context);
    free(buffer);
}

/**
 * Regression: a 4096-byte string literal (one byte past the old fixed decode buffer's limit, now gone) is still
 * exactly one token holding all 4096 bytes, and lexing continues after it. There is no length limit
 * (implementation-defined.md), so this is not an error case.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_string_4096(void) {
    enum { CONTENT_LENGTH = 4096, SOURCE_LENGTH = CONTENT_LENGTH + 3 }; // + 2 for the quotes, + 1 for a trailing ';'
    char* const buffer = utils_xmalloc(SOURCE_LENGTH);
    buffer[0] = '\"';
    memset(buffer + 1, 'a', CONTENT_LENGTH);
    buffer[SOURCE_LENGTH - 2] = '\"';
    buffer[SOURCE_LENGTH - 1] = ';';

    Context* const context = context_create(
        source_text_create(text_create_from_buffer(buffer, SOURCE_LENGTH), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(context, 0);
    List* tokens = lexer_lex_source_text(lexer);

    if (CHECK(list_get_size(tokens) == 3)) {
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        const Token* const third = list_at(tokens, 2);
        CHECK(first->kind == TOKEN_STRING_LITERAL);
        CHECK(first->range.position.offset == 0 && first->range.length == SOURCE_LENGTH - 1);
        const Text* const literal = list_at_const(context->string_literals_table, first->as.id);
        CHECK(literal->length == CONTENT_LENGTH);
        CHECK(memcmp(literal->data, buffer + 1, CONTENT_LENGTH) == 0);
        CHECK(second->kind == TOKEN_SEMICOLON);
        CHECK(second->range.position.offset == SOURCE_LENGTH - 1);
        CHECK(third->kind == TOKEN_EOF);
    }

    list_destroy(tokens);
    lexer_destroy(lexer);
    context_destroy(context);
    free(buffer);
}

/**
 * Regression: identifier interning. In "a b a", both a-s get the same id, b gets a different one, and the table
 * holds exactly two identifiers.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_identifier_interning_a_b_a(void) {
    const TextView source = text_view_value_of_literal("a b a");
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(context, 0);
    List* tokens = lexer_lex_source_text(lexer);

    if (CHECK(list_get_size(tokens) == 4)) {
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        const Token* const third = list_at(tokens, 2);
        CHECK(first->kind == TOKEN_IDENT && second->kind == TOKEN_IDENT && third->kind == TOKEN_IDENT);
        CHECK(first->as.id == third->as.id);
        CHECK(first->as.id != second->as.id);
        CHECK(list_get_size(context->identifiers_table) == 2);
    }

    list_destroy(tokens);
    lexer_destroy(lexer);
    context_destroy(context);
}

/**
 * Regression (ADR-010): string literals are interned as bytes + length, so "a\0b" and "a" are different literals,
 * while two "a"-s share one id.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_string_interning_with_nul(void) {
    const TextView source = text_view_value_of_literal("\"a\\0b\" \"a\" \"a\"");
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(context, 0);
    List* tokens = lexer_lex_source_text(lexer);

    if (CHECK(list_get_size(tokens) == 4)) {
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        const Token* const third = list_at(tokens, 2);
        CHECK(first->kind == TOKEN_STRING_LITERAL && second->kind == TOKEN_STRING_LITERAL && third->kind ==
              TOKEN_STRING_LITERAL);
        CHECK(first->as.id != second->as.id);
        CHECK(second->as.id == third->as.id);
        CHECK(list_get_size(context->string_literals_table) == 2);
    }

    list_destroy(tokens);
    lexer_destroy(lexer);
    context_destroy(context);
}

// Step 9 coverage tests: each one covers lines or branches of Lexer.c that tests/coverage.sh reported as never run.

/**
 * Coverage: every whitespace character separates two tokens, so "a<w>b" is IDENT, IDENT, EOF with b at offset 2.
 * Before this, '\t', '\v', '\f' and '\r' were never lexed.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_whitespace_characters(void) {
    const char whitespace[] = {' ', '\t', '\n', '\v', '\f', '\r'};
    for (int i = 0; i < (int) sizeof whitespace; ++i) {
        const char source[] = {'a', whitespace[i], 'b'};
        List* tokens = test_lexer_lex_buffer(source, 3);

        if (CHECK(list_get_size(tokens) == 3)) {
            const Token* const first = list_at(tokens, 0);
            const Token* const second = list_at(tokens, 1);
            CHECK(first->kind == TOKEN_IDENT && first->range.position.offset == 0 && first->range.length == 1);
            CHECK(second->kind == TOKEN_IDENT && second->range.position.offset == 2 && second->range.length == 1);
            CHECK(((Token*)list_at(tokens, 2))->kind == TOKEN_EOF);
        }

        list_destroy(tokens);
    }
}

/**
 * Coverage: a word ends at the first non-word character, whatever its ASCII value: below 'A', between 'Z' and 'a'
 * ('[' '\\' ']' '^' '`') and above 'z' ('{' '|' '}' '~'). "a<c>" is IDENT, then one token for c (a punctuator or
 * UNKNOWN), then EOF.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_word_ends_at_non_word_character(void) {
    const char followers[] = {'@', '[', '\\', ']', '^', '`', '{', '|', '}', '~'};
    for (int i = 0; i < (int) sizeof followers; ++i) {
        const char source[] = {'a', followers[i]};
        List* tokens = test_lexer_lex_buffer(source, 2);

        if (CHECK(list_get_size(tokens) == 3)) {
            const Token* const first = list_at(tokens, 0);
            const Token* const second = list_at(tokens, 1);
            CHECK(first->kind == TOKEN_IDENT && first->range.position.offset == 0 && first->range.length == 1);
            CHECK(second->kind != TOKEN_IDENT && second->range.position.offset == 1 && second->range.length == 1);
            CHECK(((Token*)list_at(tokens, 2))->kind == TOKEN_EOF);
        }

        list_destroy(tokens);
    }
}

/**
 * Coverage: every escape sequence in SUBSET.md, in one string literal. The expected bytes are the values from the C
 * standard (ASCII), written as numbers on purpose, not as C escapes.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_string_all_escapes(void) {
    // the source text:  "\0\a\b\f\n\r\t\v\\\'\"\?"
    const TextView source = text_view_value_of_literal("\"\\0\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\?\"");
    const char expected[] = {0x00, 0x07, 0x08, 0x0C, 0x0A, 0x0D, 0x09, 0x0B, 0x5C, 0x27, 0x22, 0x3F};
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(context, 0);
    List* tokens = lexer_lex_source_text(lexer);

    if (CHECK(list_get_size(tokens) == 2)) {
        const Token* const first = list_at(tokens, 0);
        CHECK(first->kind == TOKEN_STRING_LITERAL);
        CHECK(first->range.position.offset == 0 && first->range.length == source.length);
        const Text* const literal = list_at_const(context->string_literals_table, first->as.id);
        if (CHECK(literal->length == (int)sizeof expected))
            CHECK(memcmp(literal->data, expected, sizeof expected) == 0);
        CHECK(((Token*)list_at(tokens, 1))->kind == TOKEN_EOF);
    }

    list_destroy(tokens);
    lexer_destroy(lexer);
    context_destroy(context);
}

/**
 * Coverage: the escape sequences that the character-constant tests did not use ('\b', '\v', '\\', '\'', '\"', '\?'),
 * each one a CONST_INT token with the standard's (ASCII) value.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_char_escapes(void) {
    const struct {
        const char* source;
        int value;
    } cases[] = {
        {"'\\b'", 0x08},
        {"'\\v'", 0x0B},
        {"'\\\\'", 0x5C},
        {"'\\''", 0x27},
        {"'\\\"'", 0x22},
        {"'\\?'", 0x3F}
    };
    for (int i = 0; i < (int) (sizeof cases / sizeof cases[0]); ++i) {
        const int length = (int) strlen(cases[i].source);
        List* tokens = test_lexer_lex_buffer(cases[i].source, length);

        if (CHECK(list_get_size(tokens) == 2)) {
            const Token* const first = list_at(tokens, 0);
            CHECK(first->kind == TOKEN_CONST_INT);
            CHECK(first->range.position.offset == 0 && first->range.length == length);
            CHECK(first->as.value == cases[i].value);
            CHECK(((Token*)list_at(tokens, 1))->kind == TOKEN_EOF);
        }

        list_destroy(tokens);
    }
}

/**
 * Coverage: an unknown escape sequence ("\q") keeps the literal one token long, and lexing continues after it. What
 * byte the sequence turns into is not checked (a placeholder: the diagnostic is what reports it).
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_string_unknown_escape(void) {
    const TextView source = text_view_value_of_literal("\"a\\qb\" x");
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(source.data, source.length), "<source>"), test_compilation_arguments());
    Lexer* lexer = lexer_create(context, 0);
    List* tokens = lexer_lex_source_text(lexer);

    if (CHECK(list_get_size(tokens) == 3)) {
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == TOKEN_STRING_LITERAL);
        CHECK(first->range.position.offset == 0 && first->range.length == 6);
        const Text* const literal = list_at_const(context->string_literals_table, first->as.id);
        if (CHECK(literal->length == 3)) {
            CHECK(literal->data[0] == 'a');
            CHECK(literal->data[2] == 'b');
        }
        CHECK(second->kind == TOKEN_IDENT && second->range.position.offset == 7);
        CHECK(((Token*)list_at(tokens, 2))->kind == TOKEN_EOF);
    }

    list_destroy(tokens);
    lexer_destroy(lexer);
    context_destroy(context);
}

/**
 * Coverage: a literal that hits the end of the file before its closing quote still ends in one token, which holds
 * what was read, followed by EOF. A lone backslash at the very end ("abc\) is the same. The diagnostic (unexpected
 * EOF) is checked in the test_lexer_diag_* tests.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_unterminated_literal_at_eof(void) {
    const struct {
        const char* source;
        TokenKind kind;
        const char* content; ///< the string's bytes; ignored for a character constant
        int value; ///< the character constant's value; ignored for a string
    } cases[] = {
        {"\"abc", TOKEN_STRING_LITERAL, "abc", 0},
        {"\"abc\\", TOKEN_STRING_LITERAL, "abc", 0},
        {"\"", TOKEN_STRING_LITERAL, "", 0},
        {"'a", TOKEN_CONST_INT, NULL, 'a'}
    };
    for (int i = 0; i < (int) (sizeof cases / sizeof cases[0]); ++i) {
        const int length = (int) strlen(cases[i].source);
        Context* const context = context_create(
            source_text_create(text_create_from_buffer(cases[i].source, length), "<source>"),
            test_compilation_arguments());
        Lexer* lexer = lexer_create(context, 0);
        List* tokens = lexer_lex_source_text(lexer);

        if (CHECK(list_get_size(tokens) == 2)) {
            const Token* const first = list_at(tokens, 0);
            const Token* const second = list_at(tokens, 1);
            CHECK(first->kind == cases[i].kind);
            CHECK(first->range.position.offset == 0 && first->range.length == length);
            if (cases[i].kind == TOKEN_STRING_LITERAL) {
                const Text* const literal = list_at_const(context->string_literals_table, first->as.id);
                const int content_length = (int) strlen(cases[i].content);
                if (CHECK(literal->length == content_length))
                    CHECK(memcmp(literal->data, cases[i].content, content_length) == 0);
            } else
                CHECK(first->as.value == cases[i].value);
            CHECK(second->kind == TOKEN_EOF && second->range.position.offset == length);
        }

        list_destroy(tokens);
        lexer_destroy(lexer);
        context_destroy(context);
    }
}

/**
 * Coverage: a literal cannot continue past a newline. In "abc<newline>x the string ends at the newline and x is an
 * identifier again (and the same for 'a<newline>x). The diagnostic (missing delimiter) is checked in the test_lexer_diag_* tests.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_unterminated_literal_at_newline(void) {
    const TextView string_source = text_view_value_of_literal("\"abc\nx");
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(string_source.data, string_source.length), "<source>"),
        test_compilation_arguments());
    Lexer* lexer = lexer_create(context, 0);
    List* tokens = lexer_lex_source_text(lexer);

    if (CHECK(list_get_size(tokens) == 3)) {
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == TOKEN_STRING_LITERAL && first->range.position.offset == 0);
        const Text* const literal = list_at_const(context->string_literals_table, first->as.id);
        if (CHECK(literal->length == 3))
            CHECK(memcmp(literal->data, "abc", 3) == 0);
        CHECK(second->kind == TOKEN_IDENT && second->range.position.offset == 5 && second->range.length == 1);
        CHECK(((Token*)list_at(tokens, 2))->kind == TOKEN_EOF);
    }

    list_destroy(tokens);
    lexer_destroy(lexer);
    context_destroy(context);

    const TextView char_source = text_view_value_of_literal("'a\nx");
    tokens = test_lexer_lex_buffer(char_source.data, char_source.length);
    if (CHECK(list_get_size(tokens) == 3)) {
        const Token* const first = list_at(tokens, 0);
        const Token* const second = list_at(tokens, 1);
        CHECK(first->kind == TOKEN_CONST_INT && first->as.value == 'a' && first->range.position.offset == 0);
        CHECK(second->kind == TOKEN_IDENT && second->range.position.offset == 3 && second->range.length == 1);
        CHECK(((Token*)list_at(tokens, 2))->kind == TOKEN_EOF);
    }
    list_destroy(tokens);
}

/**
 * Coverage: a decimal constant with a leading zero ("0123", "00") is octal, which is not supported: one CONST_INT
 * token spanning all its digits, and lexing continues after it. Its value is not checked (a placeholder: the
 * diagnostic is what reports it).
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_leading_zero_constant(void) {
    const char* const sources[] = {"0123 x", "00 x"};
    const int lengths[] = {4, 2}; // of the constant
    for (int i = 0; i < 2; ++i) {
        List* tokens = test_lexer_lex_buffer(sources[i], (int) strlen(sources[i]));

        if (CHECK(list_get_size(tokens) == 3)) {
            const Token* const first = list_at(tokens, 0);
            const Token* const second = list_at(tokens, 1);
            CHECK(first->kind == TOKEN_CONST_INT);
            CHECK(first->range.position.offset == 0 && first->range.length == lengths[i]);
            CHECK(second->kind == TOKEN_IDENT && second->range.position.offset == lengths[i] + 1);
            CHECK(((Token*)list_at(tokens, 2))->kind == TOKEN_EOF);
        }

        list_destroy(tokens);
    }
}

/**
 * Coverage: the largest int (2147483647) has its exact value. One more, and far more, is still one CONST_INT token
 * over all its digits, with no crash or signed overflow (UBSan would catch one). The value of those is not checked: it
 * is a placeholder: the "too big" diagnostic is what reports it.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_int_constant_limits(void) {
    const char* const sources[] = {"2147483647", "2147483648", "99999999999999999999"};
    for (int i = 0; i < 3; ++i) {
        const int length = (int) strlen(sources[i]);
        List* tokens = test_lexer_lex_buffer(sources[i], length);

        if (CHECK(list_get_size(tokens) == 2)) {
            const Token* const first = list_at(tokens, 0);
            CHECK(first->kind == TOKEN_CONST_INT);
            CHECK(first->range.position.offset == 0 && first->range.length == length);
            if (i == 0)
                CHECK(first->as.value == INT_MAX);
            CHECK(((Token*)list_at(tokens, 1))->kind == TOKEN_EOF);
        }

        list_destroy(tokens);
    }
}

/**
 * Coverage: a character that starts no token ('@', '$', '`', '\') is a one-character UNKNOWN token, and lexing
 * continues after it: "a<c>b" is IDENT, UNKNOWN, IDENT, EOF. The diagnostic (unknown character) is checked in the test_lexer_diag_* tests.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_unknown_character(void) {
    const char unknown[] = {'@', '$', '`', '\\'};
    for (int i = 0; i < (int) sizeof unknown; ++i) {
        const char source[] = {'a', unknown[i], 'b'};
        List* tokens = test_lexer_lex_buffer(source, 3);

        if (CHECK(list_get_size(tokens) == 4)) {
            const Token* const first = list_at(tokens, 0);
            const Token* const second = list_at(tokens, 1);
            const Token* const third = list_at(tokens, 2);
            CHECK(first->kind == TOKEN_IDENT && first->range.position.offset == 0);
            CHECK(second->kind == TOKEN_UNKNOWN && second->range.position.offset == 1 && second->range.length == 1);
            CHECK(third->kind == TOKEN_IDENT && third->range.position.offset == 2);
            CHECK(((Token*)list_at(tokens, 3))->kind == TOKEN_EOF);
        }

        list_destroy(tokens);
    }
}

/**
 * Coverage: the ends of every character class an identifier is made of ('A'..'Z', 'a'..'z', '_', '0'..'9') belong to
 * it: "AZaz_09" is one IDENT spanning all 7 characters. No test had an uppercase identifier before.
 * @note Written by Claude on 10-Oct-26, not by Tomer.
 */
static void test_lexer_identifier_character_class_ends(void) {
    const TextView source = text_view_value_of_literal("AZaz_09");
    List* tokens = test_lexer_lex_buffer(source.data, source.length);

    if (CHECK(list_get_size(tokens) == 2)) {
        const Token* const first = list_at(tokens, 0);
        CHECK(first->kind == TOKEN_IDENT);
        CHECK(first->range.position.offset == 0 && first->range.length == source.length);
        CHECK(((Token*)list_at(tokens, 1))->kind == TOKEN_EOF);
    }

    list_destroy(tokens);
}

// Step 8: diagnostics (written by Claude on 11-Oct-26, not by Tomer)

/**
 * Shorthand for building an expected @c Diagnostic in source text 0.
 */
static Diagnostic diag(const DiagnosticCode code, const int offset, const int length) {
    return (Diagnostic){
        .code = code,
        .range = {.position = {.source_text_index = 0, .offset = offset}, .length = length}
    };
}

/**
 * Prints @b source to stderr in quotes, with its newlines as backslash-n, so a failure report stays on one line.
 */
static void print_source_escaped(const char* const source) {
    fputc('"', stderr);
    for (const char* c = source; *c != '\0'; ++c) {
        if (*c == '\n')
            fputs("\\n", stderr);
        else
            fputc(*c, stderr);
    }
    fputc('"', stderr);
}

/**
 * Lexes @b source and checks that the diagnostics the lexer collected are exactly @b expected : same count, same order,
 * same code and same range (@c diagnostic_compare ). On a mismatch it also prints the source, the diagnostics that were
 * expected and the ones the lexer actually collected, each as code@offset+length (the code is the number from
 * @c DiagnosticCode ).
 * @param source a '\0'-terminated string
 * @param expected the diagnostics that should have been collected, in order
 * @param expected_count how many there are in @b expected
 */
static void check_diagnostics(const char* const source, const Diagnostic* const expected, const int expected_count) {
    Context* const context = context_create(
        source_text_create(text_create_from_buffer(source, (int) strlen(source)), "<source>"),
        test_compilation_arguments());
    Lexer* const lexer = lexer_create(context, 0);
    List* const tokens = lexer_lex_source_text(lexer);

    const List* const actual = context->diagnostic_report_table;
    const int actual_count = list_get_size(actual);
    bool passed = CHECK(actual_count == expected_count);
    if (passed) {
        for (int i = 0; i < expected_count; ++i) {
            const Diagnostic* const got = list_at_const(actual, i);
            passed = CHECK(diagnostic_equals(*got, expected[i])) && passed;
        }
    }
    if (!passed) {
        fputs("  diagnostic mismatch for source ", stderr);
        print_source_escaped(source);
        fputs(", expected", stderr);
        if (expected_count == 0)
            fputs(" none", stderr);
        for (int i = 0; i < expected_count; ++i)
            fprintf(stderr, " %d@%d+%d", (int) expected[i].code, expected[i].range.position.offset,
                    expected[i].range.length);
        fputs(", got", stderr);
        if (actual_count == 0)
            fputs(" none", stderr);
        for (int i = 0; i < actual_count; ++i) {
            const Diagnostic* const got = list_at_const(actual, i);
            fprintf(stderr, " %d@%d+%d", (int) got->code, got->range.position.offset, got->range.length);
        }
        fputc('\n', stderr);
    }

    list_destroy(tokens);
    lexer_destroy(lexer);
    context_destroy(context);
}

/**
 * Valid source produces no diagnostics: not for keywords, identifiers, punctuators, the largest int, a lone '0', or
 * well-formed literals with every kind of escape.
 */
static void test_lexer_diag_clean_input(void) {
    const char* const sources[] = {
        "",
        "int main(void) { return 2147483647; }",
        "x == 0 + 'a' - '\\n' ;",
        "\"hello\\n\\t\\\\\\\"\\0\" \"\"",
    };
    for (int i = 0; i < (int) (sizeof sources / sizeof sources[0]); ++i)
        check_diagnostics(sources[i], NULL, 0);
}

/**
 * A character that starts no token is one UNKNOWN_CHARACTER diagnostic, over exactly that character.
 */
static void test_lexer_diag_unknown_character(void) {
    const char* const sources[] = {"a@b", "a$b", "a`b", "a\\b"};
    for (int i = 0; i < 4; ++i) {
        const Diagnostic expected[] = {diag(DIAGNOSTIC_CODE_LEXER_UNKNOWN_CHARACTER, 1, 1)};
        check_diagnostics(sources[i], expected, 1);
    }
}

/**
 * An empty character constant ('') is EXPECTED_EXPRESSION over the whole literal, both quotes included.
 */
static void test_lexer_diag_empty_char(void) {
    const Diagnostic expected[] = {diag(DIAGNOSTIC_CODE_LEXER_EXPECTED_EXPRESSION, 0, 2)};
    check_diagnostics("'' x", expected, 1);
}

/**
 * A multi-character constant ('ab') is MULTI_CHARACTER_LITERAL over the whole literal.
 */
static void test_lexer_diag_multi_char(void) {
    const Diagnostic expected[] = {diag(DIAGNOSTIC_CODE_LEXER_MULTI_CHARACTER_LITERAL, 0, 4)};
    check_diagnostics("'ab' x", expected, 1);
}

/**
 * "123abc" is INVALID_NUMERICAL_SEQUENCE over the bad suffix only ("abc", offset 3, length 3), like a compiler pointing
 * at the suffix and not at the digits that were fine.
 */
static void test_lexer_diag_invalid_numerical_sequence(void) {
    const Diagnostic expected[] = {diag(DIAGNOSTIC_CODE_LEXER_INVALID_NUMERICAL_SEQUENCE, 3, 3)};
    check_diagnostics("123abc;", expected, 1);
}

/**
 * A leading zero followed by a digit (octal, unsupported) is INVALID_NUMERICAL_PREFIX over the '0' only.
 * A lone "0" is fine (test_lexer_diag_clean_input).
 */
static void test_lexer_diag_invalid_numerical_prefix(void) {
    const char* const sources[] = {"0123 x", "00 x"};
    for (int i = 0; i < 2; ++i) {
        const Diagnostic expected[] = {diag(DIAGNOSTIC_CODE_LEXER_INVALID_NUMERICAL_PREFIX, 0, 1)};
        check_diagnostics(sources[i], expected, 1);
    }
}

/**
 * A constant that doesn't fit in an int is NUMERIC_CONSTANT_OUT_OF_BOUNDS over all its digits. INT_MAX itself
 * (2147483647) is fine: that is the boundary.
 */
static void test_lexer_diag_constant_out_of_bounds(void) {
    check_diagnostics("2147483647", NULL, 0);

    const Diagnostic one_more[] = {diag(DIAGNOSTIC_CODE_LEXER_NUMERIC_CONSTANT_OUT_OF_BOUNDS, 0, 10)};
    check_diagnostics("2147483648", one_more, 1);

    const Diagnostic far_more[] = {diag(DIAGNOSTIC_CODE_LEXER_NUMERIC_CONSTANT_OUT_OF_BOUNDS, 0, 20)};
    check_diagnostics("99999999999999999999", far_more, 1);
}

/**
 * An unknown escape sequence is INVALID_ESCAPE_SEQUENCE over the backslash AND the character after it (length 2), in
 * a string and in a character constant. The literal itself is still well-formed, so it's the only diagnostic.
 * @attention FAILS until Lexer.c is fixed: @c lexer_lex_literal_to_text reads @c escaped_sequence_start after popping
 * the escaped character, so the range it reports has length 0 and starts after the escape, not at the backslash.
 */
static void test_lexer_diag_invalid_escape_sequence(void) {
    const Diagnostic in_string[] = {diag(DIAGNOSTIC_CODE_LEXER_INVALID_ESCAPE_SEQUENCE, 2, 2)};
    check_diagnostics("\"a\\qb\" x", in_string, 1);

    const Diagnostic in_char[] = {diag(DIAGNOSTIC_CODE_LEXER_INVALID_ESCAPE_SEQUENCE, 1, 2)};
    check_diagnostics("'\\q' x", in_char, 1);

    const Diagnostic first_in_string[] = {diag(DIAGNOSTIC_CODE_LEXER_INVALID_ESCAPE_SEQUENCE, 1, 2)};
    check_diagnostics("\"\\q\"", first_in_string, 1);
}

/**
 * A literal cut by a newline is EXPECTED_QUOTATION_MARKS (string) or EXPECTED_APOSTROPHE (character) over the literal
 * as the token has it, newline included (the same range as the token, see test_lexer_unterminated_literal_at_newline).
 */
static void test_lexer_diag_unterminated_at_newline(void) {
    const Diagnostic string[] = {diag(DIAGNOSTIC_CODE_LEXER_EXPECTED_QUOTATION_MARKS, 0, 5)};
    check_diagnostics("\"abc\nx", string, 1);

    const Diagnostic character[] = {diag(DIAGNOSTIC_CODE_LEXER_EXPECTED_APOSTROPHE, 0, 3)};
    check_diagnostics("'a\nx", character, 1);

    const Diagnostic empty_string[] = {diag(DIAGNOSTIC_CODE_LEXER_EXPECTED_QUOTATION_MARKS, 0, 2)};
    check_diagnostics("\"\nx", empty_string, 1);
}

/**
 * A literal cut by the end of the file is UNEXPECTED_EOF over everything from the opening quote to the end, including
 * a lone backslash at the very end.
 */
static void test_lexer_diag_unterminated_at_eof(void) {
    const struct {
        const char* source;
        int length; ///< the length of the whole literal, which is the whole source
    } cases[] = {
        {"\"abc", 4},
        {"\"abc\\", 5},
        {"\"", 1},
        {"'a", 2},
    };
    for (int i = 0; i < 4; ++i) {
        const Diagnostic expected[] = {diag(DIAGNOSTIC_CODE_LEXER_UNEXPECTED_EOF, 0, cases[i].length)};
        check_diagnostics(cases[i].source, expected, 1);
    }
}

/**
 * One mistake, one diagnostic: a character constant that is cut short and has no character at all ("'" + newline, or
 * "'" at the end of the file) reports only the missing delimiter, not also an empty-constant error. But "'ab" cut by
 * the end of the file is two mistakes, a missing delimiter and a multi-character constant, so both are reported, in
 * that order.
 */
static void test_lexer_diag_no_cascade(void) {
    const Diagnostic apostrophe_only[] = {diag(DIAGNOSTIC_CODE_LEXER_EXPECTED_APOSTROPHE, 0, 2)};
    check_diagnostics("'\nx", apostrophe_only, 1);

    const Diagnostic eof_empty[] = {diag(DIAGNOSTIC_CODE_LEXER_UNEXPECTED_EOF, 0, 1)};
    check_diagnostics("'", eof_empty, 1);

    const Diagnostic eof_multi[] = {
        diag(DIAGNOSTIC_CODE_LEXER_UNEXPECTED_EOF, 0, 3),
        diag(DIAGNOSTIC_CODE_LEXER_MULTI_CHARACTER_LITERAL, 0, 3),
    };
    check_diagnostics("'ab", eof_multi, 2);
}

/**
 * Lexing continues after an error, so several errors in one file are all reported, in the order they appear:
 * "@ 123abc $" is UNKNOWN_CHARACTER at 0, INVALID_NUMERICAL_SEQUENCE ("abc") at 5, UNKNOWN_CHARACTER at 9.
 */
static void test_lexer_diag_several_in_order(void) {
    const Diagnostic expected[] = {
        diag(DIAGNOSTIC_CODE_LEXER_UNKNOWN_CHARACTER, 0, 1),
        diag(DIAGNOSTIC_CODE_LEXER_INVALID_NUMERICAL_SEQUENCE, 5, 3),
        diag(DIAGNOSTIC_CODE_LEXER_UNKNOWN_CHARACTER, 9, 1),
    };
    check_diagnostics("@ 123abc $", expected, 3);
}

// ++ and -- (written by Claude on 21-Oct-26, not by Tomer)
// The lexer has ++ and -- tokens (maximal munch, SUBSET.md "v2 candidates"), so "a+++b" is a ++ + b. In v1 the parser
// rejects them (slice 2); the lexer reports nothing.

/** One expected token in source text 0: its kind and its range. */
typedef struct {
    TokenKind kind;
    int offset;
    int length;
} ExpectedToken;

/**
 * Lexes @b source and checks that its tokens are exactly @b expected : same count, and the same kind and range for each
 * (values and identifier spellings aren't compared). @b expected ends with the EOF token. On a mismatch it also prints
 * the source and the tokens the lexer actually produced, each as kind@offset+length (the kind is the number from
 * @c TokenKind ).
 * @param source a '\0'-terminated string
 */
static void check_tokens(const char* const source, const ExpectedToken* const expected, const int expected_count) {
    List* const tokens = test_lexer_lex_buffer(source, (int) strlen(source));

    const int actual_count = list_get_size(tokens);
    bool passed = CHECK(actual_count == expected_count);
    if (passed) {
        for (int i = 0; i < expected_count; ++i) {
            const Token* const got = list_at_const(tokens, i);
            passed = CHECK(got->kind == expected[i].kind) && passed;
            passed = CHECK(got->range.position.offset == expected[i].offset) && passed;
            passed = CHECK(got->range.length == expected[i].length) && passed;
        }
    }
    if (!passed) {
        fputs("  token mismatch for source ", stderr);
        print_source_escaped(source);
        fputs(", expected", stderr);
        for (int i = 0; i < expected_count; ++i)
            fprintf(stderr, " %d@%d+%d", (int) expected[i].kind, expected[i].offset, expected[i].length);
        fputs(", got", stderr);
        for (int i = 0; i < actual_count; ++i) {
            const Token* const got = list_at_const(tokens, i);
            fprintf(stderr, " %d@%d+%d", (int) got->kind, got->range.position.offset, got->range.length);
        }
        fputc('\n', stderr);
    }

    list_destroy(tokens);
}

/** "++" and "--" are one token each, not two '+' or two '-'. */
static void test_lexer_plus_plus_and_minus_minus(void) {
    const ExpectedToken plus_plus[] = {{TOKEN_PLUS_PLUS, 0, 2}, {TOKEN_EOF, 2, 0}};
    check_tokens("++", plus_plus, 2);

    const ExpectedToken minus_minus[] = {{TOKEN_MINUS_MINUS, 0, 2}, {TOKEN_EOF, 2, 0}};
    check_tokens("--", minus_minus, 2);
}
/** Maximal munch: "+++" is ++ then +, "---" is -- then -, and "++++" is ++ ++ (never + ++ +). */
static void test_lexer_maximal_munch_of_signs(void) {
    const ExpectedToken three_pluses[] = {{TOKEN_PLUS_PLUS, 0, 2}, {TOKEN_PLUS, 2, 1}, {TOKEN_EOF, 3, 0}};
    check_tokens("+++", three_pluses, 3);

    const ExpectedToken three_minuses[] = {{TOKEN_MINUS_MINUS, 0, 2}, {TOKEN_MINUS, 2, 1}, {TOKEN_EOF, 3, 0}};
    check_tokens("---", three_minuses, 3);

    const ExpectedToken four_pluses[] = {{TOKEN_PLUS_PLUS, 0, 2}, {TOKEN_PLUS_PLUS, 2, 2}, {TOKEN_EOF, 4, 0}};
    check_tokens("++++", four_pluses, 3);
}
/** "a+++b" is a ++ + b (SUBSET.md's example), and "x--;" is x -- ;. */
static void test_lexer_plus_plus_between_words(void) {
    const ExpectedToken a_plus_plus_plus_b[] = {
        {TOKEN_IDENT, 0, 1}, {TOKEN_PLUS_PLUS, 1, 2}, {TOKEN_PLUS, 3, 1}, {TOKEN_IDENT, 4, 1}, {TOKEN_EOF, 5, 0}
    };
    check_tokens("a+++b", a_plus_plus_plus_b, 5);

    const ExpectedToken x_minus_minus_sc[] = {
        {TOKEN_IDENT, 0, 1}, {TOKEN_MINUS_MINUS, 1, 2}, {TOKEN_SEMICOLON, 3, 1}, {TOKEN_EOF, 4, 0}
    };
    check_tokens("x--;", x_minus_minus_sc, 4);
}
/** "--2" is -- then 2: the input slice 2's parser rejects (SUBSET.md A.2.1), as a single token. */
static void test_lexer_minus_minus_2(void) {
    const ExpectedToken expected[] = {{TOKEN_MINUS_MINUS, 0, 2}, {TOKEN_CONST_INT, 2, 1}, {TOKEN_EOF, 3, 0}};
    check_tokens("--2", expected, 3);
}
/** Whitespace splits: "- -2", "+ +2" and "-\n-2" are two separate signs, which slice 2 accepts. */
static void test_lexer_signs_split_by_whitespace(void) {
    const ExpectedToken minus_space_minus[] = {
        {TOKEN_MINUS, 0, 1}, {TOKEN_MINUS, 2, 1}, {TOKEN_CONST_INT, 3, 1}, {TOKEN_EOF, 4, 0}
    };
    check_tokens("- -2", minus_space_minus, 4);

    const ExpectedToken plus_space_plus[] = {
        {TOKEN_PLUS, 0, 1}, {TOKEN_PLUS, 2, 1}, {TOKEN_CONST_INT, 3, 1}, {TOKEN_EOF, 4, 0}
    };
    check_tokens("+ +2", plus_space_plus, 4);

    const ExpectedToken minus_newline_minus[] = {
        {TOKEN_MINUS, 0, 1}, {TOKEN_MINUS, 2, 1}, {TOKEN_CONST_INT, 3, 1}, {TOKEN_EOF, 4, 0}
    };
    check_tokens("-\n-2", minus_newline_minus, 4);
}
/** Different signs never merge: "+-" is + -, "-+" is - +, and "-+-+" is four tokens. */
static void test_lexer_mixed_signs(void) {
    const ExpectedToken plus_minus[] = {{TOKEN_PLUS, 0, 1}, {TOKEN_MINUS, 1, 1}, {TOKEN_EOF, 2, 0}};
    check_tokens("+-", plus_minus, 3);

    const ExpectedToken minus_plus[] = {{TOKEN_MINUS, 0, 1}, {TOKEN_PLUS, 1, 1}, {TOKEN_EOF, 2, 0}};
    check_tokens("-+", minus_plus, 3);

    const ExpectedToken alternating[] = {
        {TOKEN_MINUS, 0, 1}, {TOKEN_PLUS, 1, 1}, {TOKEN_MINUS, 2, 1}, {TOKEN_PLUS, 3, 1}, {TOKEN_EOF, 4, 0}
    };
    check_tokens("-+-+", alternating, 5);
}
/** There's no compound assignment in Cminor, so "+=" and "-=" are a sign then '='. */
static void test_lexer_sign_equals(void) {
    const ExpectedToken plus_equals[] = {{TOKEN_PLUS, 0, 1}, {TOKEN_EQUALS, 1, 1}, {TOKEN_EOF, 2, 0}};
    check_tokens("+=", plus_equals, 3);

    const ExpectedToken minus_equals[] = {{TOKEN_MINUS, 0, 1}, {TOKEN_EQUALS, 1, 1}, {TOKEN_EOF, 2, 0}};
    check_tokens("-=", minus_equals, 3);
}
/** ++ and -- are valid tokens, so the lexer reports nothing for them: rejecting them is the parser's job. */
static void test_lexer_diag_plus_plus_minus_minus(void) {
    check_diagnostics("++ -- +++ --- a+++b x--; --2", NULL, 0);
}

int main(const int argc, char** const argv) {
    harness_init(argc, argv);
    RUN_TEST(test_lexer_empty_buffer);
    RUN_TEST(test_lexer_single_punctuator);
    RUN_TEST(test_lexer_single_keyword);
    RUN_TEST(test_lexer_single_numeric);
    RUN_TEST(test_lexer_single_char);
    RUN_TEST(test_lexer_single_string);
    RUN_TEST(test_lexer_single_identifier);
    RUN_TEST(test_lexer_int_x_eq_5_sc);
    RUN_TEST(test_lexer_if_num_int_eq_eq_1234_printf_hello_sc);
    // Step 4c regression tests (written by Claude)
    RUN_TEST(test_lexer_space_x);
    RUN_TEST(test_lexer_x_newline);
    RUN_TEST(test_lexer_plus);
    RUN_TEST(test_lexer_eq_eq_sc);
    RUN_TEST(test_lexer_empty_char);
    RUN_TEST(test_lexer_multi_char);
    RUN_TEST(test_lexer_123abc);
    RUN_TEST(test_lexer_string_4095);
    RUN_TEST(test_lexer_string_4096);
    RUN_TEST(test_lexer_identifier_interning_a_b_a);
    RUN_TEST(test_lexer_string_interning_with_nul);
    // Step 9 coverage tests (written by Claude)
    RUN_TEST(test_lexer_whitespace_characters);
    RUN_TEST(test_lexer_word_ends_at_non_word_character);
    RUN_TEST(test_lexer_string_all_escapes);
    RUN_TEST(test_lexer_char_escapes);
    RUN_TEST(test_lexer_string_unknown_escape);
    RUN_TEST(test_lexer_unterminated_literal_at_eof);
    RUN_TEST(test_lexer_unterminated_literal_at_newline);
    RUN_TEST(test_lexer_leading_zero_constant);
    RUN_TEST(test_lexer_int_constant_limits);
    RUN_TEST(test_lexer_unknown_character);
    RUN_TEST(test_lexer_identifier_character_class_ends);
    // Step 8 diagnostics tests (written by Claude)
    RUN_TEST(test_lexer_diag_clean_input);
    RUN_TEST(test_lexer_diag_unknown_character);
    RUN_TEST(test_lexer_diag_empty_char);
    RUN_TEST(test_lexer_diag_multi_char);
    RUN_TEST(test_lexer_diag_invalid_numerical_sequence);
    RUN_TEST(test_lexer_diag_invalid_numerical_prefix);
    RUN_TEST(test_lexer_diag_constant_out_of_bounds);
    RUN_TEST(test_lexer_diag_invalid_escape_sequence);
    RUN_TEST(test_lexer_diag_unterminated_at_newline);
    RUN_TEST(test_lexer_diag_unterminated_at_eof);
    RUN_TEST(test_lexer_diag_no_cascade);
    RUN_TEST(test_lexer_diag_several_in_order);
    // ++ and -- tests (written by Claude)
    RUN_TEST(test_lexer_plus_plus_and_minus_minus);
    RUN_TEST(test_lexer_maximal_munch_of_signs);
    RUN_TEST(test_lexer_plus_plus_between_words);
    RUN_TEST(test_lexer_minus_minus_2);
    RUN_TEST(test_lexer_signs_split_by_whitespace);
    RUN_TEST(test_lexer_mixed_signs);
    RUN_TEST(test_lexer_sign_equals);
    RUN_TEST(test_lexer_diag_plus_plus_minus_minus);
    return harness_finish();
}
