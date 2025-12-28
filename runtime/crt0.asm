; Zeal 8-bit OS crt0.asm - minimal startup for user programs
; Provides _start entry, calls _main, then exits via syscall

; Start of crt0.asm
_start:
    call main     ; Call user main()
    ld a, l
    call _exit

; End of crt0.asm
