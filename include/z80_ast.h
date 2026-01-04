#ifndef Z80AST_H
#define Z80AST_H

#include <stdint.h>

typedef enum {
    I_UNKNOWN = 0,
    I_LD_R_R,
    I_LD_R_NNNN, /* r = nnnn */
    I_LD_R_MEMI, /* r = (nnnn) */
    I_LD_R_MEMR, /* r = (reg) */
    I_LD_R_MEMR_OFF, /* r = (reg + off) */
    I_ADD_A,
    I_ADC_A,
    I_SUB_A,
    I_SBC_A,
    I_ADD_HL,
    I_ADC_HL,
    I_SUB_HL,
    I_SBC_HL,
    I_INC8,
    I_INC16,
    I_DEC8,
    I_DEC16,
    I_AND,
    I_OR,
    I_XOR,
    I_CP,
    I_CPL,
    I_EX_DE_HL,
    I_RRA,
    I_JR,
    I_JP,
    I_JPCond,
    I_CALL,
    I_CALLCond,
    I_RET,
    I_RETCond,
    I_DJNZ,
    I_PUSH,
    I_POP,
    I_LABEL,
    I_DM,
    I_DB,
    I_DW,
    I_DS,
} z80_op_t;

typedef enum {
    REG_A = 'a',
    REG_F = 'f',
    REG_B = 'b',
    REG_C = 'c',
    REG_D = 'd',
    REG_E = 'e',
    REG_H = 'h',
    REG_L = 'l',
} z80_reg_t;

typedef struct {
    z80_op_t op;
    union {
        struct {
            z80_reg_t l;
        } r8;
        struct {
            z80_reg_t l;
            z80_reg_t r;
        } r16;
        struct {
            uint8_t l;
            uint8_t r;
        } a8_8; /* Two 8-bit values */
        struct {
            uint8_t l;
            uint16_t r;
        } a8_16;
        struct {
            uint16_t l;
            uint8_t r;
        } a16_a8;
    } args;
} z80_instr_t;

#endif