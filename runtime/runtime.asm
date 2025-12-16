; Runtime library for Zeal 8-bit C Compiler
; Helper functions for operations not directly supported by Z80

; Multiply A by L, result in A
; Destroys: B
__mul_a_l:
    ld b, 0        ; result = 0
    or a           ; check if A is 0
    ret z
__mul_loop:
    add a, b       ; B += A
    ld b, a
    dec l
    ret z          ; if L == 0, done
    ld a, b
    jr __mul_loop

; Divide A by L, result in A (quotient), remainder in B
; Destroys: B, C
__div_a_l:
    ld b, 0        ; quotient = 0
    ld c, a        ; save dividend in C
__div_loop:
    ld a, c        ; A = remaining dividend
    cp l           ; compare with divisor
    ret c          ; if A < L, done (quotient in B)
    sub l          ; A -= L
    ld c, a        ; save new dividend
    inc b          ; quotient++
    jr __div_loop

; Modulo A by L, result in A (remainder)
; Uses division routine
__mod_a_l:
    call __div_a_l
    ld a, c        ; remainder is in C after division
    ret
