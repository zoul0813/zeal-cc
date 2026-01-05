#include "cc_compat.h"
#include "codegen.h"

#include "common.h"

/* Common instructions */
z80_instr_t z80_push_hl   = { I_PUSH, M_RR_RR,  { .rr_rr = { REG_HL } } };
z80_instr_t z80_pop_hl    = { I_POP, M_RR_RR,   { .rr_rr = { REG_HL } } };
z80_instr_t z80_ld_hl_lab = { I_LD, M_RR_LABEL, { .rr_label = { REG_HL, 0 } } };


static uint8_t pp_ld[]  = "  ld R,R\n";
static uint8_t pp_ld_r_n[] = "  ld R,0x00\n";
static uint8_t pp_ld_rr_nn[] = "  ld RR,0x0000\n";
static uint8_t pp_ld_rr_rr[] = "  ld RR,RR\n";
static uint8_t pp_ld_r_label[] = "  ld R,";
static uint8_t pp_ld_r_mlabel[] = "  ld R,(";
static uint8_t pp_ld_label_r[] = "  ld (";
static uint8_t pp_ld_rr_label[] = "  ld RR,";
static uint8_t pp_ld_rr_mlabel[] = "  ld RR,(";
static uint8_t pp_ld_label_rr[] = "  ld (";
static uint8_t pp_call[] = "  call ";
static uint8_t pp_add[] = "  add a, R\n";
static uint8_t pp_add_hl_rr[] = "  add hl, RR\n";
static uint8_t pp_sub[] = "  sub R\n";
static uint8_t pp_sbc[] = "  sbc a, R\n";
static uint8_t pp_and[] = "  and R\n";
static uint8_t pp_or[]  = "  or R\n";
static uint8_t pp_push[] = "  push RR\n";
static uint8_t pp_pop[] = "  pop RR\n";
static uint8_t pp_ld_r_mem[] = "  ld R,(RR)\n";
static uint8_t pp_ld_mem_r[] = "  ld (RR),R\n";
static uint8_t pp_ld_r_memi[] = "  ld R,(0x0000)\n";
static uint8_t pp_ld_memi_r[] = "  ld (0x0000),R\n";
static uint8_t pp_ld_r_memo[] = "  ld R,(IX+0x00)\n";
static uint8_t pp_ld_memo_r[] = "  ld (IX+0x00),R\n";
static uint8_t pp_cpl[] = "  cpl\n";
static uint8_t pp_neg[] = "  neg\n";
static uint8_t pp_rra[] = "  rra\n";
static uint8_t pp_ret[] = "  ret\n";
static uint8_t pp_djnz[] = "  djnz ";
static uint8_t pp_inc16[] = "  inc RR\n";
static uint8_t pp_dec16[] = "  dec RR\n";

static const char* const z80_cond_strings[] = {
    "nz",
    "z",
    "nc",
    "c",
    "po",
    "pe",
    "p",
    "m",
};

static const char* const z80_rr_strings[] = {
    "bc",
    "de",
    "hl",
    "sp",
    "ix",
    "iy",
    "af",
    "??",
};

static const uint8_t z80_mem_rr[] = {
    REG_BC,
    REG_DE,
    REG_HL,
    REG_SP,
};

static void z80_write_rr(uint8_t* out, uint8_t rr) {
    if (rr < (uint8_t)DIM(z80_rr_strings)) {
        const char* name = z80_rr_strings[rr];
        out[0] = (uint8_t)name[0];
        out[1] = (uint8_t)name[1];
        return;
    }
    out[0] = '?';
    out[1] = '?';
}

static bool z80_write_mem_rr(uint8_t* out, uint8_t mem) {
    if (mem < (uint8_t)DIM(z80_mem_rr)) {
        z80_write_rr(out, z80_mem_rr[mem]);
        return true;
    }
    return false;
}

static void z80_write_hex8(uint8_t* out, uint8_t value) {
    static const char hex[] = "0123456789abcdef";
    out[0] = hex[(value >> 4) & 0xF];
    out[1] = hex[value & 0xF];
}

static void z80_write_hex16(uint8_t* out, uint16_t value) {
    static const char hex[] = "0123456789abcdef";
    out[0] = hex[(value >> 12) & 0xF];
    out[1] = hex[(value >> 8) & 0xF];
    out[2] = hex[(value >> 4) & 0xF];
    out[3] = hex[value & 0xF];
}

static void z80_write_idx_disp(uint8_t* out, uint8_t idx, int8_t disp) {
    out[0] = 'i';
    out[1] = (idx == IDX_IY) ? 'y' : 'x';
    out[2] = '+';
    out[3] = '0';
    out[4] = 'x';
    z80_write_hex8(out + 5, (uint8_t)disp);
}

static void z80_emit_reg8(uint8_t r) {
    char buf[2];
    buf[0] = (char)r;
    buf[1] = '\0';
    codegen_emit(buf);
}

static void z80_emit_cond(uint8_t cond) {
    if (cond < (uint8_t)DIM(z80_cond_strings)) {
        codegen_emit(z80_cond_strings[cond]);
        return;
    }
    codegen_emit("?");
}

static uint8_t z80_format_ex_operand(char* out, uint8_t rr, bool mem_sp) {
    if (mem_sp) {
        out[0] = '(';
        out[1] = 's';
        out[2] = 'p';
        out[3] = ')';
        out[4] = '\0';
        return 4;
    }
    if (rr == REG_AF) {
        out[0] = 'a';
        out[1] = 'f';
        out[2] = '\0';
        return 2;
    }
    if (rr == REG_AF_ALT) {
        out[0] = 'a';
        out[1] = 'f';
        out[2] = '\'';
        out[3] = '\0';
        return 3;
    }

    z80_write_rr((uint8_t*)out, rr);
    out[2] = '\0';
    return 2;
}

void codegen_emit_z80(uint8_t len, const z80_instr_t* instrs)
{
    uint8_t* str = NULL;

    while (len--) {
        z80_op_t op = (z80_op_t)instrs->op;
        z80_addr_mode_t mode = (z80_addr_mode_t)instrs->mode;
        switch (op) {
            case I_LD:
                if (mode == M_R_R) {
                    pp_ld[5] = instrs->args.r_r.dst;
                    pp_ld[7] = instrs->args.r_r.src;
                    str = pp_ld;
                } else if (mode == M_R_N) {
                    pp_ld_r_n[5] = instrs->args.r_n.r;
                    z80_write_hex8(&pp_ld_r_n[9], instrs->args.r_n.imm);
                    str = pp_ld_r_n;
                } else if (mode == M_RR_NN) {
                    z80_write_rr(&pp_ld_rr_nn[5], instrs->args.rr_nn.rr);
                    z80_write_hex16(&pp_ld_rr_nn[10], instrs->args.rr_nn.imm);
                    str = pp_ld_rr_nn;
                } else if (mode == M_RR_RR) {
                    z80_write_rr(&pp_ld_rr_rr[5], instrs->args.rr_rr.dst);
                    z80_write_rr(&pp_ld_rr_rr[8], instrs->args.rr_rr.src);
                    str = pp_ld_rr_rr;
                } else if (mode == M_R_LABEL) {
                    pp_ld_r_mlabel[5] = instrs->args.r_label.r;
                    codegen_emit((const char*)pp_ld_r_mlabel);
                    codegen_emit(instrs->args.r_label.label);
                    codegen_emit(")\n");
                    str = NULL;
                } else if (mode == M_LABEL_R) {
                    codegen_emit((const char*)pp_ld_label_r);
                    codegen_emit(instrs->args.label_r.label);
                    codegen_emit("), ");
                    z80_emit_reg8(instrs->args.label_r.r);
                    codegen_emit("\n");
                    str = NULL;
                } else if (mode == M_RR_LABEL) {
                    z80_write_rr(&pp_ld_rr_label[5], instrs->args.rr_label.rr);
                    codegen_emit((const char*)pp_ld_rr_label);
                    codegen_emit(instrs->args.rr_label.label);
                    codegen_emit("\n");
                    str = NULL;
                } else if (mode == M_RR_MEM_LABEL) {
                    z80_write_rr(&pp_ld_rr_mlabel[5], instrs->args.rr_label.rr);
                    codegen_emit((const char*)pp_ld_rr_mlabel);
                    codegen_emit(instrs->args.rr_label.label);
                    codegen_emit(")\n");
                    str = NULL;
                } else if (mode == M_LABEL_RR) {
                    codegen_emit((const char*)pp_ld_label_rr);
                    codegen_emit(instrs->args.label_rr.label);
                    codegen_emit("), ");
                    {
                        char rr[3];
                        z80_write_rr((uint8_t*)rr, instrs->args.label_rr.rr);
                        rr[2] = '\0';
                        codegen_emit(rr);
                    }
                    codegen_emit("\n");
                    str = NULL;
                } else if (mode == M_R_MEM) {
                    if (z80_write_mem_rr(&pp_ld_r_mem[8], instrs->args.r_mem.mem)) {
                        pp_ld_r_mem[5] = instrs->args.r_mem.r;
                        str = pp_ld_r_mem;
                    }
                } else if (mode == M_MEM_R) {
                    if (z80_write_mem_rr(&pp_ld_mem_r[6], instrs->args.mem_r.mem)) {
                        pp_ld_mem_r[10] = instrs->args.mem_r.r;
                        str = pp_ld_mem_r;
                    }
                } else if (mode == M_R_MEMI) {
                    pp_ld_r_memi[5] = instrs->args.r_memi.r;
                    z80_write_hex16(&pp_ld_r_memi[10], instrs->args.r_memi.addr);
                    str = pp_ld_r_memi;
                } else if (mode == M_MEMI_R) {
                    pp_ld_memi_r[14] = instrs->args.memi_r.r;
                    z80_write_hex16(&pp_ld_memi_r[8], instrs->args.memi_r.addr);
                    str = pp_ld_memi_r;
                } else if (mode == M_R_MEMO) {
                    pp_ld_r_memo[5] = instrs->args.r_memo.r;
                    z80_write_idx_disp(&pp_ld_r_memo[8], instrs->args.r_memo.idx, instrs->args.r_memo.disp);
                    str = pp_ld_r_memo;
                } else if (mode == M_MEMO_R) {
                    pp_ld_memo_r[15] = instrs->args.memo_r.r;
                    z80_write_idx_disp(&pp_ld_memo_r[6], instrs->args.memo_r.idx, instrs->args.memo_r.disp);
                    str = pp_ld_memo_r;
                }
                break;
            case I_ADD:
                if (mode == M_R_R) {
                    pp_add[9] = instrs->args.r_r.dst;
                    str = pp_add;
                } else if (mode == M_RR_RR && instrs->args.rr_rr.dst == REG_HL) {
                    z80_write_rr(&pp_add_hl_rr[10], instrs->args.rr_rr.src);
                    str = pp_add_hl_rr;
                }
                break;
            case I_SUB:
                if (mode == M_R_R) {
                    pp_sub[6] = instrs->args.r_r.dst;
                    str = pp_sub;
                }
                break;
            case I_SBC:
                if (mode == M_R_R) {
                    pp_sbc[9] = instrs->args.r_r.dst;
                    str = pp_sbc;
                }
                break;
            case I_AND:
                if (mode == M_R_R) {
                    pp_and[6] = instrs->args.r_r.dst;
                    str = pp_and;
                }
                break;
            case I_OR:
                if (mode == M_R_R) {
                    pp_or[5] = instrs->args.r_r.dst;
                    str = pp_or;
                }
                break;
            case I_CPL:
                str = pp_cpl;
                break;
            case I_NEG:
                str = pp_neg;
                break;
            case I_EX:
                if (mode == M_RR_RR) {
                    uint8_t dst = instrs->args.rr_rr.dst;
                    uint8_t src = instrs->args.rr_rr.src;
                    char op_buf[5];
                    codegen_emit("  ex ");
                    z80_format_ex_operand(op_buf, dst, false);
                    codegen_emit(op_buf);
                    codegen_emit(", ");
                    z80_format_ex_operand(op_buf, src, false);
                    codegen_emit(op_buf);
                    codegen_emit("\n");
                    str = NULL;
                } else if (mode == M_MEM_RR && instrs->args.mem_rr.mem == MEM_SP) {
                    char op_buf[5];
                    codegen_emit("  ex ");
                    z80_format_ex_operand(op_buf, REG_SP, true);
                    codegen_emit(op_buf);
                    codegen_emit(", ");
                    z80_format_ex_operand(op_buf, instrs->args.mem_rr.rr, false);
                    codegen_emit(op_buf);
                    codegen_emit("\n");
                    str = NULL;
                }
                break;
            case I_CALL:
                if (mode == M_LABEL) {
                    codegen_emit((const char*)pp_call);
                    codegen_emit(instrs->args.label.label);
                    codegen_emit("\n");
                    str = NULL;
                }
                break;
            case I_JP:
                if (mode == M_LABEL) {
                    codegen_emit("  jp ");
                    codegen_emit(instrs->args.label.label);
                    codegen_emit("\n");
                    str = NULL;
                }
                break;
            case I_JR:
                if (mode == M_LABEL) {
                    codegen_emit("  jr ");
                    codegen_emit(instrs->args.label.label);
                    codegen_emit("\n");
                    str = NULL;
                } else if (mode == M_COND_LABEL) {
                    codegen_emit("  jr ");
                    z80_emit_cond(instrs->args.cond_label.cond);
                    codegen_emit(", ");
                    codegen_emit(instrs->args.cond_label.label);
                    codegen_emit("\n");
                    str = NULL;
                }
                break;
            case I_DJNZ:
                if (mode == M_LABEL) {
                    codegen_emit((const char*)pp_djnz);
                    codegen_emit(instrs->args.label.label);
                    codegen_emit("\n");
                    str = NULL;
                }
                break;
            case I_RRA:
                str = pp_rra;
                break;
            case I_INC:
                if (mode == M_RR_RR) {
                    z80_write_rr(&pp_inc16[6], instrs->args.rr_rr.dst);
                } else {
                    pp_inc16[6] = instrs->args.r_r.dst;
                    pp_inc16[7] = ' ';
                }
                str = pp_inc16;
                break;
            case I_DEC:
                if (mode == M_RR_RR) {
                    z80_write_rr(&pp_dec16[6], instrs->args.rr_rr.dst);
                    str = pp_dec16;
                }
                break;
            case I_PUSH:
                if (mode == M_RR_RR) {
                    z80_write_rr(&pp_push[7], instrs->args.rr_rr.dst);
                    str = pp_push;
                }
                break;
            case I_POP:
                if (mode == M_RR_RR) {
                    z80_write_rr(&pp_pop[6], instrs->args.rr_rr.dst);
                    str = pp_pop;
                }
                break;
            case I_RET:
                str = pp_ret;
                break;
            case I_LABEL:
                if (mode == M_LABEL) {
                    codegen_emit(instrs->args.label.label);
                    codegen_emit(":\n");
                    str = NULL;
                }
                break;
            default:
                break;
        }
        if (str) {
            codegen_emit((const char*)str);
            str = NULL;
        }
        instrs++;
    }
}
