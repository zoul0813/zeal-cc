#include "cc_compat.h"
#include "codegen.h"

#include "common.h"

static uint8_t pp_ld[]  = "  ld R,R\n";
static uint8_t pp_add[] = "  add R\n";
static uint8_t pp_sbc[] = "  sbc R\n";
static uint8_t pp_and[] = "  and R\n";
static uint8_t pp_or[]  = "  or R\n";
static uint8_t pp_ld_r_mem[] = "  ld R,(RR)\n";
static uint8_t pp_ld_mem_r[] = "  ld (RR),R\n";
static uint8_t pp_cpl[] = "  cpl\n";
static uint8_t pp_rra[] = "  rra\n";
static uint8_t pp_inc16[] = "  inc RR\n";
static uint8_t pp_dec16[] = "  dec RR\n";

static void z80_write_rr(uint8_t* out, uint8_t rr) {
    switch ((z80_reg16_t)rr) {
        case REG_BC:
            out[0] = 'b';
            out[1] = 'c';
            break;
        case REG_DE:
            out[0] = 'd';
            out[1] = 'e';
            break;
        case REG_HL:
            out[0] = 'h';
            out[1] = 'l';
            break;
        case REG_SP:
            out[0] = 's';
            out[1] = 'p';
            break;
        case REG_IX:
            out[0] = 'i';
            out[1] = 'x';
            break;
        case REG_IY:
            out[0] = 'i';
            out[1] = 'y';
            break;
        default:
            out[0] = '?';
            out[1] = '?';
            break;
    }
}

static bool z80_write_mem_rr(uint8_t* out, uint8_t mem) {
    switch ((z80_mem_t)mem) {
        case MEM_BC:
            z80_write_rr(out, REG_BC);
            return true;
        case MEM_DE:
            z80_write_rr(out, REG_DE);
            return true;
        case MEM_HL:
            z80_write_rr(out, REG_HL);
            return true;
        case MEM_SP:
            z80_write_rr(out, REG_SP);
            return true;
        default:
            break;
    }
    return false;
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
                if (mode == Z80_AM_R8_R8) {
                    pp_ld[5] = instrs->args.r8_r8.dst;
                    pp_ld[7] = instrs->args.r8_r8.src;
                    str = pp_ld;
                } else if (mode == Z80_AM_R8_MEM) {
                    if (z80_write_mem_rr(&pp_ld_r_mem[8], instrs->args.r8_mem.mem)) {
                        pp_ld_r_mem[5] = instrs->args.r8_mem.r;
                        str = pp_ld_r_mem;
                    }
                } else if (mode == Z80_AM_MEM_R8) {
                    if (z80_write_mem_rr(&pp_ld_mem_r[6], instrs->args.mem_r8.mem)) {
                        pp_ld_mem_r[10] = instrs->args.mem_r8.r;
                        str = pp_ld_mem_r;
                    }
                }
                break;
            case I_ADD:
                pp_add[6] = instrs->args.r8.r;
                str = pp_add;
                break;
            case I_SBC:
                pp_sbc[6] = instrs->args.r8.r;
                str = pp_sbc;
                break;
            case I_AND:
                pp_and[6] = instrs->args.r8.r;
                str = pp_and;
                break;
            case I_OR:
                pp_or[5] = instrs->args.r8.r;
                str = pp_or;
                break;
            case I_CPL:
                str = pp_cpl;
                break;
            case I_EX:
                if (mode == Z80_AM_R16_R16) {
                    uint8_t dst = instrs->args.r16_r16.dst;
                    uint8_t src = instrs->args.r16_r16.src;
                    char op_buf[5];
                    codegen_emit("  ex ");
                    z80_format_ex_operand(op_buf, dst, false);
                    codegen_emit(op_buf);
                    codegen_emit(", ");
                    z80_format_ex_operand(op_buf, src, false);
                    codegen_emit(op_buf);
                    codegen_emit("\n");
                    str = NULL;
                } else if (mode == Z80_AM_MEM_R16 && instrs->args.mem_r16.mem == MEM_SP) {
                    char op_buf[5];
                    codegen_emit("  ex ");
                    z80_format_ex_operand(op_buf, REG_SP, true);
                    codegen_emit(op_buf);
                    codegen_emit(", ");
                    z80_format_ex_operand(op_buf, instrs->args.mem_r16.rr, false);
                    codegen_emit(op_buf);
                    codegen_emit("\n");
                    str = NULL;
                }
                break;
            case I_RRA:
                str = pp_rra;
                break;
            case I_INC:
                if (mode == Z80_AM_R16) {
                    z80_write_rr(&pp_inc16[6], instrs->args.r16.rr);
                } else {
                    pp_inc16[6] = instrs->args.r8.r;
                    pp_inc16[7] = ' ';
                }
                str = pp_inc16;
                break;
            case I_DEC:
                if (mode == Z80_AM_R16) {
                    z80_write_rr(&pp_dec16[6], instrs->args.r16.rr);
                    str = pp_dec16;
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
