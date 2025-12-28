; int _putchar(char c)

    ; .equ STD_BUFFER_SIZE, 80

    ; int _putchar(int c)
    ; Print a character on the standard output. Output is buffered.
    ; Parameters:
    ;   HL - Character to print
    ; Returns:
    ;   DE - Character printed, EOF on error
    ; .globl _putchar
_putchar:
    ; Store the character to print in E, ignore high byte
    ld d, 0
    ld e, l
    ; Add the character to the buffer and increment the index
    ld a, (_putchar_idx)
    ld c, a ; Backup A
    ld hl, _putchar_buffer
    ; ADD HL, A
    add a, l
    ld l, a
    adc h
    sub l
    ld h, a
    ; Store the byte to print
    ld (hl), e
    ; We have to flush the buffer if ++A is 80 OR if A is '\n'
    inc c
    ld a, c
    ld (_putchar_idx), a    ; In most cases, we won't flush
    ; BC = A
    ld b, 0
    sub 80
    jr z, _putchar_flush
    ; Check if the character is \n
    ld a, e
    sub 0x0A
    ; Return if we have nothing to flush
    ret nz
_putchar_flush:
    ; BC contains the current length of the buffer, update the index to 0 (A)
    ld (_putchar_idx), a
    ; Write the buffer to STDOUT:
    ;   H - Opened dev
    ;   DE - Buffer source
    ;   BC - Buffer size
    ; Returns:
    ;   A - Error value
    ;   BC - Number of bytes written
    ld h, a ; DEV_STDOUT = 0, A is 0 here
    push de ; Return value
    ld de, _putchar_buffer
    ; syscall 1
    ld l, 1
    rst 0x8
    pop de
    ; Check if an error occurred
    or a
    ; Return directly on success
    ret z
_putchar_error:
    ; Error, set DE to EOF (-1)
    ld de, 0xffff
    ret


    ; int fflush_stdout(void);
    ; Flush stdout if its buffer is not empty.
    ; Parameters:
    ;   None
    ; Returns:
    ;   DE - 0 upon completion, EOF in case of error
_fflush_stdout:
    ; If we have nothing in the buffer, there is nothing to flush
    ld a, (_putchar_idx)
    or a
    ret z
    ; Set BC to the size of the buffer while setting A to the new index: 0
    ld c, a
    xor a
    ld b, a
    ; Return value in DE will be kept, set it to 0
    ld d, a
    ld e, a
    jp _putchar_flush

_putchar_idx:
    .ds 1
_putchar_buffer:
    .ds 80
