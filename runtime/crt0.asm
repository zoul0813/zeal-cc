; Zeal 8-bit OS crt0.asm - minimal startup for user programs
; Provides _start entry, calls _main, then exits via syscall

    .globl _start
    .globl _main

_start:
    call _main        ; Call user main()
    ld l, a           ; Move return value from A to L (compiler ABI)
    rst 0x8           ; ZOS exit syscall (returns to shell)
    halt

; End of crt0.asm
