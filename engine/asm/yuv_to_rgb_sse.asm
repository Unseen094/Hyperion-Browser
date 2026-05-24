; YUV420p → BGRA SSE4.1 conversion
; 16 pixels per iteration
; Calling convention: Microsoft x64
; yuv_to_rgb_sse(y: ptr byte, u: ptr byte, v: ptr byte, stride_y: int,
;                 stride_uv: int, out: ptr uint32_t, width: int, height: int)

section .data
align 16
coeff_128     dd 128.0, 128.0, 128.0, 128.0
coeff_1_164   dd 1.164, 1.164, 1.164, 1.164
coeff_0_391   dd 0.0, -0.391, 0.0, -0.391
coeff_1_596   dd 1.596, 0.0, 1.596, 0.0
coeff_0_813   dd 0.0, -0.813, 0.0, -0.813
coeff_2_018   dd 2.018, 0.0, 2.018, 0.0
zero          dd 0, 0, 0, 0
clamp_min     dd 0.0, 0.0, 0.0, 0.0
clamp_max     dd 255.0, 255.0, 255.0, 255.0
alpha_255     dd 255.0, 255.0, 255.0, 255.0

section .text
global yuv_to_rgb_sse
yuv_to_rgb_sse:
    push rbp
    mov rbp, rsp
    push rbx
    push rdi
    push rsi
    ; rcx = y, rdx = u, r8 = v, r9d = stride_y
    ; [rbp+48] = stride_uv, [rbp+56] = out, [rbp+64] = width, [rbp+72] = height
    mov r10d, [rbp+64]       ; width
    mov r11d, [rbp+72]       ; height
    mov r12d, [rbp+48]       ; stride_uv
    mov r13, [rbp+56]        ; out ptr
    mov r14d, r9d            ; stride_y

    movaps xmm7, [coeff_128]
    movaps xmm8, [coeff_1_164]
    movaps xmm9, [coeff_0_391]
    movaps xmm10, [coeff_1_596]
    movaps xmm11, [coeff_0_813]
    movaps xmm12, [coeff_2_018]
    movaps xmm13, [clamp_min]
    movaps xmm14, [clamp_max]
    movaps xmm15, [alpha_255]

    xor r15d, r15d           ; row = 0
.row_loop:
    cmp r15d, r11d
    jge .done
    xor ebx, ebx            ; col = 0
    ; uv row index = row / 2
    mov eax, r15d
    shr eax, 1
    imul eax, r12d          ; uv offset
    mov rdi, rdx            ; u base
    add rdi, rax
    mov rsi, r8             ; v base
    add rsi, rax
    ; y row offset
    mov eax, r15d
    imul eax, r14d
    mov rdi_y, rcx
    add rdi_y, rax
.col_loop:
    cmp ebx, r10d
    jge .next_row
    ; Load 4 Y values
    movd xmm0, [rdi_y + ebx]
    punpcklbw xmm0, xmm0
    punpcklwd xmm0, xmm0
    cvtdq2ps xmm0, xmm0
    ; Load U (subsample)
    movzx eax, byte [rdi + (ebx/2)]
    movd xmm1, eax
    punpcklbw xmm1, xmm1
    punpcklwd xmm1, xmm1
    cvtdq2ps xmm1, xmm1
    ; Load V
    movzx eax, byte [rsi + (ebx/2)]
    movd xmm2, eax
    punpcklbw xmm2, xmm2
    punpcklwd xmm2, xmm2
    cvtdq2ps xmm2, xmm2
    ; Subtract 128
    subps xmm0, xmm7
    subps xmm1, xmm7
    subps xmm2, xmm7
    ; Y *= 1.164
    mulps xmm0, xmm8
    ; R = Y + 1.596 * V
    movaps xmm3, xmm2
    mulps xmm3, xmm10
    addps xmm3, xmm0
    minps xmm3, xmm14
    maxps xmm3, xmm13
    cvtps2dq xmm3, xmm3
    ; G = Y - 0.391 * U - 0.813 * V
    movaps xmm4, xmm1
    mulps xmm4, xmm9
    movaps xmm5, xmm2
    mulps xmm5, xmm11
    addps xmm4, xmm5
    addps xmm4, xmm0
    minps xmm4, xmm14
    maxps xmm4, xmm13
    cvtps2dq xmm4, xmm4
    ; B = Y + 2.018 * U
    movaps xmm5, xmm1
    mulps xmm5, xmm12
    addps xmm5, xmm0
    minps xmm5, xmm14
    maxps xmm5, xmm13
    cvtps2dq xmm5, xmm5
    ; Pack BGRA
    packssdw xmm3, xmm4
    packssdw xmm5, xmm5
    packuswb xmm3, xmm5
    ; Write 4 pixels
    movdqu [r13 + rbx*4], xmm3
    add ebx, 4
    jmp .col_loop
.next_row:
    inc r15d
    jmp .row_loop
.done:
    pop rsi
    pop rdi
    pop rbx
    pop rbp
    ret
