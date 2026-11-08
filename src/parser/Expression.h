//
// Created by tomer on 14-Oct-26.
//

#ifndef C_COMPILER_EXPRESSION_H
#define C_COMPILER_EXPRESSION_H

typedef struct Parser Parser;
#include "Range.h"

typedef struct ExpressionNode ExpressionNode;

// IMPORTANT - when adding new entries, do not forget adding matching lookup value in Parser::token_kind_to_binary_expression_kind
typedef enum {
    // Arithmetic:
    BINARY_EXPRESSION_KIND_ADD,                     // +
    BINARY_EXPRESSION_KIND_SUBTRACT,                // -
    BINARY_EXPRESSION_KIND_MULTIPLY,                // *
    BINARY_EXPRESSION_KIND_DIVIDE,                  // /
    BINARY_EXPRESSION_KIND_MODULO,                  // %
    // Bitwise & Shift:
    BINARY_EXPRESSION_KIND_BINARY_SHIFT_LEFT,       // <<
    BINARY_EXPRESSION_KIND_BINARY_SHIFT_RIGHT,      // >>
    BINARY_EXPRESSION_KIND_BITWISE_INCLUSIVE_OR,    // |
    BINARY_EXPRESSION_KIND_BITWISE_EXCLUSIVE_OR,    // ^
    BINARY_EXPRESSION_KIND_BITWISE_AND,             // &

    BINARY_EXPRESSION_KIND_NONE,
    BINARY_EXPRESSION_KIND__COUNT,
} ExpressionBinaryKind;
typedef struct {
    ExpressionBinaryKind kind;
    const ExpressionNode* left;
    const ExpressionNode* right;
} ExpressionBinary;

// IMPORTANT - when adding new entries, do not forget adding matching lookup value in Parser::token_kind_to_unary_expression_kind
typedef enum {
    UNARY_EXPRESSION_KIND_BINARY_NEGATE,    // ~
    UNARY_EXPRESSION_KIND_LOGICAL_NOT,      // !
    UNARY_EXPRESSION_KIND_NUMERIC_PLUS,     // +
    UNARY_EXPRESSION_KIND_NUMERIC_MINUS,    // -

    UNARY_EXPRESSION_KIND_NONE,
    UNARY_EXPRESSION_KIND__COUNT,
} ExpressionUnaryKind;
typedef struct {
    ExpressionUnaryKind kind;
    const ExpressionNode* operand;
} ExpressionUnary;

typedef struct {
    int value;
} ExpressionIntConstant;

typedef enum {
    EXPRESSION_KIND_INT_CONSTANT,
    EXPRESSION_KIND_UNARY,
    EXPRESSION_KIND_BINARY,

    EXPRESSION_KIND__COUNT
} ExpressionKind;
struct ExpressionNode {
    Range range;
    int id; ///< the identifier that @c Sema uses to intern info about @c ExpressionNode
    ExpressionKind kind;
    union {
        ExpressionIntConstant int_constant;
        ExpressionUnary unary;
        ExpressionBinary binary;
    } as;
} ;

#endif //C_COMPILER_EXPRESSION_H
