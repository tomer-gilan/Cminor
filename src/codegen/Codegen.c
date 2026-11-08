//
// Created by tomer on 19-Oct-26.
//

#include "Codegen.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "Context.h"

Codegen* codegen_create(Context* const context, const TranslationUnitNode* const ast, const SemaResult sema_result) {
    assert(context != NULL);
    assert(ast != NULL);

    Codegen* to_return = utils_xmalloc(sizeof (Codegen));
    *to_return = (Codegen){
        .context = context,
        .ast = ast,
        .sema_result = sema_result,
        .emitter = emitter_create(),
    };
    return to_return;
}
void codegen_destroy(Codegen* const this) {
    assert(this != NULL);

    emitter_destroy(this->emitter);
    free(this);
}

// helpers:

static Width codegen_width_by_type(const Type* type) {
    switch (type->kind) {
        case TYPE_KIND_INT:
            return WIDTH_LONG;
        case TYPE_KIND_CHAR:
            return WIDTH_BYTE;
        // I might be wrong but these types do not have a width:
        case TYPE_KIND_FUNCTION:
        case TYPE_KIND_POINTER:
        case TYPE_KIND_ARRAY:
        case TYPE_KIND_VOID:
        case TYPE_KIND__COUNT:
        case TYPE_KIND_NONE:
        default:
            assert(false && "invalid program state");
            return -1; // fictive, should never reach this point
    }
}
static Width codegen_width_by_expression_node(
    const Codegen* const this,
    const ExpressionNode* const expression_node) {
    assert(this != NULL);
    assert(expression_node != NULL);

    const int expression_id = expression_node->id;
    assert(0 <= expression_id && expression_id < this->sema_result.expressions_count);

    return codegen_width_by_type(this->sema_result.expressions_semantics[expression_id].type);
}

// Code Generation:

static void codegen_generate_statement(Codegen* this, const StatementNode* statement_node);

/**
 * generates code that leads the expression to be saved in %rax.
 */
static void codegen_generate_expression(               Codegen* const this, const ExpressionNode*          const expression_node) {
    assert(this != NULL);
    assert(expression_node != NULL);

    const Width width = codegen_width_by_expression_node(this, expression_node);

    switch (expression_node->kind) {
        case EXPRESSION_KIND_INT_CONSTANT:
            emitter_emit_mov(
                    this->emitter,
                    width,
                    operand_make_immediate(expression_node->as.int_constant.value),
                    operand_make_register(register_make(REGISTER_KIND_RAX, width)));
            break;
        case EXPRESSION_KIND_UNARY: {
            // TODO: delegate this to a helper that gets the operand.
            codegen_generate_expression(this, expression_node->as.unary.operand);
            const Operand rax = operand_make_register(register_make(REGISTER_KIND_RAX, width));
            switch (expression_node->as.unary.kind) {
                case UNARY_EXPRESSION_KIND_BINARY_NEGATE:
                    emitter_emit_not(this->emitter, width, rax);
                    break;
                case UNARY_EXPRESSION_KIND_LOGICAL_NOT:
                    // change flags to remember if %rax was 0:
                    emitter_emit_cmp(
                        this->emitter,
                        width,
                        operand_make_immediate(0),
                        rax);
                    // zeros %rax (because 32 bit operations zero out all 64 bits):
                    emitter_emit_mov(
                        this->emitter,
                        WIDTH_LONG,
                        operand_make_immediate(0),
                        rax);
                    // stores equality check in %al (which de-facto is also %rax):
                    emitter_emit_sete(this->emitter, operand_make_register(register_make(rax.as.register_.kind, WIDTH_BYTE)));
                    break;
                case UNARY_EXPRESSION_KIND_NUMERIC_PLUS:
                    break; // no-op by design
                case UNARY_EXPRESSION_KIND_NUMERIC_MINUS:
                    emitter_emit_neg(this->emitter, width, rax);
                    break;
                case UNARY_EXPRESSION_KIND_NONE:
                case UNARY_EXPRESSION_KIND__COUNT:
                    assert(false && "invalid program state");
                    break;
            }
            break;
        }
        case EXPRESSION_KIND_BINARY: {
            // TODO: delegate this to a helper that takes both side's type

            codegen_generate_expression(this, expression_node->as.binary.left);    // result is saved in ax.
            const Operand rax = operand_make_register(register_make(REGISTER_KIND_RAX, codegen_width_by_expression_node(this, expression_node->as.binary.right)));
            emitter_emit_push(this->emitter, rax);                                  // backs it up

            codegen_generate_expression(this, expression_node->as.binary.right);     // result is saved in ax.
            // Note - we choose cx because its caller saved.
            const Operand rcx = operand_make_register(register_make(REGISTER_KIND_RCX, codegen_width_by_expression_node(this, expression_node->as.binary.left)));
            emitter_emit_mov(this->emitter, rax.as.register_.width, rax, rcx);
            emitter_emit_pop(this->emitter, rax);                                   // restores into cx.

            switch (expression_node->as.binary.kind) {
                case BINARY_EXPRESSION_KIND_ADD:
                    emitter_emit_add(this->emitter, width, rcx, rax);
                    break;
                case BINARY_EXPRESSION_KIND_SUBTRACT:
                    emitter_emit_sub(this->emitter, width, rcx, rax);
                    break;
                case BINARY_EXPRESSION_KIND_MULTIPLY:
                    emitter_emit_imul(this->emitter, width, rcx, rax);
                    break;
                case BINARY_EXPRESSION_KIND_DIVIDE:
                    emitter_emit_cltd(this->emitter);
                    emitter_emit_idiv(this->emitter, rcx);
                    break;
                case BINARY_EXPRESSION_KIND_MODULO:
                    emitter_emit_cltd(this->emitter);
                    emitter_emit_idiv(this->emitter, rcx);
                    const Operand rdx = operand_make_register(register_make(REGISTER_KIND_RDX, WIDTH_LONG));
                    emitter_emit_mov(this->emitter, width, rdx, rax);
                    break;
                case BINARY_EXPRESSION_KIND_NONE:
                case BINARY_EXPRESSION_KIND__COUNT:
                    assert(false && "invalid program state");
                    break;
            }
            break;
        }
        case EXPRESSION_KIND__COUNT:
        default:
            assert(false && "invalid program state");
            break;
    }
}
static void codegen_generate_compound_statement(        Codegen* const this, const CompoundStatement*       const compound_statement) {
    assert(this != NULL);
    assert(compound_statement != NULL);

    emitter_indent(this->emitter);
    for (const BlockItem* curr = compound_statement->head; curr != NULL; curr = curr->next) {
        switch (curr->kind) {
            case BLOCK_ITEM_KIND_STATEMENT:
                codegen_generate_statement(this, curr->as.statement);
                break;
            case BLOCK_ITEM_KIND_DECLARATION: // TODO: implement when the implementing the relevant slices
            case BLOCK_ITEM_KIND__COUNT:
            default:
                assert(false && "invalid program state");
        }
    }
    emitter_dedent(this->emitter);
}
static void codegen_generate_statement(                 Codegen* const this, const StatementNode*           const statement_node) {
    assert(this != NULL);
    assert(statement_node != NULL);

    switch (statement_node->kind) {
        case STATEMENT_KIND_RETURN:
            codegen_generate_expression(this, statement_node->as.return_statement.expression);  // moves return value to %rax
            emitter_emit_ret(this->emitter);                                                    // returns
            break;
        case STATEMENT_KIND_COMPOUND:
            codegen_generate_compound_statement(this, &(statement_node->as.compound_statement));
            break;
        case STATEMENT_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
}
static void codegen_generate_function_definition_node(  Codegen* const this, const FunctionDefinitionNode*  const function_definition_node) {
    assert(this != NULL);
    assert(function_definition_node != NULL);

    const int identifier_id = function_definition_node->declarator->identifier.as.id;

    const Label label = label_make_symbol(this->context, identifier_id);
    // External Linkage:
    emitter_emit_external_linkage(this->emitter, label);
    // Entry Point:
    emitter_emit_label(this->emitter, label);
    // Body:
    codegen_generate_statement(this, function_definition_node->body);
}
static void codegen_generate_external_definition(       Codegen* const this, const ExternalDeclarationItem* const external_definition_node) {
    assert(this != NULL);
    assert(external_definition_node != NULL);

    // TODO: in the future this function will have more meat. currently, only external FUNCTION definitions are supported.
    codegen_generate_function_definition_node(this, external_definition_node->as.external_function_definition);
}
static Text* codegen_generate_translation_unit_node(    Codegen* const this, const TranslationUnitNode*     const translation_unit_node) {
    for (const ExternalDeclarationItem* curr = translation_unit_node->head; curr != NULL; curr = curr->next) {
        codegen_generate_external_definition(this, curr);
    }
    return emitter_detach_text(this->emitter);
}

Text* codegen_generate(Codegen* const this) {
    assert(this != NULL);

    return codegen_generate_translation_unit_node(this, this->ast);
}
