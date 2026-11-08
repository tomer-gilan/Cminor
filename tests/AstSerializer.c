//
// Created by tomer on 21-Oct-26. Moved to tests/ and taken over by Claude on 31-Oct-26.
//
// Every form is printed through three helpers: open_form prints "(head", begin_child prints the separator before a
// child, and close_form prints ")". The separator always comes before a child, never after one, so no form can end up
// with a stray space ("(block )") or lose one between two children. In NESTED mode, a child that starts its own line
// (a function definition or a statement) is indented by the number of forms open around it, 2 spaces each.
//

#include "AstSerializer.h"

#include <assert.h>
#include <stdlib.h>

#include "Context.h"
#include "Printer.h"
#include "Token.h"
#include "Utils.h"

struct AstSerializer {
    AstSerializationMode serialization_mode;
    const ExpressionSemantics* expression_semantics; ///< @c NULL for the untyped dump
    const Context* context;

    Printer* printer;   ///< the dump being printed; only during ast_serializer_serialize
    int open_form_count;
};

/** Where a child goes in NESTED mode. In FLAT mode both are a single space. */
typedef enum {
    CHILD_PLACEMENT_SAME_LINE,  ///< expressions, atoms, declarators, (parameters …), conditions
    CHILD_PLACEMENT_OWN_LINE,   ///< function definitions, declarations and statements

    CHILD_PLACEMENT__COUNT,
} ChildPlacement;

// Forms:

static void ast_serializer_open_form(AstSerializer* const this, const TextView head) {
    assert(this != NULL);

    printer_print_char(this->printer, '(');
    printer_print_text_view(this->printer, head);
    this->open_form_count++;
}
static void ast_serializer_begin_child(AstSerializer* const this, const ChildPlacement placement) {
    assert(this != NULL);

    if (this->serialization_mode == AST_SERIALIZATION_MODE_FLAT || placement == CHILD_PLACEMENT_SAME_LINE) {
        printer_print_char(this->printer, ' ');
        return;
    }
    assert(this->serialization_mode == AST_SERIALIZATION_MODE_NESTED && placement == CHILD_PLACEMENT_OWN_LINE);
    printer_print_char(this->printer, '\n');
    for (int i = 0; i < this->open_form_count; i++)
        printer_print_text_view(this->printer, text_view_value_of_literal("  "));
}
static void ast_serializer_close_form(AstSerializer* const this) {
    assert(this != NULL);
    assert(this->open_form_count > 0);

    printer_print_char(this->printer, ')');
    this->open_form_count--;
}

// Atoms:

static void ast_serializer_serialize_identifier(AstSerializer* const this, const int identifier_id) {
    assert(this != NULL);

    printer_print_text_view(this->printer, *(const TextView*)list_at_const(this->context->identifiers_table, identifier_id));
}
/** A type, all suffixes, read right to left (ast-dump-format.md, Atoms): @c int*[3] is an array of 3 pointers to int. */
static void ast_serializer_serialize_type(AstSerializer* const this, const Type* const type) {
    assert(this != NULL);
    assert(type != NULL);

    switch (type->kind) {
        case TYPE_KIND_INT:
            printer_print_text_view(this->printer, text_view_value_of_literal("int"));
            break;
        case TYPE_KIND_CHAR:
            printer_print_text_view(this->printer, text_view_value_of_literal("char"));
            break;
        case TYPE_KIND_VOID:
            printer_print_text_view(this->printer, text_view_value_of_literal("void"));
            break;
        case TYPE_KIND_POINTER:
            ast_serializer_serialize_type(this, type->as.pointer.type);
            printer_print_char(this->printer, '*');
            break;
        case TYPE_KIND_ARRAY:
            ast_serializer_serialize_type(this, type->as.array.type);
            printer_print_char(this->printer, '[');
            printer_print_int(this->printer, type->as.array.length);
            printer_print_char(this->printer, ']');
            break;
        case TYPE_KIND_FUNCTION:
            ast_serializer_serialize_type(this, type->as.function.return_type);
            printer_print_char(this->printer, '(');
            if (type->as.function.parameters_count == 0)
                printer_print_text_view(this->printer, text_view_value_of_literal("void"));
            for (int i = 0; i < type->as.function.parameters_count; i++) {
                if (i != 0)
                    printer_print_char(this->printer, ',');
                ast_serializer_serialize_type(this, type->as.function.parameters_types[i]);
            }
            printer_print_char(this->printer, ')');
            break;
        case TYPE_KIND_NONE:
        case TYPE_KIND__COUNT:
            assert(false && "not a type");
            break;
    }
}
/** Typed dump only: the @c :T after an expression's head or atom, with the type sema recorded for it. */
static void ast_serializer_serialize_expression_type(AstSerializer* const this, const ExpressionNode* const expression_node) {
    assert(this != NULL);

    if (this->expression_semantics == NULL)
        return;
    const Type* const type = this->expression_semantics[expression_node->id].type;
    assert(type != NULL && "sema recorded no type for this expression");
    printer_print_char(this->printer, ':');
    ast_serializer_serialize_type(this, type);
}
/** @c name:T , or @c :T for an unnamed parameter. The same in the typed and the untyped dump. */
static void ast_serializer_serialize_declarator(AstSerializer* const this, const DeclaratorNode* const declarator) {
    assert(this != NULL);
    assert(declarator != NULL);

    if (declarator->identifier.kind == TOKEN_IDENT)
        ast_serializer_serialize_identifier(this, declarator->identifier.as.id);
    printer_print_char(this->printer, ':');
    ast_serializer_serialize_type(this, declarator->type);
}

// Expressions:

static TokenKind unary_expression_kind_to_token_kind(const ExpressionUnaryKind kind) {
    switch (kind) {
        case UNARY_EXPRESSION_KIND_BINARY_NEGATE:
            return TOKEN_TILDE;
        case UNARY_EXPRESSION_KIND_LOGICAL_NOT:
            return TOKEN_EXCLAMATION;
        case UNARY_EXPRESSION_KIND_NUMERIC_PLUS:
            return TOKEN_PLUS;
        case UNARY_EXPRESSION_KIND_NUMERIC_MINUS:
            return TOKEN_MINUS;
        case UNARY_EXPRESSION_KIND_NONE:
        case UNARY_EXPRESSION_KIND__COUNT:
            break;
    }
    assert(false && "not a unary operator");
    return TOKEN_NONE;
}
static TokenKind binary_expression_kind_to_token_kind(const ExpressionBinaryKind kind) {
    switch (kind) {
        case BINARY_EXPRESSION_KIND_ADD:
            return TOKEN_PLUS;
        case BINARY_EXPRESSION_KIND_SUBTRACT:
            return TOKEN_MINUS;
        case BINARY_EXPRESSION_KIND_MULTIPLY:
            return TOKEN_STAR;
        case BINARY_EXPRESSION_KIND_DIVIDE:
            return TOKEN_FORWARD_SLASH;
        case BINARY_EXPRESSION_KIND_MODULO:
            return TOKEN_PERCENT;
        case BINARY_EXPRESSION_KIND_NONE:
        case BINARY_EXPRESSION_KIND__COUNT:
            break;
    }
    assert(false && "not a binary operator");
    return TOKEN_NONE;
}

static void ast_serializer_serialize_expression(AstSerializer* const this, const ExpressionNode* const expression_node) {
    assert(this != NULL);
    assert(expression_node != NULL);

    switch (expression_node->kind) {
        case EXPRESSION_KIND_INT_CONSTANT:
            printer_print_int(this->printer, expression_node->as.int_constant.value);
            ast_serializer_serialize_expression_type(this, expression_node);
            break;
        case EXPRESSION_KIND_UNARY:
            // The operator's own spelling: the operand count tells unary - from binary - (ast-dump-format.md, rule 5).
            ast_serializer_open_form(this, token_kind_get_spelling(unary_expression_kind_to_token_kind(expression_node->as.unary.kind)));
            ast_serializer_serialize_expression_type(this, expression_node);
            ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
            ast_serializer_serialize_expression(this, expression_node->as.unary.operand);
            ast_serializer_close_form(this);
            break;
        case EXPRESSION_KIND_BINARY:
            ast_serializer_open_form(this, token_kind_get_spelling(binary_expression_kind_to_token_kind(expression_node->as.binary.kind)));
            ast_serializer_serialize_expression_type(this, expression_node);
            ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
            ast_serializer_serialize_expression(this, expression_node->as.binary.left);
            ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
            ast_serializer_serialize_expression(this, expression_node->as.binary.right);
            ast_serializer_close_form(this);
            break;
        case EXPRESSION_KIND__COUNT:
            assert(false && "not an expression kind");
            break;
    }
}

// Statements:

static void ast_serializer_serialize_statement(AstSerializer* const this, const StatementNode* const statement_node) {
    assert(this != NULL);
    assert(statement_node != NULL);

    switch (statement_node->kind) {
        case STATEMENT_KIND_RETURN:
            ast_serializer_open_form(this, text_view_value_of_literal("return"));
            if (statement_node->as.return_statement.expression != NULL) {  // "return;" is just (return)
                ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
                ast_serializer_serialize_expression(this, statement_node->as.return_statement.expression);
            }
            ast_serializer_close_form(this);
            break;
        case STATEMENT_KIND_COMPOUND:
            ast_serializer_open_form(this, text_view_value_of_literal("block"));
            for (const BlockItem* item = statement_node->as.compound_statement.head; item != NULL; item = item->next) {
                ast_serializer_begin_child(this, CHILD_PLACEMENT_OWN_LINE);
                switch (item->kind) {
                    case BLOCK_ITEM_KIND_STATEMENT:
                        ast_serializer_serialize_statement(this, item->as.statement);
                        break;
                    case BLOCK_ITEM_KIND_DECLARATION:  // slice 6: BlockItem has no declaration member yet
                    case BLOCK_ITEM_KIND__COUNT:
                        assert(false && "not a block item kind the AST has yet");
                        break;
                }
            }
            ast_serializer_close_form(this);
            break;
        case STATEMENT_KIND__COUNT:
            assert(false && "not a statement kind");
            break;
    }
}

// External definitions:

/** @c ⟦declarator⟧ without an initializer, @c (init-declarator ⟦declarator⟧ ⟦initializer⟧) with one. */
static void ast_serializer_serialize_declarator_item(AstSerializer* const this, const DeclaratorItem* const item) {
    assert(this != NULL);
    assert(item != NULL);

    if (item->initializer == NULL) {
        ast_serializer_serialize_declarator(this, item->declarator);
        return;
    }
    ast_serializer_open_form(this, text_view_value_of_literal("init-declarator"));
    ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
    ast_serializer_serialize_declarator(this, item->declarator);
    ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
    ast_serializer_serialize_expression(this, item->initializer);
    ast_serializer_close_form(this);
}
static void ast_serializer_serialize_function_definition(AstSerializer* const this, const FunctionDefinitionNode* const function_definition) {
    assert(this != NULL);
    assert(function_definition != NULL);

    ast_serializer_open_form(this, text_view_value_of_literal("function-definition"));
    ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
    ast_serializer_serialize_declarator(this, function_definition->declarator);

    // (parameters) is the one form that prints even when empty (ast-dump-format.md, rule 3).
    ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
    ast_serializer_open_form(this, text_view_value_of_literal("parameters"));
    for (const DeclaratorItem* parameter = function_definition->parameters; parameter != NULL; parameter = parameter->next) {
        ast_serializer_begin_child(this, CHILD_PLACEMENT_SAME_LINE);
        ast_serializer_serialize_declarator_item(this, parameter);
    }
    ast_serializer_close_form(this);

    ast_serializer_begin_child(this, CHILD_PLACEMENT_OWN_LINE);
    ast_serializer_serialize_statement(this, function_definition->body);
    ast_serializer_close_form(this);
}
static void ast_serializer_serialize_translation_unit(AstSerializer* const this, const TranslationUnitNode* const translation_unit) {
    assert(this != NULL);
    assert(translation_unit != NULL);

    ast_serializer_open_form(this, text_view_value_of_literal("translation-unit"));
    for (const ExternalDeclarationItem* item = translation_unit->head; item != NULL; item = item->next) {
        ast_serializer_begin_child(this, CHILD_PLACEMENT_OWN_LINE);
        switch (item->kind) {
            case EXTERNAL_DECLARATION_ITEM_KIND_FUNCTION:
                ast_serializer_serialize_function_definition(this, item->as.external_function_definition);
                break;
            case EXTERNAL_DECLARATION_ITEM_KIND_DECLARATION:  // slice 10: the item has no declaration member yet
            case EXTERNAL_DECLARATION_ITEM_KIND__COUNT:
                assert(false && "not an external declaration kind the AST has yet");
                break;
        }
    }
    ast_serializer_close_form(this);
}

// API:

AstSerializer* ast_serializer_create(
    const Context* const context,
    const AstSerializationMode serialization_mode,
    const ExpressionSemantics* const expression_semantics) {
    assert(context != NULL);
    assert(0 <= serialization_mode && serialization_mode < AST_SERIALIZATION_MODE__COUNT);

    AstSerializer* const to_return = utils_xmalloc(sizeof (AstSerializer));
    *to_return = (AstSerializer){
        .serialization_mode = serialization_mode,
        .expression_semantics = expression_semantics,
        .context = context,
        .printer = NULL,
        .open_form_count = 0
    };
    return to_return;
}
void ast_serializer_destroy(AstSerializer* const this) {
    assert(this != NULL);

    free(this);
}

Text* ast_serializer_serialize(AstSerializer* const this, const TranslationUnitNode* const translation_unit) {
    assert(this != NULL);
    assert(translation_unit != NULL);
    assert(this->printer == NULL && this->open_form_count == 0);

    this->printer = printer_create();
    ast_serializer_serialize_translation_unit(this, translation_unit);
    assert(this->open_form_count == 0);
    if (this->serialization_mode == AST_SERIALIZATION_MODE_NESTED)
        printer_print_char(this->printer, '\n');

    Text* const dump = printer_detach_text(this->printer);
    printer_destroy(this->printer);
    this->printer = NULL;
    return dump;
}
