/* The native ZOS driver runs each stage in its own address space. */
#ifdef __SDCC
#include <core.h>
#include <zos_sys.h>
#include <zos_vfs.h>

#define WORK "cc-work"
#define AST WORK "/input.ast"
#define ASM WORK "/output.asm"
#define BIN WORK "/output.bin"
#define SAVE WORK "/previous"

static char args[PATH_MAX], output[PATH_MAX];
static unsigned char buffer[256];
static zos_stat_t info;

static int stage(const char *program, const char *first, const char *second) {
    const char *argv[] = {args};
    unsigned int len = str_len(first);
    uint8_t status = 0;
    if (len + (second ? 1 + str_len(second) : 0) >= sizeof(args)) {
        put_s("cc: argument path too long for "); put_s(program); put_s("\n");
        return ERR_PATH_TOO_LONG;
    }
    str_cpy(args, first);
    if (second) {
        args[len++] = ' ';
        str_cpy(args + len, second);
    }
    zos_err_t err = exec(EXEC_PRESERVE_PROGRAM, program, argv, &status);
    if (err || status) {
        put_s("cc: failed "); put_s(program); put_s(" status ");
        put_u8(err ? err : status); put_s("\n");
    }
    return err ? err : status;
}

/* ZOS has no rename syscall. Back up an existing destination before publishing. */
static int copy_file(const char *src, const char *dst) {
    zos_dev_t in = open(src, O_RDONLY);
    if (in < 0) return -in;
    zos_dev_t out = open(dst, O_WRONLY | O_CREAT | O_TRUNC);
    int err = out < 0 ? -out : 0;
    while (!err) {
        uint16_t n = sizeof(buffer);
        err = read(in, buffer, &n);
        if (err || !n) break;
        uint16_t written = n;
        err = write(out, buffer, &written);
        if (!err && written != n) err = ERR_FAILURE;
    }
    if (out >= 0 && close(out) && !err) err = ERR_FAILURE;
    if (close(in) && !err) err = ERR_FAILURE;
    return err;
}

/* Resolve the destination directory before cc-work exists. This prevents a
   user-supplied output path from referring to a private intermediate. */
static int check_output_parent(const char *path) {
    const char *slash = str_chrr(path, '/');
    if (!slash) return 0;
    unsigned int len = slash - path + 1;
    if (len >= sizeof(args)) return ERR_PATH_TOO_LONG;
    str_cpyn(args, path, len);
    args[len] = 0;
    zos_dev_t parent = opendir(args);
    if (parent < 0) return -parent;
    return close(parent);
}

int main(int argc, char **argv) {
    if (!argc) goto usage;
    char *src = argv[0];
    int asm_only = 0, keep_backup = 0;
    if (src[0] == '-' && src[1] == 'S' && src[2] == ' ') {
        asm_only = 1;
        src += 3;
    }
    char *dst = str_chr(src, ' ');
    if (dst) {
        *dst++ = 0;
        if (str_chr(dst, ' ') || !str_ends_with(dst, ".asm")) goto usage;
        asm_only = 1;
    }
    if (!str_ends_with(src, ".c") || str_len(src) + 2 >= sizeof(output)) goto usage;
    if (!dst) {
        str_cpy(output, src);
        str_cpy(output + str_len(src) - 2, asm_only ? ".asm" : ".bin");
        dst = output;
    }
    int err = check_output_parent(dst);
    if (err) { put_s("cc: cannot open output directory\n"); return err; }
    /* A private directory owns every intermediate. Never reuse stale files. */
    err = mkdir(WORK);
    if (err) { put_s("cc: cannot create cc-work\n"); return err; }
    err = stage("/bin/cc_parse", src, AST);
    if (!err) err = stage("/bin/cc_semantic", AST, 0);
    if (!err) err = stage("/bin/cc_codegen", AST, ASM);
    if (!err && !asm_only) err = stage("/bin/zealasm", ASM, BIN);
    if (!err) {
        int lookup = stat(dst, &info);
        int existing = lookup == 0;
        if (lookup && lookup != ERR_NO_SUCH_ENTRY) err = lookup;
        if (existing && D_ISDIR(info.s_flags)) err = ERR_INVALID_PARAMETER;
        if (!err && existing) err = copy_file(dst, SAVE);
        if (!err) {
            err = copy_file(asm_only ? ASM : BIN, dst);
            if (err && existing && copy_file(SAVE, dst)) {
                keep_backup = 1;
                put_s("cc: recover previous output from " SAVE "\n");
            } else if (err && !existing) {
                rm(dst);
            }
        }
        if (err) put_s("cc: could not publish output\n");
    }
    rm(AST); rm(ASM); rm(BIN);
    if (!keep_backup) { rm(SAVE); rm(WORK); }
    return err;
usage:
    put_s("usage: cc [-S] source.c [output.asm]\n");
    return ERR_INVALID_PARAMETER;
}
#else
#include <stdio.h>
int main(void) {
    fputs("cc is the ZOS driver. Use the native cc_parse, cc_semantic and cc_codegen stages on the host.\n", stderr);
    return 2;
}
#endif
