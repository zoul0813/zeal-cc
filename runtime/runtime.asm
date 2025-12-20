; Runtime library for Zeal 8-bit C Compiler
; Helper functions for operations not directly supported by Z80

; Multiply A by L, result in A
; Destroys: B
__mul_a_l:
    ld b, l        ; multiplier (loop counter)
    ld c, a        ; multiplicand
    ld a, b
    or c
    ret z          ; if either operand is zero
    ld a, 0        ; result accumulator
__mul_loop:
    add a, c
    djnz __mul_loop
    ret

; Divide A by L, result in A (quotient), remainder in B
; Destroys: B, C
__div_a_l:
    ld c, a            ; dividend
    ld a, l
    or a
    ret z              ; divide by zero => 0
    ld b, 0            ; quotient
__div_loop:
    ld a, c
    cp l
    jr c,__div_done
    sub l
    ld c, a
    inc b
    jr __div_loop
__div_done:
    ld a, b            ; result = quotient
    ret

; Modulo A by L, result in A (remainder)
__mod_a_l:
    ld b, l            ; divisor
    ld c, a            ; remainder working copy
    ld a, b
    or a
    ret z              ; divide by zero => 0
__mod_loop:
    ld a, c
    cp b
    jr c,__mod_done
    sub b
    ld c, a
    jr __mod_loop
__mod_done:
    ld a, c
    ret
