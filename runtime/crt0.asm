; Zeal 8-bit OS crt0.asm - minimal startup for user programs
; Provides _start entry, calls _main, then exits via syscall

; Start of crt0.asm
_start:
    call main     ; Call user main()
    ld a, l
    call _exit

    ; void exit(uint8_t retval);
    ; Parameters:
    ;   A - retval
_exit:
    ld h, a       ; Move return value from A to H (compiler ABI)
    ld l, 15      ; EXIT syscall
    rst 0x8       ; ZOS exit syscall (returns to shell)
    halt

; End of crt0.asm
