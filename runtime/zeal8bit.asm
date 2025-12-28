; SPDX-FileCopyrightText: 2023 Zeal 8-bit Computer <contact@zeal8bit.com>
;
; SPDX-License-Identifier: Apache-2.0

; Size of the buffers used for getchar and putchar

; The following routines are written with SDCC new calling convention:
; __sdcccall(1).
; Thus, most parameters will be given in registers. Also, because the
; routines don't have variadic arguments and return a value less or equal
; to 16-bit, we will need to clean the stack.

; zos_err_t read(zos_dev_t dev, void* buf, uint16_t* size);
; Parameters:
;   A       - dev
;   DE      - buf
;   [Stack] - size*
read:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld e, (ix+6)
    ld d, (ix+7)
    ld l, (ix+8)
    ld h, (ix+9)
    ld c, (hl)
    inc hl
    ld b, (hl)
    pop ix
_read:
    ; We have to get the size from the pointer which is on the stack.
    ; Pop the return address and exchange with the pointer on the top.
    ; pop hl
    ; ex (sp), hl
    ; HL contains size*, top of the stack contains the return address.
    ; Dereference the size in BC
    ; ld c, (hl)
    ; inc hl
    ; ld b, (hl)
    ; Save back the address of size on the stack, we will use it to save
    ; the returned value (in BC)
    push hl
    ; Syscall parameters:
    ;   H - Opened dev
    ;   DE - Buffer source
    ;   BC - Buffer size
    ; Returns:
    ;   A - Error value
    ;   BC - Number of bytes read
    ld h, a
    ld l, 0
    rst 0x8
    ; In any case, we have to clean the stack
    pop hl
    ; If error returned is not ERR_SUCCESS, do not alter the given size*
    or a
    jp nz, _read_r
    ; No error so far, we can fill the pointer. Note, HL points to the MSB!
    ld (hl), b
    dec hl
    ld (hl), c
_read_r:
    ret


; zos_err_t write(zos_dev_t dev, const void* buf, uint16_t* size);
write:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld e, (ix+6)
    ld d, (ix+7)
    ld l, (ix+8)
    ld h, (ix+9)
    ld c, (hl)
    inc hl
    ld b, (hl)
    pop ix
_write:
    ; This routine is exactly the same as the one above
    ; pop hl
    ; ex (sp), hl
    ; ld c, (hl)
    ; inc hl
    ; ld b, (hl)
    push hl
    ; Syscall parameters:
    ;   H - Opened dev
    ;   DE - Buffer source
    ;   BC - Buffer size
    ; Returns:
    ;   A - Error value
    ;   BC - Number of bytes written
    ld h, a
    ld l, 1
    rst 0x8
    pop hl
    or a
    jp nz, _write_r
    ld (hl), b
    dec hl
    ld (hl), c
_write_r:
    ret


; int8_t open(const char* name, uint8_t flags);
; Parameters:
;   HL      - name
;   [Stack] - flags
open:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    ld a, (ix+6)
    ld b, h
    ld c, l
    ld h, a
    pop ix
_open:
    ; Copy name/path in BC, as required by the syscall.
    ; ld b, h
    ; ld c, l
    ; Get the flags, which are on the stack, behind the return address.
    ; We have to clean the stack.
    ; pop hl
    ; dec sp
    ; ex (sp), hl
    ; Syscall parameters:
    ;   BC - Name
    ;   H - Flags
    ld l, 2
    rst 0x8
_open_r:
    ret


; zos_err_t close(zos_dev_t dev);
; Parameters:
;   A - dev
close:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    pop ix
_close:
    ld h, a
    ; Syscall parameters:
    ;   H - dev
    ld l, 3
    rst 0x8
    ret


; zos_err_t dstat(zos_dev_t dev, zos_stat_t* stat);
; Parameters:
;   A  - dev
;   DE - *stat
dstat:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld e, (ix+6)
    ld d, (ix+7)
    pop ix
_dstat:
    ; Syscall parameters:
    ;   H  - Opened dev
    ;   DE - File stat structure address
    ld h, a
    ld l, 4
    rst 0x8
    ret


; zos_err_t stat(const char* path, zos_stat_t* stat);
; Parameters:
;   HL - path
;   DE - stat
stat:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    ld e, (ix+6)
    ld d, (ix+7)
    pop ix
_stat:
    ; Syscall parameters:
    ;   BC - Path to file
    ;   DE - File stat structure address
    ld b, h
    ld c, l
    ld l, 5
    rst 0x8
    ret


; zos_err_t seek(zos_dev_t dev, int32_t* offset, zos_whence_t whence);
; Parameters:
;   A  - dev
;   DE - *offset
;   [Stack] - whence
seek:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld l, (ix+8)
    ld h, a
    ld e, (ix+6)
    ld d, (ix+7)
    ; Load 32-bit offset from *offset into BCDE
    push ix
    push de
    pop ix
    ld e, (ix+0)
    ld d, (ix+1)
    ld c, (ix+2)
    ld b, (ix+3)
    pop ix
    ; Point HL to offset MSB for write-back
    ld l, (ix+6)
    ld h, (ix+7)
    inc hl
    inc hl
    inc hl
    ld a, l
_seek:
    ; Pop return address in HL, and exchange with whence
    ; pop hl
    ; dec sp
    ; ex (sp), hl
    ; Put the whence in A and the dev in H
    ; ld l, a
    ; ld a, h
    ; ld h, l
    ; push hl
    ; Start by dereferencing offset from DE
    ; ex de, hl
    ; ld e, (hl)
    ; inc hl
    ; ld d, (hl)
    ; inc hl
    ; ld c, (hl)
    ; inc hl
    ; ld b, (hl)
    ; Pop the parameter H back from the stack and save the offset
    ; ex (sp), hl
    ; Syscall parameters:
    ;   H - Dev number, must refer to an opened driver (not a file)
    ;   BCDE - 32-bit offset, signed if whence is SEEK_CUR/SEEK_END.
    ;          Unsigned if SEEK_SET.
    ;   A - Whence. Can be SEEK_CUR, SEEK_END, SEEK_SET.
    push hl
    ld l, 6
    rst 0x8
    pop hl
    ; If an error occurred, return directly, without modifying offset* value.
    or a
    jr nz, _seek_r
    ; Update the value else (HL points to MSB)
    ld (hl), b
    dec hl
    ld (hl), c
    dec hl
    ld (hl), d
    dec hl
    ld (hl), e
_seek_r:
    pop ix
    ret


; zos_err_t ioctl(zos_dev_t dev, uint8_t cmd, void* arg);
; Parameters:
;   A - dev
;   L - cmd
;   [Stack] - arg
ioctl:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld l, (ix+6)
    ld e, (ix+8)
    ld d, (ix+9)
    ld c, l
    ld h, a
    pop ix
_ioctl:
    ; Put the command in C before we alter HL
    ; ld c, l
    ; Get "arg" parameter out of the stack
    ; pop hl
    ; ex (sp), hl
    ; ex de, hl
    ; Syscall parameters:
    ;   H - Dev number
    ;   C - Command number
    ;   DE - 16-bit parameter. Driver dependent.
    ld h, a
    ld l, 7
    rst 0x8
_ioctl_r:
    ret


; zos_err_t mkdir(const char* path);
; Parameter:
;   HL - path
mkdir:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_mkdir:
    ; Syscall parameter:
    ;   DE - Path
    ex de, hl
    ld l, 8
    rst 0x8
    ret


; zos_err_t chdir(const char* path);
; Parameter:
;   HL - path
chdir:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_chdir:
    ; Syscall parameter:
    ;   DE - Path
    ex de, hl
    ld l, 9
    rst 0x8
    ret


; zos_err_t curdir(char* path);
; Parameter:
;   HL - path
curdir:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_curdir:
    ; Syscall parameter:
    ;   DE - Path
    ex de, hl
    ld l, 10
    rst 0x8
    ret


; zos_err_t opendir(const char* path);
; Parameter:
;   HL - path
opendir:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_opendir:
    ; Syscall parameter:
    ;   DE - Path
    ex de, hl
    ld l, 11
    rst 0x8
    ret


; zos_err_t readdir(zos_dev_t dev, zos_dir_entry_t* dst);
; Parameters:
;   A - dev
;   DE - dst
readdir:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld e, (ix+6)
    ld d, (ix+7)
    pop ix
_readdir:
    ; Syscall parameter:
    ;   H - Opened dev number
    ;   DE - Directory entry address to fill
    ld h, a
    ld l, 12
    rst 0x8
    ret


; zos_err_t rm(const char* path);
; Parameter:
;   HL - path
rm:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_rm:
    ; Syscall parameter:
    ;   DE - Path
    ex de, hl
    ld l, 13
    rst 0x8
    ret


; zos_err_t mount(zos_dev_t dev, char letter, zos_fs_t fs);
; Parameters:
;   A - dev
;   L - letter
;   [Stack] - fs
mount:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld l, (ix+6)
    ld e, (ix+8)
    ld h, a
    ld d, l
    pop ix
_mount:
    ; Save letter in B, we will need HL
    ; ld b, l
    ; Pop fs number from the stack
    ; pop hl
    ; dec sp
    ; ex (sp), hl
    ; fs number in H
    ; Syscall parameters:
    ;   H - Dev number
    ;   D - Letter for the drive
    ;   E - File system
    ; ld e, h
    ; ld d, b
    ; ld h, a
    ld l, 14
    rst 0x8
_mount_r:
    ret


; void exit(uint8_t retval);
; Parameters:
;   A - retval
exit:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    pop ix
_exit:
    ; Return value must be put in H
    ld h, a
    ld l, 15
    rst 0x8
    ret

; zos_err_t exec(zos_exec_mode_t mode, const char* name, char* argv[], uint8_t* retval) CALL_CONV;
; Parameters:
;   A - mode
;   DE - name
;   [SP + 2] - argv
;   [SP + 4] - retval
exec:
    push ix
    ld ix, 0
    add ix, sp
    ; BC = name
    ld e, (ix+6)
    ld d, (ix+7)
    ld b, d
    ld c, e
    ; DE = argv (dereferenced if non-null)
    ld e, (ix+8)
    ld d, (ix+9)
    ld a, d
    or e
    jr z, exec_arg_ready
    push de
    pop hl
    ld e, (hl)
    inc hl
    ld d, (hl)
exec_arg_ready:
    ; Store retval pointer in HL
    ld l, (ix+10)
    ld h, (ix+11)
    ; syscall
    ld a, (ix+4)
    ld h, a
    push hl
    push ix
    push iy
    ld l, 16
    rst 0x8
    pop iy
    pop ix
    pop hl
    or a
    jr nz, exec_r
    ; Store retval if provided
    or h
    or l
    jr z, exec_r
    ld (hl), d
    xor a
exec_r:
    pop ix
    ret
_exec:
    ; (sdcccall stack handling removed)
    ret


; zos_err_t dup(zos_dev_t dev, zos_dev_t ndev);
; Parameters:
;   A - dev
;   L - ndev
dup:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld l, (ix+6)
    pop ix
_dup:
    ; Syscall parameters:
    ;   H - Old dev number
    ;   E - New dev number
    ld h, a
    ld e, l
    ld l, 17
    rst 0x8
    ret


; zos_err_t msleep(uint16_t duration);
; Parameters:
;   HL - duration
msleep:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_msleep:
    ; Syscall parameters:
    ;    DE - duration
    ex de, hl
    ld l, 18
    rst 0x8
    ret


; zos_err_t settime(uint8_t id, zos_time_t* time);
; Parameters:
;   A - id
;   DE - time
settime:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld e, (ix+6)
    ld d, (ix+7)
    pop ix
_settime:
    ; Syscall parameters:
    ;   H - id
    ;   DE - time (v0.1.0 implementation of Zeal 8-bit OS requires
    ;        the milliseconds in DE directly, not an address)
    ex de, hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld h, a
    ld l, 19
    rst 0x8
    ret


; zos_err_t gettime(uint8_t id, zos_time_t* time);
; Parameters:
;   A - id
;   DE - time
gettime:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld e, (ix+6)
    ld d, (ix+7)
    pop ix
_gettime:
    ; BC will be saved during the syscall
    ld b, d
    ld c, e
    ; Syscall parameters:
    ;   H - id
    ;   DE - time (v0.1.0 implementation of Zeal 8-bit OS requires
    ;        the milliseconds in DE directly, not an address)
    ld h, a
    ld l, 20
    rst 0x8
    ; Syscall returns the time in DE on success.
    or a
    ret nz
    ; Success, we can fill the structure.
    ld l, c
    ld h, b
    ld (hl), e
    inc hl
    ld (hl), d
    ret


; zos_err_t setdate(const zos_date_t* date);
; Parameter:
;   HL - date
setdate:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_setdate:
    ; Syscall parameter:
    ;   DE - Date stucture
    ex de, hl
    ld l, 21
    rst 0x8
    ret


; zos_err_t getdate(const zos_date_t* date);
; Parameter:
;   HL - date
getdate:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_getdate:
    ; Syscall parameter:
    ;   DE - Date stucture
    ex de, hl
    ld l, 22
    rst 0x8
    ret


; zos_err_t map(void* vaddr, uint32_t paddr);
; Parameters:
;   HL - vaddr
;   [Stack] - paddr
map:
    push ix
    ld ix, 0
    add ix, sp
    ld e, (ix+4)
    ld d, (ix+5)
    ld c, (ix+7)
    ld b, (ix+8)
    ld h, (ix+9)
    pop ix
_map:
    ; Syscall parameters:
    ;   DE - Virtual adress
    ;   HBC - Upper 24-bit of physical address
    ; ex de, hl   ; virtual address in DE
    ; pop hl
    ; pop bc
    ; ex (sp), hl
    ; ld h, l
    ld l, 23
    rst 0x8
    ret


; zos_err_t swap(zos_dev_t dev, zos_dev_t ndev);
; Parameters:
;   A - fdev
;   L - sdev
swap:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld l, (ix+6)
    pop ix
_swap:
    ; Syscall parameters:
    ;   H - First dev number
    ;   E - Second dev number
    ld h, a
    ld e, l
    ld l, 24
    rst 0x8
    ret


; zos_err_t palloc(uint8_t* page_index);
; Parameters:
;   HL - page_index
palloc:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
_palloc:
    ; DE won't be altered by the syscall
    ex de, hl
    ld l, 25
    rst 0x8
    ex de, hl
    ; Store the returned page in `page_index`
    ld (hl), b
    ret


; zos_err_t pfree(uint8_t page_index);
; Parameters:
;   A - page_index
pfree:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    pop ix
_pfree:
    ; Syscall parameters:
    ;   B - Page to free
    ld b, a
    ld l, 26
    rst 0x8
    ret


; zos_err_t pmap(uint8_t page_index, const void* vaddr) CALL_CONV;
; Parameters:
;   A - page_index
;   DE - vaddr
pmap:
    push ix
    ld ix, 0
    add ix, sp
    ld a, (ix+4)
    ld e, (ix+6)
    ld d, (ix+7)
    pop ix
_pmap:
    ; Syscall parameters:
    ;   DE - Virtual address
    ;   HBC - Upper 24-bits of the physical address to map.
    ; Put A lowest two bits in B's highest two bits
    ld b, 0
    ld c, b
    or a    ; Clear carry flag
    rra
    rr b
    rra
    rr b
    ld h, a
    ; MAP
    ld l, 23
    rst 0x8
    ret


; int getchar(void)
; Get next character from standard input. Input is buffered.
; Returns:
;   DE - Character received
getchar:
_getchar:
    ; Get the size of the buffer, if it's 0, we have to call the READ syscall
    ld a, (_getchar_size)
    or a
    jp nz, _getchar_next
    ; Read a buffer from STDIN:
    ;   H - Opened dev
    ;   DE - Buffer source
    ;   BC - Buffer size
    ; Returns:
    ;   A - Error value
    ;   BC - Number of bytes written
    ld h, 1 ; DEV_STDIN
    ld de, _getchar_buffer
    ld bc, 80
    ld l, 0
    rst 0x8
    or a
    jr nz, _putchar_error
    ; Save the size in the static variable, we can ignore B, we know it's 0
    ; Put the size in A as required by the rest of the code
    ld a, c
    ld (_getchar_size), a
_getchar_next:
    ; Before reading the character, check if we are going to reach the end of the buffer.
    ; In other words, check if Idx + 1 == A (size)
    ld hl, _getchar_idx
    ld d, 0
    ld e, (hl)  ; Index of the buffer in DE
    inc (hl)
    cp (hl)
    ; If result is not 0 (likely), no need to reset the size and index
    jr nz, _getchar_reset
    ; Reset both index and size
    ld (hl), d  ; D is 0 already
    inc hl
    ld (hl), d
    dec hl
_getchar_reset:
    ; HL is pointing to the index in the buffer
    inc hl
    inc hl
    ; Offset of the next character to read: ADD HL, DE
    add hl, de
    ; Character to return in E, D is already 0
    ld e, (hl)
    ret



; int _putchar(int c)
; Print a character on the standard output. Output is buffered.
; Parameters:
;   HL - Character to print
; Returns:
;   DE - Character printed, EOF on error
putchar:
    push ix
    ld ix, 0
    add ix, sp
    ld l, (ix+4)
    ld h, (ix+5)
    pop ix
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
fflush_stdout:
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
    jr _putchar_flush


_getchar_idx:
    .ds 1
_getchar_size:
    .ds 1
_getchar_buffer:
    .ds 80

_putchar_idx:
    .ds 1
_putchar_buffer:
    .ds 80
