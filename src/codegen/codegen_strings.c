#include "codegen_strings.h"

const char CG_STR_NL[] = "\n";
const char CG_STR_LD_A_ZERO[] = "  ld a, 0\n";
const char CG_STR_LD_A_ONE[] = "  ld a, 1\n";
const char CG_STR_LD_HL_ZERO[] = "  ld hl, 0\n";
const char CG_STR_LD_A_IX_PREFIX[] = "  ld a, (ix";
const char CG_STR_LD_IX_PREFIX[] = "  ld (ix";
const char CG_STR_ADD_HL_DE[] = "  add hl, de\n";
const char CG_STR_RPAREN_NL[] = ")\n";
const char CG_STR_DB[] = "  .db ";
const char CG_STR_DW[] = "  .dw ";
const char CG_STR_DM[] = "  .dm ";
const char CG_STR_DS[] = "  .ds ";
const char CG_STR_OR_A_SBC_HL_DE[] = "  or a\n  sbc hl, de\n";
const char CG_STR_EX_DE_HL_OR_A_SBC_HL_DE[] = "  ex de, hl\n  or a\n  sbc hl, de\n";
const char CG_STR_IX_FRAME_SET[] = "  ld ix, 0\n  add ix, sp\n";
const char CG_STR_OR_A_JP_Z[] = "  or a\n  jp z, ";

const char CG_MSG_FAILED_READ_AST_HEADER[] = "Failed to read AST header\n";
const char CG_MSG_FAILED_READ_AST_STRING_TABLE[] = "Failed to read AST string table\n";
const char CG_MSG_UNSUPPORTED_ARRAY_ACCESS[] = "Unsupported array access";
const char CG_MSG_ARRAY_INIT_NOT_SUPPORTED[] = "Array initialization not supported";
const char CG_MSG_USAGE_CODEGEN[] = "Usage: cc_codegen <input.ast> <output.asm>\n";
const char CG_MSG_FAILED_OPEN_OUTPUT[] = "Failed to open output file\n";
const char CG_MSG_CODEGEN_FAILED[] = "Code generation failed\n";
