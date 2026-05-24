; ---- Motion Compensation with AVX2 bilinear interpolation -----------------
; Performs 8x8, 16x16 bilinear MC using AVX2 for sub-pixel motion vectors.

section .text

global mc_bilinear_8x8_avx2
global mc_bilinear_16x16_avx2

; void mc_bilinear_8x8_avx2(
;     uint8_t *dst, int dst_stride,
;     const uint8_t *ref, int ref_stride,
;     int mv_x, int mv_y);
;
; Interpolates an 8x8 block from reference with quarter-pel precision.
mc_bilinear_8x8_avx2:
    push      rbp
    mov       rbp, rsp
    push      rbx

    mov       rdi, rcx             ; dst
    mov       esi, edx             ; dst_stride
    mov       rdx, r8              ; ref
    mov       ecx, r9d             ; ref_stride

    ; Parameters passed on stack: mv_x, mv_y
    mov       r10d, [rbp + 48]     ; mv_x
    mov       r11d, [rbp + 56]     ; mv_y

    and       r10d, 3              ; frac_x (0-3)
    and       r11d, 3              ; frac_y (0-3)
    sar       dword [rbp + 48], 2  ; int_x
    sar       dword [rbp + 56], 2  ; int_y

    mov       r8d, [rbp + 48]      ; int_x
    mov       r9d, [rbp + 56]      ; int_y

    ; Precompute horizontal filter taps for frac_x
    ; frac_x=0: [64,0,0,0]
    ; frac_x=1: [48,16,0,0]
    ; frac_x=2: [32,32,0,0]
    ; frac_x=3: [16,48,0,0]
    vpbroadcastd ymm0, dword [.filter_taps + r10*4]
    vpbroadcastd ymm1, dword [.filter_taps2 + r10*4]

    xor       r12d, r12d
.row_loop:
    ; Load 8 pixels from ref[row] and ref[row+1]
    lea       r13, [rdx + r8]      ; ref + int_x
    lea       r14, [r13 + r9 * rcx] ; ref + int_x + int_y * stride

    vpmovzxbd ymm2, [r14 + r12 * rcx]        ; ref row y
    vpmovzxbd ymm3, [r14 + r12 * rcx + 1]    ; ref row y, +1
    vpmovzxbd ymm4, [r14 + (r12+1) * rcx]    ; ref row y+1
    vpmovzxbd ymm5, [r14 + (r12+1) * rcx + 1]; ref row y+1, +1

    ; Horizontal interpolation
    vpmulld   ymm2, ymm2, ymm0
    vpmulld   ymm3, ymm3, ymm1
    vpaddd    ymm2, ymm2, ymm3

    vpmulld   ymm4, ymm4, ymm0
    vpmulld   ymm5, ymm5, ymm1
    vpaddd    ymm4, ymm4, ymm5

    ; Vertical interpolation
    vpbroadcastd ymm6, dword [.filter_taps + r11*4]
    vpbroadcastd ymm7, dword [.filter_taps2 + r11*4]
    vpmulld   ymm2, ymm2, ymm6
    vpmulld   ymm4, ymm4, ymm7
    vpaddd    ymm2, ymm2, ymm4

    ; Round and pack
    vpcmpeqd  ymm3, ymm3, ymm3
    vpslld    ymm3, ymm3, 11       ; 2048 for rounding
    vpaddd    ymm2, ymm2, ymm3
    vpsrad    ymm2, ymm2, 12

    vpackusdw ymm2, ymm2, ymm2
    vpackuswb ymm2, ymm2, ymm2

    ; Store 8 bytes
    vmovd     [rdi + r12 * rsi], xmm2

    inc       r12d
    cmp       r12d, 8
    jl        .row_loop

    pop       rbx
    pop       rbp
    ret

align 16
.filter_taps:
    dd 64, 48, 32, 16
.filter_taps2:
    dd 0, 16, 32, 48

; void mc_bilinear_16x16_avx2(
;     uint8_t *dst, int dst_stride,
;     const uint8_t *ref, int ref_stride,
;     int mv_x, int mv_y);
;
; 16x16 bilinear motion compensation.
mc_bilinear_16x16_avx2:
    push      rbp
    mov       rbp, rsp

    mov       rdi, rcx             ; dst
    mov       esi, edx             ; dst_stride
    mov       rdx, r8              ; ref
    mov       ecx, r9d             ; ref_stride
    mov       r10d, [rbp + 48]     ; mv_x
    mov       r11d, [rbp + 56]     ; mv_y

    and       r10d, 3
    and       r11d, 3
    sar       dword [rbp + 48], 2
    sar       dword [rbp + 56], 2
    mov       r8d, [rbp + 48]
    mov       r9d, [rbp + 56]

    vpbroadcastd ymm0, dword [.filter_taps + r10*4]
    vpbroadcastd ymm1, dword [.filter_taps2 + r10*4]

    xor       r12d, r12d
.row_loop_16:
    lea       r13, [rdx + r8]
    lea       r14, [r13 + r9 * rcx]

    vpmovzxbd ymm2, [r14 + r12 * rcx]
    vpmovzxbd ymm3, [r14 + r12 * rcx + 1]
    vpmovzxbd ymm4, [r14 + (r12+1) * rcx]
    vpmovzxbd ymm5, [r14 + (r12+1) * rcx + 1]

    vpmulld   ymm2, ymm2, ymm0
    vpmulld   ymm3, ymm3, ymm1
    vpaddd    ymm2, ymm2, ymm3
    vpmulld   ymm4, ymm4, ymm0
    vpmulld   ymm5, ymm5, ymm1
    vpaddd    ymm4, ymm4, ymm5

    vpbroadcastd ymm6, dword [.filter_taps + r11*4]
    vpbroadcastd ymm7, dword [.filter_taps2 + r11*4]
    vpmulld   ymm2, ymm2, ymm6
    vpmulld   ymm4, ymm4, ymm7
    vpaddd    ymm2, ymm2, ymm4

    vpcmpeqd  ymm3, ymm3, ymm3
    vpslld    ymm3, ymm3, 11
    vpaddd    ymm2, ymm2, ymm3
    vpsrad    ymm2, ymm2, 12

    vpackusdw ymm2, ymm2, ymm2
    vpackuswb ymm2, ymm2, ymm2
    vpermq    ymm2, ymm2, 0xD8

    vmovdqu   [rdi + r12 * rsi], xmm2

    inc       r12d
    cmp       r12d, 16
    jl        .row_loop_16

    pop       rbp
    ret
