; Fast frame-pointer-based stack walk (x64)
; NASM syntax
section .text
global stack_walk_x64
stack_walk_x64:
    push rbp
    mov rbp, rsp
    ; rcx = output buffer (uint64_t*)
    ; rdx = max_depth
    xor r10, r10                ; depth counter
    mov r11, [rbp]              ; current frame (FP)
.loop:
    test rdx, rdx
    jz .done
    test r11, r11
    jz .done
    mov rax, [r11 + 8]          ; return address
    mov [rcx + r10*8], rax
    inc r10
    dec rdx
    mov r11, [r11]              ; prev frame
    jmp .loop
.done:
    mov rax, r10
    pop rbp
    ret
