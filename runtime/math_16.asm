; 16-bit mul/div/mod

; Multiply HL by DE (unsigned), result in HL
__mul_hl_de:
    ld a, h
    or d
    jr nz, __mul_hl_de_f
    ld a, l
    ld l, e
    call __mul_a_l
    ld l, a
    ld h, 0
    ret
__mul_hl_de_f:
    ld b, h
    ld c, l
    ld hl, 0
    ld a, 16
__mul_hl_de_l:
    bit 0, e
    jr z, __mul_hl_de_s
    add hl, bc
__mul_hl_de_s:
    sla c
    rl b
    srl d
    rr e
    dec a
    jr nz, __mul_hl_de_l
    ret

; Divide HL by DE (unsigned), quotient in HL
__div_hl_de:
    ld a, d
    or e
    jr nz, __div_hl_de_f
    ld hl, 0
    ret
__div_hl_de_f:
    ld a, h
    or d
    jr nz, __div_hl_de_l
    ld a, l
    ld l, e
    call __div_a_l
    ld l, a
    ld h, 0
    ret
__div_hl_de_l:
    ld bc, 0
__div_hl_de_i:
    or a
    sbc hl, de
    jr c, __div_hl_de_d
    inc bc
    jr __div_hl_de_i
__div_hl_de_d:
    add hl, de
    ld h, b
    ld l, c
    ret

; Modulo HL by DE (unsigned), remainder in HL
__mod_hl_de:
    ld a, d
    or e
    jr nz, __mod_hl_de_f
    ld hl, 0
    ret
__mod_hl_de_f:
    ld a, h
    or d
    jr nz, __mod_hl_de_l
    ld a, l
    ld l, e
    call __mod_a_l
    ld l, a
    ld h, 0
    ret
__mod_hl_de_l:
    or a
    sbc hl, de
    jr c, __mod_hl_de_d
    jr __mod_hl_de_l
__mod_hl_de_d:
    add hl, de
    ret
