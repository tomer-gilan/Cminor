//
// Created by tomer on 16-Oct-26.
//

#include "Sema.h"
#include "Context.h"
#include "ScopeStack.h"
#include "Diagnostic.h"

#include <assert.h>
#include <stdlib.h>

struct Sema {
    Context* context;
    const TranslationUnitNode* ast;
    bool error;                                 ///< error flag
    ScopeStack* scope_stack;

    ExpressionSemantics* expressions_semantics; ///< keyed by @c expression.id
    int expressions_count;
};

static void sema_set_error(Sema* this) {
    assert(this != NULL);
    this->error = true;
}
static bool sema_has_error(const Sema* const this) {
    assert(this != NULL);
    return this->error;
}
static void sema_report_diagnostic(Sema* const this, const DiagnosticCode code, const Range range) {
    assert(this != NULL);
    sema_set_error(this);
    context_report_diagnostic(this->context, code, range);
}

/**
 * records an @c ExpressionNode into an @c ExpressionSemantics entry in @c expressions_semantics
 */
static void sema_record_expression(Sema* const this, int const id, const Type* const type, const Symbol* const identifier_symbol) {
    assert(this != NULL);
    assert(0 <= id && id < this->expressions_count);

    this->expressions_semantics[id] = (ExpressionSemantics){
        .type = type,
        .identifier_symbol = identifier_symbol
    };
}
/**
 * illustration: @c ScopeStack [@c identifier.as.id ] = @c Symbol* .
 *
 * interns a @c Symbol* into the @c ScopeStack , keyed by the @c Declarator 's @c identifier.as.id
 */
static void sema_record_declarator(Sema* const this, const DeclaratorNode* const declarator) {
    assert(this != NULL);
    assert(declarator != NULL);
    const int identifier_id = declarator->identifier.as.id;
    assert(0 <= identifier_id && identifier_id < list_get_size(this->context->identifiers_table));

    scope_stack_set(this->scope_stack, identifier_id, symbol_allocate(this->context->arena, declarator));
}

Sema* sema_create(Context* const context, const TranslationUnitNode* const ast, const int expression_count) {
    assert(context != NULL);
    assert(ast != NULL);
    assert(expression_count >= 0);

    Sema* to_return = utils_xmalloc(sizeof(Sema));
    *to_return = (Sema){
        .context = context,
        .ast = ast,
        .error = false,
        .scope_stack = scope_stack_create(list_get_size(context->identifiers_table)),
        .expressions_semantics = arena_calloc(context->arena, sizeof(ExpressionSemantics), expression_count),
        .expressions_count = expression_count
    };
    return to_return;
}
void sema_destroy(Sema* const this) {
    assert(this != NULL);

    scope_stack_destroy(this->scope_stack);
    free(this);
}

// analysis:

static void sema_analyze_expression(                    Sema* const this, const ExpressionNode*             const expression_node) {
    assert(this != NULL);
    if (sema_has_error(this))
        return;
    assert(expression_node != NULL);
    assert(0 <= expression_node->id && expression_node->id < this->expressions_count);

    const Type* type = NULL;
    const Symbol* identifier_symbol = NULL;
    assert(this->expressions_semantics[expression_node->id].type == NULL);
    // TODO: check that the expression kind is identifier. then assert the symbol is NULL. then fetch the symbol from ScopeStack. if NULL, issue diagnostic (variable used but not declared), else, assign.
    switch (expression_node->kind) {
        case EXPRESSION_KIND_INT_CONSTANT:
            type = type_get_int();
            break;
        case EXPRESSION_KIND_UNARY:
            sema_analyze_expression(this, expression_node->as.unary.operand);
            if (sema_has_error(this))
                return;

            type = this->expressions_semantics[expression_node->as.unary.operand->id].type; // the implicit assumption at play: the operand's type is unary-operator-invariant. TODO: add actual type inferring logic in later slices.
            break;
        case EXPRESSION_KIND_BINARY:
            sema_analyze_expression(this, expression_node->as.binary.left);
            if (sema_has_error(this))
                return;

            sema_analyze_expression(this, expression_node->as.binary.right);
            if (sema_has_error(this))
                return;

            type = type_get_int(); // TODO: add actual type inferring logic in later slices.
            break;
        case EXPRESSION_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
    sema_record_expression(this, expression_node->id, type, identifier_symbol);
}
static void sema_analyze_statement(                     Sema* const this, const StatementNode*              const statement_node) {
    assert(this != NULL);
    if (sema_has_error(this))
        return;
    assert(statement_node != NULL);

    switch (statement_node->kind) {
        case STATEMENT_KIND_RETURN:
            sema_analyze_expression(this, statement_node->as.return_statement.expression);
            break;
        case STATEMENT_KIND_COMPOUND: {
            for (const BlockItem* curr = statement_node->as.compound_statement.head; curr != NULL; curr = curr->next) {
                // analyse the block item:
                switch (curr->kind) {
                    case BLOCK_ITEM_KIND_STATEMENT:
                        sema_analyze_statement(this, curr->as.statement);
                        break;
                    case BLOCK_ITEM_KIND_DECLARATION: // TODO: add support in future slices
                    case BLOCK_ITEM_KIND__COUNT:
                    default:
                        assert(false && "invalid program state");
                }
            }
        }
            break;
        case STATEMENT_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
}
static void sema_analyze_function_definition_node(      Sema* const this, const FunctionDefinitionNode*     const function_definition_node) {
    assert(this != NULL);
    if (sema_has_error(this))
        return;
    assert(function_definition_node != NULL);

    // TODO: in the future, also check that the function return type matches the actually returned expression.

    const int identifier_id = function_definition_node->declarator->identifier.as.id;
    if (scope_stack_was_identifier_declared_in_current_scope(this->scope_stack, identifier_id)) {
        sema_report_diagnostic(this, DIAGNOSTIC_CODE_SEMA_SYMBOL_ALREADY_DEFINED, function_definition_node->declarator->identifier.range);
        return;
    }
    sema_record_declarator(this, function_definition_node->declarator);

    sema_analyze_statement(this, function_definition_node->body);
}
static void sema_analyze_external_definition(           Sema* const this, const ExternalDeclarationItem*    const external_definition_node) {
    assert(this != NULL);
    if (sema_has_error(this))
        return;
    assert(external_definition_node != NULL);

    switch (external_definition_node->kind) {
        case EXTERNAL_DECLARATION_ITEM_KIND_FUNCTION:
            sema_analyze_function_definition_node(this, external_definition_node->as.external_function_definition);
            break;
        case EXTERNAL_DECLARATION_ITEM_KIND_DECLARATION: // TODO: add support
        case EXTERNAL_DECLARATION_ITEM_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
}
static SemaResult sema_analyze_translation_unit_node(   Sema* const this, const TranslationUnitNode*        const translation_unit_node) {
    assert(this != NULL);
    assert(translation_unit_node != NULL);

    for (const ExternalDeclarationItem* curr = translation_unit_node->head; curr != NULL; curr = curr->next) {
        sema_analyze_external_definition(this, curr);
    }
    return (SemaResult){
        .expressions_semantics = this->expressions_semantics,
        .expressions_count = this->expressions_count
    };
}

SemaResult sema_analyze(Sema* const this) {
    assert(this != NULL);

    return sema_analyze_translation_unit_node(this, this->ast);
}