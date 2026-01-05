#ifndef Z80AST_H
#define Z80AST_H

#include <stdint.h>

/*
 * Compact Z80 instruction representation.
 *
 * z80_instr_t stays 4-6 bytes by using 8-bit op/mode fields and a
 * small union for operands. The enums below describe the legal ranges.
 */
#if __linux____ || __APPLE__ || __GNUC__ || __clang__
#define Z80_ATTR    static
#else
#define Z80_ATTR    static const
#endif

#define Z80_WRITE(struct_addr)  ((z80_instr_t*) (struct_addr))

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
    COND_NONE,
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
    M_NONE = 0,
    M_R_R,
    M_RR_RR,
    M_R_N,
    M_RR_NN,
    M_R_MEM,
    M_MEM_R,
    M_MEM_RR,
    M_R_MEMI,
    M_MEMI_R,
    M_R_MEMO,
    M_MEMO_R,
    M_BIT_R,
    M_BIT_MEMO,
    M_R_LABEL,
    M_LABEL_R,
    M_RR_LABEL,
    M_LABEL_RR,
    M_RR_MEM_LABEL,
    M_LABEL,
    M_COND_LABEL,
    M_COND_REL,
    M_COND_ABS,
    M_REL,
    M_ABS,
    M_RST,
    M_IM,
} z80_addr_mode_t;

typedef struct {
    uint8_t op;   /* z80_op_t */
    uint8_t mode; /* z80_addr_mode_t */
    union {
        struct { uint8_t dst; uint8_t src; } r_r;
        struct { uint8_t dst; uint8_t src; } rr_rr;
        struct { uint8_t r; uint8_t imm; } r_n;
        struct { uint8_t rr; uint16_t imm; } rr_nn;
        struct { uint8_t r; uint8_t mem; } r_mem;
        struct { uint8_t mem; uint8_t r; } mem_r;
        struct { uint8_t mem; uint8_t rr; } mem_rr;
        struct { uint8_t r; uint16_t addr; } r_memi;
        struct { uint16_t addr; uint8_t r; } memi_r;
        struct { uint8_t r; uint8_t idx; int8_t disp; } r_memo;
        struct { uint8_t idx; int8_t disp; uint8_t r; } memo_r;
        struct { uint8_t bit; uint8_t r; } bit_r;
        struct { uint8_t bit; uint8_t idx; int8_t disp; } bit_memo;
        struct { uint8_t r; const char* label; } r_label;
        struct { const char* label; uint8_t r; } label_r;
        struct { uint8_t rr; const char* label; } rr_label;
        struct { const char* label; uint8_t rr; } label_rr;
        struct { const char* label; } label;
        struct { uint8_t cond; const char* label; } cond_label;
        struct { uint8_t cond; int8_t disp; } cond_rel;
        struct { uint8_t cond; uint16_t addr; } cond_abs;
        struct { uint16_t addr; } abs;
        struct { int8_t disp; } rel;
        struct { uint8_t vec; } rst;
        struct { uint8_t mode; } im;
    } args;
} z80_instr_t;

/* Common instructions */
extern z80_instr_t z80_push_hl;
extern z80_instr_t z80_pop_hl;
extern z80_instr_t z80_ld_hl_lab;

#endif
