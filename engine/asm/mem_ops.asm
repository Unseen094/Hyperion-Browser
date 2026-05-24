; SIMD Memory Operations (x64)
; NASM syntax
section .text

; ---- memcpy using REP MOVSB (ERMSB) + AVX512 ----
global memcpy_ermsb
memcpy_ermsb:
    push rdi
    push rsi
    mov rdi, rcx        ; dest
    mov rsi, rdx        ; src
    mov rcx, r8         ; size
    rep movsb
    mov rax, rcx
    pop rsi
    pop rdi
    ret

; ---- memcpy using AVX512 (large copies) ----
global memcpy_avx512
memcpy_avx512:
    push rdi
    push rsi
    mov rdi, rcx        ; dest
    mov rsi, rdx        ; src
    mov rcx, r8         ; size

    ; Aligned 64-byte copies
    mov rax, rcx
    and rax, 63
    sub rcx, rax

.loop:
    test rcx, rcx
    jz .tail
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 [rdi], zmm0
    add rsi, 64
    add rdi, 64
    sub rcx, 64
    jmp .loop

.tail:
    mov rcx, rax
    rep movsb
    pop rsi
    pop rdi
    ret

; ---- memset with non-temporal stores ----
global memset_nt
memset_nt:
    push rdi
    mov rdi, rcx        ; dest
    movzx rax, dl       ; value (byte)
    mov rcx, r8         ; size

    ; Fill 8-byte register
    mov ah, al
    mov bx, ax
    shl rax, 16
    mov ax, bx
    mov rdx, rax
    shl rax, 32
    or rax, rdx         ; rax = 8 copies of value

    ; Aligned non-temporal store loop
.loop:
    cmp rcx, 64
    jb .tail
    movnti [rdi], rax
    movnti [rdi+8], rax
    movnti [rdi+16], rax
    movnti [rdi+24], rax
    movnti [rdi+32], rax
    movnti [rdi+40], rax
    movnti [rdi+48], rax
    movnti [rdi+56], rax
    add rdi, 64
    sub rcx, 64
    jmp .loop

.tail:
    rep stosb           ; remainder
    sfence              ; flush NT buffers
    pop rdi
    ret

; ---- memcmp using SSE4.1 ----
global memcmp_sse
memcmp_sse:
    push rdi
    push rsi
    mov rdi, rcx        ; a
    mov rsi, rdx        ; b
    mov rcx, r8         ; size
    xor rax, rax

.loop:
    cmp rcx, 16
    jb .tail
    movdqu xmm0, [rdi]
    movdqu xmm1, [rsi]
    pcmpeqb xmm0, xmm1
    pmovmskb edx, xmm0
    cmp edx, 0xFFFF
    jne .diff
    add rdi, 16
    add rsi, 16
    sub rcx, 16
    jmp .loop

.tail:
    repe cmpsb
    je .equal
    mov al, 1
.diff:
    sub al, 1
    sbb rax, rax
    or rax, 1
    pop rsi
    pop rdi
    ret
.equal:
    xor rax, rax
    pop rsi
    pop rdi
    ret
