#ifndef Z80AST_H
#define Z80AST_H

#include <stdint.h>

/*
 * Compact Z80 instruction representation.
 *
 * z80_instr_t stays 4-6 bytes by using 8-bit op/mode fields and a
 * small union for operands. The enums below describe the legal ranges.
 */

typedef enum {
    I_NOP = 0,
    I_LD,
    I_ADD,
    I_ADC,
    I_SUB,
    I_SBC,
    I_AND,
    I_OR,
    I_XOR,
    I_CP,
    I_INC,
    I_DEC,
    I_CPL,
    I_DAA,
    I_SCF,
    I_CCF,
    I_NEG,
    I_EX,
    I_EXX,
    I_RLCA,
    I_RRCA,
    I_RLA,
    I_RRA,
    I_RLC,
    I_RRC,
    I_RL,
    I_RR,
    I_SLA,
    I_SRA,
    I_SRL,
    I_BIT,
    I_SET,
    I_RES,
    I_JP,
    I_JR,
    I_DJNZ,
    I_CALL,
    I_RET,
    I_RETI,
    I_RETN,
    I_RST,
    I_PUSH,
    I_POP,
    I_IN,
    I_OUT,
    I_IM,
    I_HALT,
    I_DI,
    I_EI,
    I_LDI,
    I_LDIR,
    I_LDD,
    I_LDDR,
    I_CPI,
    I_CPIR,
    I_CPD,
    I_CPDR,
    I_INI,
    I_INIR,
    I_IND,
    I_INDR,
    I_OUTI,
    I_OTIR,
    I_OUTD,
    I_OTDR,
    I_RRD,
    I_RLD,

    /* Pseudo-ops. */
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
    REG_I = 'i',
    REG_R = 'r',

    REG_IXH = 0x80,
    REG_IXL,
    REG_IYH,
    REG_IYL,
} z80_reg8_t;

typedef enum {
    REG_BC = 0,
    REG_DE,
    REG_HL,
    REG_SP,
    REG_IX,
    REG_IY,
    REG_AF,
    REG_AF_ALT,
} z80_reg16_t;

typedef enum {
    IDX_IX = 0,
    IDX_IY,
} z80_idx_t;

typedef enum {
    COND_NZ = 0,
    COND_Z,
    COND_NC,
    COND_C,
    COND_PO,
    COND_PE,
    COND_P,
    COND_M,
} z80_cond_t;

typedef enum {
    MEM_BC = 0,
    MEM_DE,
    MEM_HL,
    MEM_SP,
    MEM_NN,
    MEM_C,
} z80_mem_t;

typedef enum {
    Z80_AM_NONE = 0,
    Z80_AM_R8,
    Z80_AM_R16,
    Z80_AM_R8_R8,
    Z80_AM_R16_R16,
    Z80_AM_R8_N,
    Z80_AM_R16_NN,
    Z80_AM_R8_MEM,
    Z80_AM_MEM_R8,
    Z80_AM_MEM_R16,
    Z80_AM_R8_MEMI,
    Z80_AM_MEMI_R8,
    Z80_AM_R8_MEMO,
    Z80_AM_MEMO_R8,
    Z80_AM_BIT_R8,
    Z80_AM_BIT_MEMO,
    Z80_AM_COND_REL,
    Z80_AM_COND_ABS,
    Z80_AM_REL,
    Z80_AM_ABS,
    Z80_AM_RST,
    Z80_AM_IM,
} z80_addr_mode_t;

typedef struct {
    uint8_t op;   /* z80_op_t */
    uint8_t mode; /* z80_addr_mode_t */
    union {
        struct { uint8_t r; } r8;
        struct { uint8_t rr; } r16;
        struct { uint8_t dst; uint8_t src; } r8_r8;
        struct { uint8_t dst; uint8_t src; } r16_r16;
        struct { uint8_t r; uint8_t imm; } r8_n;
        struct { uint8_t rr; uint16_t imm; } r16_nn;
        struct { uint8_t r; uint8_t mem; } r8_mem;
        struct { uint8_t mem; uint8_t r; } mem_r8;
        struct { uint8_t mem; uint8_t rr; } mem_r16;
        struct { uint8_t r; uint16_t addr; } r8_memi;
        struct { uint16_t addr; uint8_t r; } memi_r;
        struct { uint8_t r; uint8_t idx; int8_t disp; } r8_memo;
        struct { uint8_t idx; int8_t disp; uint8_t r; } memo_r8;
        struct { uint8_t bit; uint8_t r; } bit_r8;
        struct { uint8_t bit; uint8_t idx; int8_t disp; } bit_memo;
        struct { uint8_t cond; int8_t disp; } cond_rel;
        struct { uint8_t cond; uint16_t addr; } cond_abs;
        struct { uint16_t addr; } abs;
        struct { int8_t disp; } rel;
        struct { uint8_t vec; } rst;
        struct { uint8_t mode; } im;
    } args;
} z80_instr_t;

#endif
