#include "codegen.h"
#include "common.h"

#ifdef __SDCC
extern void put_s(const char* str);
extern void put_c(char c);
#else
#include <stdio.h>
#include <stdarg.h>
#endif

#define INITIAL_OUTPUT_CAPACITY 4096

codegen_t* codegen_create(const char* output_file, symbol_table_t* symbols) {
    codegen_t* gen = (codegen_t*)cc_malloc(sizeof(codegen_t));
    if (!gen) return NULL;
    
    gen->output_file = output_file;
    gen->output_capacity = INITIAL_OUTPUT_CAPACITY;
    gen->output_buffer = (char*)cc_malloc(gen->output_capacity);
    if (!gen->output_buffer) {
        cc_free(gen);
        return NULL;
    }
    
    gen->output_size = 0;
    gen->output_buffer[0] = '\0';
    
    gen->global_symbols = symbols;
    gen->current_scope = symbols;
    gen->label_counter = 0;
    gen->string_counter = 0;
    gen->temp_counter = 0;
    gen->stack_offset = 0;
    
    gen->reg_a_used = false;
    gen->reg_hl_used = false;
    gen->reg_de_used = false;
    gen->reg_bc_used = false;
    
    return gen;
}

void codegen_destroy(codegen_t* gen) {
    if (!gen) return;
    
    if (gen->output_buffer) {
        cc_free(gen->output_buffer);
    }
    
    cc_free(gen);
}

void codegen_emit(codegen_t* gen, const char* fmt, ...) {
    if (!gen || !gen->output_buffer) return;
    
    /* Simple string append for now */
    const char* p = fmt;
    while (*p && gen->output_size < gen->output_capacity - 1) {
        gen->output_buffer[gen->output_size++] = *p++;
    }
    gen->output_buffer[gen->output_size] = '\0';
}

void codegen_emit_comment(codegen_t* gen, const char* fmt, ...) {
    codegen_emit(gen, "; ");
    codegen_emit(gen, fmt);
    codegen_emit(gen, "\n");
}

char* codegen_new_label(codegen_t* gen) {
    static char label[32];
    label[0] = 'L';
    
    /* Convert label counter to string */
    int n = gen->label_counter++;
    int i = 1;
    if (n == 0) {
        label[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (n > 0) {
            temp[j++] = '0' + (n % 10);
            n /= 10;
        }
        while (j > 0) {
            label[i++] = temp[--j];
        }
    }
    label[i] = '\0';
    
    return cc_strdup(label);
}

char* codegen_new_string_label(codegen_t* gen) {
    static char label[32];
    label[0] = 'S';
    
    int n = gen->string_counter++;
    int i = 1;
    if (n == 0) {
        label[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (n > 0) {
            temp[j++] = '0' + (n % 10);
            n /= 10;
        }
        while (j > 0) {
            label[i++] = temp[--j];
        }
    }
    label[i] = '\0';
    
    return cc_strdup(label);
}

void codegen_emit_prologue(codegen_t* gen, const char* func_name) {
    codegen_emit(gen, func_name);
    codegen_emit(gen, ":\n");
    codegen_emit(gen, "    push bc\n");
    codegen_emit(gen, "    push de\n");
    codegen_emit(gen, "    push hl\n");
}

void codegen_emit_epilogue(codegen_t* gen) {
    codegen_emit(gen, "    pop hl\n");
    codegen_emit(gen, "    pop de\n");
    codegen_emit(gen, "    pop bc\n");
    codegen_emit(gen, "    ret\n");
}

/* Forward declarations */
static cc_error_t codegen_statement(codegen_t* gen, ast_node_t* node);
static cc_error_t codegen_expression(codegen_t* gen, ast_node_t* node);

static cc_error_t codegen_expression(codegen_t* gen, ast_node_t* node) {
    if (!node) return CC_ERROR_INTERNAL;
    
    switch (node->type) {
        case AST_CONSTANT:
            /* Load constant into A register */
            codegen_emit(gen, "    ld a, ");
            {
                /* Convert int to string */
                int val = node->data.constant.int_value;
                char buf[16];
                int i = 0;
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    int neg = 0;
                    if (val < 0) {
                        neg = 1;
                        val = -val;
                    }
                    char temp[16];
                    int j = 0;
                    while (val > 0) {
                        temp[j++] = '0' + (val % 10);
                        val /= 10;
                    }
                    if (neg) buf[i++] = '-';
                    while (j > 0) {
                        buf[i++] = temp[--j];
                    }
                }
                buf[i] = '\0';
                codegen_emit(gen, buf);
            }
            codegen_emit(gen, "\n");
            return CC_OK;
            
        case AST_IDENTIFIER:
            /* Load variable from stack/memory */
            codegen_emit_comment(gen, "Load variable: ");
            codegen_emit_comment(gen, node->data.identifier.name);
            codegen_emit(gen, "    ld a, (");
            codegen_emit(gen, node->data.identifier.name);
            codegen_emit(gen, ")\n");
            return CC_OK;
            
        case AST_BINARY_OP: {
            /* Evaluate left operand (result in A) */
            cc_error_t err = codegen_expression(gen, node->data.binary_op.left);
            if (err != CC_OK) return err;
            
            /* For right operand, we need to save A and evaluate right */
            /* Push left result onto stack */
            codegen_emit(gen, "    push af\n");
            
            /* Evaluate right operand (result in A) */
            err = codegen_expression(gen, node->data.binary_op.right);
            if (err != CC_OK) return err;
            
            /* Pop left operand into L */
            codegen_emit(gen, "    ld l, a\n");
            codegen_emit(gen, "    pop af\n");
            
            /* Perform operation: A op L, result in A */
            switch (node->data.binary_op.op) {
                case OP_ADD:
                    codegen_emit(gen, "    add a, l\n");
                    break;
                case OP_SUB:
                    codegen_emit(gen, "    sub l\n");
                    break;
                case OP_MUL:
                    /* Z80 doesn't have mul, need to call helper */
                    codegen_emit_comment(gen, "Multiplication (A * L)");
                    codegen_emit(gen, "    call __mul_a_l\n");
                    break;
                case OP_DIV:
                    /* Z80 doesn't have div, need to call helper */
                    codegen_emit_comment(gen, "Division (A / L)");
                    codegen_emit(gen, "    call __div_a_l\n");
                    break;
                case OP_MOD:
                    /* Modulo operation */
                    codegen_emit_comment(gen, "Modulo (A % L)");
                    codegen_emit(gen, "    call __mod_a_l\n");
                    break;
                default:
                    return CC_ERROR_CODEGEN;
            }
            return CC_OK;
        }
        
        case AST_CALL: {
            /* Function call - for now, simple calling convention */
            codegen_emit_comment(gen, "Call function: ");
            codegen_emit_comment(gen, node->data.call.name);
            codegen_emit(gen, "    call ");
            codegen_emit(gen, node->data.call.name);
            codegen_emit(gen, "\n");
            return CC_OK;
        }
        
        case AST_ASSIGN: {
            /* Assignment: evaluate RHS, store to LHS */
            cc_error_t err = codegen_expression(gen, node->data.assign.rvalue);
            if (err != CC_OK) return err;
            
            /* Store A to variable */
            if (node->data.assign.lvalue->type == AST_IDENTIFIER) {
                codegen_emit(gen, "    ld (");
                codegen_emit(gen, node->data.assign.lvalue->data.identifier.name);
                codegen_emit(gen, "), a\n");
            }
            return CC_OK;
        }
        
        default:
            return CC_ERROR_CODEGEN;
    }
}

static cc_error_t codegen_statement(codegen_t* gen, ast_node_t* node) {
    if (!node) return CC_OK;
    
    switch (node->type) {
        case AST_RETURN_STMT:
            if (node->data.return_stmt.expr) {
                /* Evaluate return expression into A */
                cc_error_t err = codegen_expression(gen, node->data.return_stmt.expr);
                if (err != CC_OK) return err;
            } else {
                codegen_emit(gen, "    ld a, 0\n");
            }
            codegen_emit(gen, "    ret\n");
            return CC_OK;
            
        case AST_VAR_DECL:
            /* Reserve space for variable */
            codegen_emit_comment(gen, "Variable: ");
            codegen_emit_comment(gen, node->data.var_decl.name);
            codegen_emit(gen, node->data.var_decl.name);
            codegen_emit(gen, ":\n");
            codegen_emit(gen, "    defb 0\n");
            
            /* If there's an initializer, generate assignment */
            if (node->data.var_decl.initializer) {
                cc_error_t err = codegen_expression(gen, node->data.var_decl.initializer);
                if (err != CC_OK) return err;
                codegen_emit(gen, "    ld (");
                codegen_emit(gen, node->data.var_decl.name);
                codegen_emit(gen, "), a\n");
            }
            return CC_OK;
            
        case AST_COMPOUND_STMT:
            /* Process statements in compound block */
            for (size_t i = 0; i < node->data.compound.stmt_count; i++) {
                cc_error_t err = codegen_statement(gen, node->data.compound.statements[i]);
                if (err != CC_OK) return err;
            }
            return CC_OK;
            
        case AST_ASSIGN:
        case AST_CALL:
            /* Expression statements */
            return codegen_expression(gen, node);
            
        default:
            return CC_OK;
    }
}

static cc_error_t codegen_function(codegen_t* gen, ast_node_t* node) {
    if (!node || node->type != AST_FUNCTION) {
        return CC_ERROR_INTERNAL;
    }
    
    codegen_emit(gen, node->data.function.name);
    codegen_emit(gen, ":\n");
    
    /* Generate function body */
    if (node->data.function.body) {
        cc_error_t err = codegen_statement(gen, node->data.function.body);
        if (err != CC_OK) return err;
    } else {
        codegen_emit(gen, "    ld a, 0\n");
        codegen_emit(gen, "    ret\n");
    }
    
    codegen_emit(gen, "\n");
    
    return CC_OK;
}

static void codegen_emit_runtime(codegen_t* gen) {
    codegen_emit(gen, "\n");
    codegen_emit_comment(gen, "Runtime library functions");
    codegen_emit(gen, "\n");
    
    /* Multiply A by L */
    codegen_emit(gen, "__mul_a_l:\n");
    codegen_emit(gen, "    ld b, 0\n");
    codegen_emit(gen, "    or a\n");
    codegen_emit(gen, "    ret z\n");
    codegen_emit(gen, "__mul_loop:\n");
    codegen_emit(gen, "    add a, b\n");
    codegen_emit(gen, "    ld b, a\n");
    codegen_emit(gen, "    dec l\n");
    codegen_emit(gen, "    ret z\n");
    codegen_emit(gen, "    ld a, b\n");
    codegen_emit(gen, "    jr __mul_loop\n");
    codegen_emit(gen, "\n");
    
    /* Divide A by L */
    codegen_emit(gen, "__div_a_l:\n");
    codegen_emit(gen, "    ld b, 0\n");
    codegen_emit(gen, "    ld c, a\n");
    codegen_emit(gen, "__div_loop:\n");
    codegen_emit(gen, "    ld a, c\n");
    codegen_emit(gen, "    cp l\n");
    codegen_emit(gen, "    ret c\n");
    codegen_emit(gen, "    sub l\n");
    codegen_emit(gen, "    ld c, a\n");
    codegen_emit(gen, "    inc b\n");
    codegen_emit(gen, "    jr __div_loop\n");
    codegen_emit(gen, "\n");
    
    /* Modulo A by L */
    codegen_emit(gen, "__mod_a_l:\n");
    codegen_emit(gen, "    call __div_a_l\n");
    codegen_emit(gen, "    ld a, c\n");
    codegen_emit(gen, "    ret\n");
}

cc_error_t codegen_generate(codegen_t* gen, ast_node_t* ast) {
    if (!gen || !ast) {
        return CC_ERROR_INTERNAL;
    }
    
    /* Emit file header */
    codegen_emit_comment(gen, "Generated by Zeal 8-bit C Compiler");
    codegen_emit_comment(gen, "Target: Z80");
    codegen_emit(gen, "\n");
    
    /* Emit standard header for ZOS */
    codegen_emit(gen, "    org 0x4000\n");
    codegen_emit(gen, "\n");
    
    codegen_emit_comment(gen, "Program code");
    
    /* Process AST - handle both PROGRAM node and direct FUNCTION node */
    if (ast->type == AST_PROGRAM) {
        /* Generate code for all top-level declarations */
        for (size_t i = 0; i < ast->data.program.decl_count; i++) {
            ast_node_t* decl = ast->data.program.declarations[i];
            if (decl->type == AST_FUNCTION) {
                cc_error_t err = codegen_function(gen, decl);
                if (err != CC_OK) return err;
            }
        }
        
        /* Emit runtime library */
        codegen_emit_runtime(gen);
        
        return CC_OK;
    } else if (ast->type == AST_FUNCTION) {
        /* Direct function node */
        cc_error_t err = codegen_function(gen, ast);
        if (err != CC_OK) return err;
        
        /* Emit runtime library */
        codegen_emit_runtime(gen);
        
        return err;
    }
    
    return CC_OK;
}

cc_error_t codegen_write_output(codegen_t* gen) {
    if (!gen || !gen->output_buffer) {
        return CC_ERROR_INTERNAL;
    }
    
#ifdef __SDCC
    /* ZOS file writing */
    zos_dev_t dev;
    zos_err_t err;
    uint16_t size;
    
    dev = open(gen->output_file, O_WRONLY | O_CREAT | O_TRUNC);
    if (dev < 0) {
        cc_error("Could not create output file");
        return CC_ERROR_FILE_NOT_FOUND;
    }
    
    size = (uint16_t)gen->output_size;
    err = write(dev, gen->output_buffer, &size);
    close(dev);
    
    if (err != ERR_SUCCESS) {
        cc_error("Could not write output file");
        return CC_ERROR_CODEGEN;
    }
    
    return CC_OK;
#else
    /* Desktop file writing */
    FILE* file = fopen(gen->output_file, "w");
    if (!file) {
        cc_error("Could not create output file");
        return CC_ERROR_FILE_NOT_FOUND;
    }
    
    fwrite(gen->output_buffer, 1, gen->output_size, file);
    fclose(file);
    
    return CC_OK;
#endif
}
