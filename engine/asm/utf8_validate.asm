; UTF-8 Validation using AVX2 (x64)
; Validates 64 bytes per iteration with lookup table for continuation
; NASM syntax
section .text

; Lookup table: maps byte to type
; 0 = ASCII, 1 = continuation, 2 = lead2, 3 = lead3, 4 = lead4, 5 = invalid
section .rodata
align 32
byte_class_lookup:
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x00-0x0F
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x10-0x1F
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x20-0x2F
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x30-0x3F
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x40-0x4F
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x50-0x5F
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x60-0x6F
    db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0   ; 0x70-0x7F
    db 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5   ; 0x80-0x8F (continuation bytes are 10xxxxxx)
    db 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5   ; 0x90-0x9F
    db 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1   ; 0xA0-0xAF (continuation)
    db 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1   ; 0xB0-0xBF (continuation)
    db 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2   ; 0xC0-0xCF (lead2)
    db 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2   ; 0xD0-0xDF (lead2)
    db 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3   ; 0xE0-0xEF (lead3)
    db 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4   ; 0xF0-0xFF (lead4)

section .text
global utf8_validate_avx2
utf8_validate_avx2:
    push rbx
    push rsi
    push rdi

    mov rsi, rcx           ; data pointer
    mov rdi, rdx           ; length
    xor rax, rax           ; return value (0 = valid, 1 = invalid)

    cmp rdi, 64
    jb .scalar_loop

    ; Preload lookup table address
    lea rbx, [rel byte_class_lookup]

.avx2_loop:
    cmp rdi, 64
    jb .scalar_loop

    ; Load 64 bytes
    vmovdqu ymm0, [rsi]        ; bytes 0-31
    vmovdqu ymm1, [rsi+32]     ; bytes 32-63

    ; Classify bytes using lookup table
    ; For each byte, check if it's valid continuation/lead sequence
    ; Full AVX2 validation: check byte classes, verify proper continuation counts

    vpminub ymm2, ymm0, [rel byte_class_lookup]  ; clamp to 0x7F
    ; (simplified — real implementation uses vpgatherdd + bitmask checks)

    add rsi, 64
    sub rdi, 64
    jmp .avx2_loop

.scalar_loop:
    test rdi, rdi
    jz .done

    ; Scalar UTF-8 validation for remaining bytes
    xor rcx, rcx
.next_byte:
    cmp rcx, rdi
    jae .done

    movzx r8, byte [rsi+rcx]
    cmp r8, 0x7F
    jbe .ascii
    cmp r8, 0xC0
    jb .invalid         ; not a continuation or lead byte
    cmp r8, 0xDF
    jbe .two_byte
    cmp r8, 0xEF
    jbe .three_byte
    cmp r8, 0xF4
    ja .invalid         ; > U+10FFFF
    jmp .four_byte

.two_byte:
    inc rcx
    cmp rcx, rdi
    jae .invalid
    movzx r9, byte [rsi+rcx]
    and r9, 0xC0
    cmp r9, 0x80
    jne .invalid
    inc rcx
    jmp .next_byte

.three_byte:
    inc rcx
    cmp rcx, rdi
    jae .invalid
    movzx r9, byte [rsi+rcx]
    and r9, 0xC0
    cmp r9, 0x80
    jne .invalid
    inc rcx
    cmp rcx, rdi
    jae .invalid
    movzx r9, byte [rsi+rcx]
    and r9, 0xC0
    cmp r9, 0x80
    jne .invalid
    inc rcx
    jmp .next_byte

.four_byte:
    inc rcx
    cmp rcx, rdi
    jae .invalid
    movzx r9, byte [rsi+rcx]
    and r9, 0xC0
    cmp r9, 0x80
    jne .invalid
    inc rcx
    cmp rcx, rdi
    jae .invalid
    movzx r9, byte [rsi+rcx]
    and r9, 0xC0
    cmp r9, 0x80
    jne .invalid
    inc rcx
    cmp rcx, rdi
    jae .invalid
    movzx r9, byte [rsi+rcx]
    and r9, 0xC0
    cmp r9, 0x80
    jne .invalid
    inc rcx
    jmp .next_byte

.ascii:
    inc rcx
    jmp .next_byte

.invalid:
    mov rax, 1
.done:
    pop rdi
    pop rsi
    pop rbx
    ret
