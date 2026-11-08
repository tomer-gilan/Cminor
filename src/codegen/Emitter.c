//
// Created by tomer on 19-Oct-26.
//

#include <assert.h>
#include <stdlib.h>

#include "Emitter.h"

#include "Printer.h"
#include "Text.h"
#include "TextView.h"
#include "Utils.h"

struct Emitter {
    int label_counter;
    Printer* printer;
}; ///< append only, assembly stream (for instructions, labels, etc.)

static void emitter_emit_sections_headers(Emitter* this);
Emitter* emitter_create(void) {
    Emitter* to_return = utils_xmalloc(sizeof (Emitter));
    *to_return = (Emitter){
        .label_counter = 0,
        .printer = printer_create()
    };

    emitter_emit_sections_headers(to_return);
    return to_return;
}
void emitter_destroy(                                   Emitter* const this) {
    assert(this != NULL);

    printer_destroy(this->printer);
    free(this);
}

static void emitter_emit_separation(                    Emitter* const this) {
    assert(this != NULL);

    printer_print_char(this->printer, ',');
    printer_print_char(this->printer, ' ');
}
static void emitter_emit_instruction_suffix(            Emitter* const this, const Width width) {
    assert(this != NULL);

    switch (width) {
        case WIDTH_BYTE:
            printer_print_char(this->printer, 'b');
            break;
        case WIDTH_LONG:
            printer_print_char(this->printer, 'l');
            break;
        case WIDTH_QUAD:
            printer_print_char(this->printer, 'q');
            break;
        default:
            assert(false && "invalid program state");
    }
}
static void emitter_emit_displacement(                  Emitter* const this, const Displacement displacement) {
    assert(this != NULL);

    switch (displacement.kind) {
        case DISPLACEMENT_KIND_LABEL: {
            Text* const label_spelling = label_get_spelling(displacement.as.label);
            printer_print_text(this->printer, label_spelling);
            text_destroy(label_spelling);
            break;
        }
        case DISPLACEMENT_KIND_NUMBER:
            printer_print_int(this->printer, displacement.as.number);
            break;
        case DISPLACEMENT_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
}
static void emitter_emit_register_name(                 Emitter* const this, const RegisterKind kind) {
    assert(this != NULL);

    switch (kind) {
        case REGISTER_KIND_RAX:
            printer_print_char(this->printer, 'a');
            break;
        case REGISTER_KIND_RBX:
            printer_print_char(this->printer, 'b');
            break;
        case REGISTER_KIND_RCX:
            printer_print_char(this->printer, 'c');
            break;
        case REGISTER_KIND_RDX:
            printer_print_char(this->printer, 'd');
            break;

        case REGISTER_KIND_RBP:
            printer_print_text_view(this->printer, text_view_value_of_literal("bp"));
            break;
        case REGISTER_KIND_RSI:
            printer_print_text_view(this->printer, text_view_value_of_literal("si"));
            break;
        case REGISTER_KIND_RDI:
            printer_print_text_view(this->printer, text_view_value_of_literal("di"));
            break;
        case REGISTER_KIND_RSP:
            printer_print_text_view(this->printer, text_view_value_of_literal("sp"));
            break;
        case REGISTER_KIND_RIP:
            printer_print_text_view(this->printer, text_view_value_of_literal("ip"));
            break;

        case REGISTER_KIND_R8:
            printer_print_text_view(this->printer, text_view_value_of_literal("r8"));
            break;
        case REGISTER_KIND_R9:
            printer_print_text_view(this->printer, text_view_value_of_literal("r9"));
            break;
        case REGISTER_KIND_R10:
            printer_print_text_view(this->printer, text_view_value_of_literal("r10"));
            break;
        case REGISTER_KIND_R11:
            printer_print_text_view(this->printer, text_view_value_of_literal("r11"));
            break;
        case REGISTER_KIND_R12:
            printer_print_text_view(this->printer, text_view_value_of_literal("r12"));
            break;
        case REGISTER_KIND_R13:
            printer_print_text_view(this->printer, text_view_value_of_literal("r13"));
            break;
        case REGISTER_KIND_R14:
            printer_print_text_view(this->printer, text_view_value_of_literal("r14"));
            break;
        case REGISTER_KIND_R15:
            printer_print_text_view(this->printer, text_view_value_of_literal("r15"));
            break;
        case REGISTER_KIND_NONE:
        case REGISTER_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
}
static void emitter_emit_operand_register(              Emitter* const this, const Register register_) {
    assert(this != NULL);

    printer_print_char(this->printer, '%');
    switch (register_.width) {
        case WIDTH_BYTE:
            emitter_emit_register_name(this, register_.kind);
            switch (register_.kind) {
                case REGISTER_KIND_RAX:
                case REGISTER_KIND_RBX:
                case REGISTER_KIND_RCX:
                case REGISTER_KIND_RDX:
                case REGISTER_KIND_RBP:
                case REGISTER_KIND_RSI:
                case REGISTER_KIND_RDI:
                case REGISTER_KIND_RSP:
                    printer_print_char(this->printer, 'l');
                    break;
                case REGISTER_KIND_R8:
                case REGISTER_KIND_R9:
                case REGISTER_KIND_R10:
                case REGISTER_KIND_R11:
                case REGISTER_KIND_R12:
                case REGISTER_KIND_R13:
                case REGISTER_KIND_R14:
                case REGISTER_KIND_R15:
                    printer_print_char(this->printer, 'b');
                    break;
                case REGISTER_KIND_RIP:
                case REGISTER_KIND_NONE:
                case REGISTER_KIND__COUNT:
                default:
                    assert(false && "invalid program state");
            }
            break;
        case WIDTH_LONG:
            switch (register_.kind) {
                case REGISTER_KIND_RAX:
                case REGISTER_KIND_RBX:
                case REGISTER_KIND_RCX:
                case REGISTER_KIND_RDX:
                    printer_print_char(this->printer, 'e');
                    emitter_emit_register_name(this, register_.kind);
                    printer_print_char(this->printer, 'x');
                    break;
                case REGISTER_KIND_RBP:
                case REGISTER_KIND_RSI:
                case REGISTER_KIND_RDI:
                case REGISTER_KIND_RSP:
                    printer_print_char(this->printer, 'e');
                    emitter_emit_register_name(this, register_.kind);
                    break;
                case REGISTER_KIND_R8:
                case REGISTER_KIND_R9:
                case REGISTER_KIND_R10:
                case REGISTER_KIND_R11:
                case REGISTER_KIND_R12:
                case REGISTER_KIND_R13:
                case REGISTER_KIND_R14:
                case REGISTER_KIND_R15:
                    emitter_emit_register_name(this, register_.kind);
                    printer_print_char(this->printer, 'd');
                    break;
                case REGISTER_KIND_RIP:
                case REGISTER_KIND_NONE:
                case REGISTER_KIND__COUNT:
                default:
                    assert(false && "invalid program state");
            }
            break;
        case WIDTH_QUAD:
            switch (register_.kind) {
                case REGISTER_KIND_RAX:
                case REGISTER_KIND_RBX:
                case REGISTER_KIND_RCX:
                case REGISTER_KIND_RDX:
                    printer_print_char(this->printer, 'r');
                    emitter_emit_register_name(this, register_.kind);
                    printer_print_char(this->printer, 'x');
                    break;
                case REGISTER_KIND_RBP:
                case REGISTER_KIND_RSI:
                case REGISTER_KIND_RDI:
                case REGISTER_KIND_RSP:
                case REGISTER_KIND_RIP:
                    printer_print_char(this->printer, 'r');
                    emitter_emit_register_name(this, register_.kind);
                    break;
                case REGISTER_KIND_R8:
                case REGISTER_KIND_R9:
                case REGISTER_KIND_R10:
                case REGISTER_KIND_R11:
                case REGISTER_KIND_R12:
                case REGISTER_KIND_R13:
                case REGISTER_KIND_R14:
                case REGISTER_KIND_R15:
                    emitter_emit_register_name(this, register_.kind);
                    break;
                case REGISTER_KIND_NONE:
                case REGISTER_KIND__COUNT:
                default:
                    assert(false && "invalid program state");
            }
            break;
        default:
            assert(false && "invalid program state");
            break;
    }
}
static void emitter_emit_operand_memory_sib(            Emitter* const this, const EffectiveAddressSib effective_address_sib) {
    assert(this != NULL);

    emitter_emit_displacement(this, effective_address_sib.displacement);
    printer_print_char(this->printer, '(');
    // Base
    emitter_emit_operand_register(this, effective_address_sib.base);
    if (effective_address_sib.index.kind != REGISTER_KIND_NONE) {
        emitter_emit_separation(this);
        // Index
        emitter_emit_operand_register(this, effective_address_sib.index);
        emitter_emit_separation(this);
        // Scale
        printer_print_int(this->printer, effective_address_sib.scale); // no leading '$', since it is not an immediate.
    }
    printer_print_char(this->printer, ')');
}
static void emitter_emit_operand_memory_rip_relative(   Emitter* const this, const EffectiveAddressRipRelative effective_address_rip_relative) {
    assert(this != NULL);

    emitter_emit_displacement(this, effective_address_rip_relative.displacement); // no leading '$', since it is not an immediate.
    printer_print_char(this->printer, '(');
    emitter_emit_operand_register(this, effective_address_rip_relative.rip);
    printer_print_char(this->printer, ')');
}
static void emitter_emit_operand_memory(                Emitter* const this, const EffectiveAddress effective_address) {
    assert(this != NULL);

    switch (effective_address.kind) {
        case EFFECTIVE_ADDRESS_KIND_SIB:
            emitter_emit_operand_memory_sib(this, effective_address.as.sib);
            break;
        case EFFECTIVE_ADDRESS_KIND_RIP_RELATIVE:
            emitter_emit_operand_memory_rip_relative(this, effective_address.as.rip_relative);
            break;
        case EFFECTIVE_ADDRESS_KIND__COUNT:
        default:
            assert(false && "invalid program state");
            break;
    }
}
static void emitter_emit_operand_immediate(             Emitter* const this, const int immediate) {
    assert(this != NULL);

    printer_print_char(this->printer, '$');
    printer_print_int(this->printer, immediate);
}
static void emitter_emit_operand(                       Emitter* const this, const Operand operand) {
    assert(this != NULL);

    switch (operand.kind) {
        case OPERAND_KIND_REGISTER:
            emitter_emit_operand_register(this, operand.as.register_);
            break;
        case OPERAND_KIND_MEMORY:
            emitter_emit_operand_memory(this, operand.as.memory);
            break;
        case OPERAND_KIND_IMMEDIATE:
            emitter_emit_operand_immediate(this, operand.as.immediate);
            break;
        case OPERAND_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
}
static void emitter_emit_sections_headers(              Emitter* const this) {
    assert(this != NULL);

    printer_print_text_view(this->printer, text_view_value_of_literal(".text"));
    printer_print_new_line(this->printer);
}
static void emitter_emit_footers(                       Emitter* const this) {
    assert(this != NULL);

    printer_print_new_line(this->printer);
    printer_print_text_view(this->printer, text_view_value_of_literal(".section .note.GNU-stack,\"\",@progbits"));
    printer_print_new_line(this->printer);
}

void emitter_indent(Emitter* const this) {
    assert(this != NULL);

    printer_indent(this->printer);
}
void emitter_dedent(Emitter* const this) {
    assert(this != NULL);

    printer_dedent(this->printer);
}
Text* emitter_detach_text(Emitter* const this) {
    assert(this != NULL);

    emitter_emit_footers(this);
    return printer_detach_text(this->printer);
}

void emitter_emit_external_linkage(Emitter* const this, const Label label) {
    assert(this != NULL);

    Text* const spelling = label_get_spelling(label);
    assert(spelling != NULL);

    printer_print_new_line(this->printer);
    printer_print_text_view(this->printer, text_view_value_of_literal(".globl "));
    printer_print_text_view(this->printer, text_as_view(spelling));

    text_destroy(spelling);
}
void emitter_emit_label(Emitter* const this, const Label label) {
    assert(this != NULL);

    printer_print_new_line(this->printer);

    // e.g.: ".L_1_while:"
    Text* const spelling = label_get_spelling(label);
    assert(spelling != NULL);

    printer_print_text(this->printer, spelling);
    printer_print_char(this->printer, ':');

    text_destroy(spelling);
}

/**
 * @note: look at the source code. The builtin Doxygen screws the table formats.
 *
 * A quick reference to the different operands combinations:
 * s\d| M | R | I
 * ---+---+---+---
 *  M | X | * | X
 * ---+---+---+---
 *  R | * | * | X
 * ---+---+---+---
 *  I | **| * | X
 *
 *  M: memory operand; R: register operand; I: immediate operand.
 *  X: invalid combination
 *  *: provided With-s must match the registers'.
 *  **: uses the provided width as-is.
 *
 *  the various asserts will have numbers next to them. That corresponds to the operand-combination-options:
 * s\d| M | R | I
 * ---+---+---+---
 *  M | 1 | 2 | 3
 * ---+---+---+---
 *  R | 4 | 5 | 6
 * ---+---+---+---
 *  I | 7 | 8 | 9
 */
static void assert_operands_validness(Width width, const Operand source, const Operand destination) {
    assert(destination.kind != OPERAND_KIND_IMMEDIATE);                                             // #3, #6, #9
    assert(!(source.kind == OPERAND_KIND_MEMORY && destination.kind == OPERAND_KIND_MEMORY));       // #1
    assert(source.kind      != OPERAND_KIND_REGISTER || source.as.register_.width       == width);  // #4, #5
    assert(destination.kind != OPERAND_KIND_REGISTER || destination.as.register_.width  == width);  // #2, #5, #8
}
static void emitter_emit_zero_operand_instruction(Emitter* const this, const TextView instruction) {
    assert(this != NULL);

    printer_print_new_line(this->printer);

    printer_print_text_view(this->printer, instruction);
}
static void emitter_emit_one_operand_instruction(Emitter* const this, const TextView instruction, const Width width, const Operand only) {
    assert(this != NULL);

    printer_print_new_line(this->printer);

    printer_print_text_view(this->printer, instruction);
    emitter_emit_instruction_suffix(this, width);
    printer_print_char(this->printer, ' ');

    emitter_emit_operand(this, only);
}
static void emitter_emit_two_operand_instruction(Emitter* const this, const TextView instruction, const Width instruction_width, const Operand source, const Operand destination) {
    assert(this != NULL);

    printer_print_new_line(this->printer);

    printer_print_text_view(this->printer, instruction);
    emitter_emit_instruction_suffix(this, instruction_width);
    printer_print_char(this->printer, ' ');

    emitter_emit_operand(this, source);
    emitter_emit_separation(this);
    emitter_emit_operand(this, destination);
}

void emitter_emit_mov(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);
    assert_operands_validness(width, source, destination);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("mov"),
        width,
        source,
        destination);
}
void emitter_emit_ret(Emitter* const this) {
    assert(this != NULL);

    emitter_emit_zero_operand_instruction(this, text_view_value_of_literal("ret"));
}
void emitter_emit_neg(Emitter* const this, const Width width, const Operand only) {
    assert(this != NULL);

    assert( (only.kind == OPERAND_KIND_MEMORY) ||
            (only.kind == OPERAND_KIND_REGISTER && only.as.register_.width == width));

    emitter_emit_one_operand_instruction(
        this,
        text_view_value_of_literal("neg"),
        width,
        only);
}
void emitter_emit_not(Emitter* const this, const Width width, const Operand only) {
    assert(this != NULL);

    assert( (only.kind == OPERAND_KIND_MEMORY) ||
            (only.kind == OPERAND_KIND_REGISTER && only.as.register_.width == width));

    emitter_emit_one_operand_instruction(
        this,
        text_view_value_of_literal("not"),
        width,
        only);
}
void emitter_emit_cmp(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);
    assert_operands_validness(width, source, destination);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("cmp"),
        width,
        source,
        destination);
}
void emitter_emit_sete(Emitter* const this, const Operand only) {
    assert(this != NULL);

    const Width width = WIDTH_BYTE;
    assert((only.kind == OPERAND_KIND_REGISTER && only.as.register_.width == width) ||
        only.kind == OPERAND_KIND_MEMORY);

    emitter_emit_one_operand_instruction(
        this,
        text_view_value_of_literal("sete"),
        width,
        only);
}

/**
 * @note: look at the source code. The builtin Doxygen screws the table formats.
 *
 * A quick reference to the different operands combinations:
 * s\d| M | R | I
 * ---+---+---+---
 *  M | X | * | X
 * ---+---+---+---
 *  R | * | * | X
 * ---+---+---+---
 *  I | **| * | X
 *
 *  M: memory operand; R: register operand; I: immediate operand.
 *  X: invalid combination
 *  *: provided With-s must match the registers'.
 *  **: uses the provided width as-is.
 *
 *  the various asserts will have numbers next to them. That corresponds to the operand-combination-options:
 * s\d| M | R | I
 * ---+---+---+---
 *  M | 1 | 2 | 3
 * ---+---+---+---
 *  R | 4 | 5 | 6
 * ---+---+---+---
 *  I | 7 | 8 | 9
 */
void emitter_emit_add(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);
    assert_operands_validness(width, source, destination);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("add"),
        width,
        source,
        destination);
}
void emitter_emit_sub(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);
    assert_operands_validness(width, source, destination);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("sub"),
        width,
        source,
        destination);
}
/**
 * imul takes a source which must be a register, and a destination which cannot be immediate.
 * It then performs in-place multiplication.
 */
void emitter_emit_imul(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);

    assert(destination.kind == OPERAND_KIND_REGISTER && destination.as.register_.width == width);
    assert(source.kind != OPERAND_KIND_IMMEDIATE);
    if (source.kind == OPERAND_KIND_REGISTER)
        assert(source.as.register_.width == width);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("imul"),
        width,
        source,
        destination);
}
/**
 * sign extends %eax -> %edx:%eax
 */
void emitter_emit_cltd(Emitter* const this) {
    assert(this != NULL);

    emitter_emit_zero_operand_instruction(this, text_view_value_of_literal("cltd"));
}
/**
 * unlike @c add and @c sub, @c idiv already expects:
 * 0. @c divisor to be a register operand of WIDTH_LONG @c Width.
 * 1. the @b divided @b value to be in @b %eax
 * 2. the @b remainder is overwritten to @b %edx.
 *
 * It then computes (in python syntax):
 *
 * (@b %eax, @b %edx) @b = @b divmod( @b %eax, @b divisor).
 *
 * This is a result of x86_64 architecture design choices.
 */
void emitter_emit_idiv(Emitter* const this, const Operand divisor) {
    assert(this != NULL);

    const Width width = WIDTH_LONG;
    assert(divisor.kind == OPERAND_KIND_REGISTER && divisor.as.register_.width == width);

    emitter_emit_one_operand_instruction(
        this,
        text_view_value_of_literal("idiv"),
        width,
        divisor);
}

static void emitter_emit_shift_instruction(
    Emitter* const this,
    const TextView instruction,
    const Width width,
    const Operand source,
    const Operand destination) {

    assert(this != NULL);
    assert(source.kind != OPERAND_KIND_MEMORY);
    if (source.kind == OPERAND_KIND_REGISTER)
        assert(source.as.register_.width == WIDTH_BYTE && source.as.register_.kind == REGISTER_KIND_RCX); // only %cl is supported.

    emitter_emit_two_operand_instruction(
        this,
        instruction,
        width,
        source,
        destination);
}

void emitter_emit_sal(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);

    emitter_emit_shift_instruction(this, text_view_value_of_literal("sal"), width, source, destination);
}
void emitter_emit_sar(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);

    emitter_emit_shift_instruction(this, text_view_value_of_literal("sar"), width, source, destination);
}

void emitter_emit_and(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);
    assert_operands_validness(width, source, destination);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("and"),
        width,
        source,
        destination);
}
void emitter_emit_xor(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);
    assert_operands_validness(width, source, destination);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("xor"),
        width,
        source,
        destination);
}
void emitter_emit_or(Emitter* const this, const Width width, const Operand source, const Operand destination) {
    assert(this != NULL);
    assert_operands_validness(width, source, destination);

    emitter_emit_two_operand_instruction(
        this,
        text_view_value_of_literal("or"),
        width,
        source,
        destination);
}

/**
 * @attention the operand's possible @c Width is ignored.
 */
void emitter_emit_push(Emitter* const this, Operand source) {
    assert(this != NULL);

    const Width width = WIDTH_QUAD;
    if (source.kind == OPERAND_KIND_REGISTER)
        source = operand_make_register(register_make(source.as.register_.kind, width));

    emitter_emit_one_operand_instruction(
        this,
        text_view_value_of_literal("push"),
        width,
        source);
}
void emitter_emit_pop(Emitter* const this, Operand destination) {
    assert(this != NULL);

    assert(destination.kind != OPERAND_KIND_IMMEDIATE);

    const Width width = WIDTH_QUAD;
    if (destination.kind == OPERAND_KIND_REGISTER)
        destination = operand_make_register(register_make(destination.as.register_.kind, width));

    emitter_emit_one_operand_instruction(
        this,
        text_view_value_of_literal("pop"),
        width,
        destination);
}