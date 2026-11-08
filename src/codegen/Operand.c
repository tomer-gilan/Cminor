//
// Created by tomer on 19-Oct-26.
//

#include "Operand.h"

#include <assert.h>
#include <stddef.h>

// Label:

Label label_make_auto(const int auto_id, const TextView name) {
    return (Label){
        .kind = LABEL_KIND_AUTO,
        .as.label_auto = {
            .auto_id = auto_id,
            .name = name
        }
    };
}
Label label_make_symbol(const Context* const context, const int identifier_id) {
    assert(context != NULL);
    assert(0 <= identifier_id && identifier_id < list_get_size(context->identifiers_table));

    return (Label){
        .kind = LABEL_KIND_SYMBOL,
        .as.label_symbol = {
            .identifier_id = identifier_id,
            .spelling = *(const TextView*)list_at_const(context->identifiers_table, identifier_id)
        }
    };
}

/**
 * @attention gives up ownership of returned @c Text*
 */
Text* label_get_spelling(const Label this) {
    Text* to_return = text_create_empty();
    switch (this.kind) {
        case LABEL_KIND_AUTO:
            text_append_text_view(to_return, text_view_value_of_literal(".L_"));
            text_append_int(to_return, this.as.label_auto.auto_id);
            text_append_char(to_return, '_');
            text_append_text_view(to_return, this.as.label_auto.name);
            break;
        case LABEL_KIND_SYMBOL:
            text_append_text_view(to_return, this.as.label_symbol.spelling);
            break;
        case LABEL_KIND__COUNT:
        default:
            assert(false && "invalid program state");
    }
    return to_return;
}

// Displacement:

Displacement displacement_make_number(const int number) {
    return (Displacement){
        .kind = DISPLACEMENT_KIND_NUMBER,
        .as.number = number
    };
}
Displacement displacement_make_label(const Label label) {
    return (Displacement){
        .kind = DISPLACEMENT_KIND_LABEL,
        .as.label = label
    };
}

// Register:

Register register_make(const RegisterKind kind, const Width width) {
    assert(0 <= kind && kind < REGISTER_KIND__COUNT);

    return (Register){
        .kind = kind,
        .width = width
    };
}

// EffectiveAddress:

EffectiveAddress effective_address_make_sib(const Width scale, const Register index, const Register base, const Displacement displacement) {
    assert(index.kind != REGISTER_KIND_RSP && index.kind != REGISTER_KIND_RIP);
    assert(base.kind != REGISTER_KIND_RIP && base.kind != REGISTER_KIND_NONE); // such that only disp(%base, %index, scale) and disp(%base) forms are allowed.

    return (EffectiveAddress){
        .kind = EFFECTIVE_ADDRESS_KIND_SIB,
        .as.sib = {
            .base = base,
            .index = index,
            .scale = scale,
            .displacement = displacement
        }
    };
}
EffectiveAddress effective_address_make_rip_relative(const Displacement displacement) {
    return (EffectiveAddress){
        .kind = EFFECTIVE_ADDRESS_KIND_RIP_RELATIVE,
        .as.rip_relative = {
            .displacement = displacement,
            .rip = register_make(REGISTER_KIND_RIP, WIDTH_QUAD)
        }
    };
}

// Operand:

Operand operand_make_register(const Register register_) {
    return (Operand){
        .kind = OPERAND_KIND_REGISTER,
        .as.register_ = register_
    };
}
Operand operand_make_memory(const EffectiveAddress effective_address) {
    return (Operand){
        .kind = OPERAND_KIND_MEMORY,
        .as.memory = effective_address
    };
}
Operand operand_make_immediate(const int immediate) {
    return (Operand){
        .kind = OPERAND_KIND_IMMEDIATE,
        .as.immediate = immediate
    };
}
