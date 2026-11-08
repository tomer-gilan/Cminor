//
// Created by tomer on 19-Oct-26.
//

#ifndef C_COMPILER_EMITTER_H
#define C_COMPILER_EMITTER_H
#include "Operand.h"
#include "Text.h"
#include "TextView.h"

typedef struct Emitter Emitter;

Emitter* emitter_create(void);
void emitter_destroy(Emitter* this);

void emitter_indent(Emitter* this);
void emitter_dedent(Emitter* this);
/**
 * @attention gives ownership of the assembly @c Text .
 */
Text* emitter_detach_text(Emitter* this);

void emitter_emit_external_linkage(Emitter* this, Label label);
void emitter_emit_label(Emitter* this, Label label);

// instruction emissions:

void emitter_emit_mov(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_ret(Emitter* this);
void emitter_emit_neg(Emitter* this, Width width, Operand only);
void emitter_emit_not(Emitter* this, Width width, Operand only);
void emitter_emit_cmp(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_sete(Emitter* this, Operand only);

//      arithmetic:

void emitter_emit_add(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_sub(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_imul(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_cltd(Emitter* this);
void emitter_emit_idiv(Emitter* this, Operand divisor);

//      bitwise & shift:
void emitter_emit_sal(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_sar(Emitter* this, Width width, Operand source, Operand destination);

void emitter_emit_and(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_xor(Emitter* this, Width width, Operand source, Operand destination);
void emitter_emit_or(Emitter* this, Width width, Operand source, Operand destination);

//      stack:
void emitter_emit_push(Emitter* this, Operand source);
void emitter_emit_pop(Emitter* this, Operand destination);


#endif //C_COMPILER_EMITTER_H
