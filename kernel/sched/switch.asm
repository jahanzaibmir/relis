section .text
bits 64
global switch_to

switch_to:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov [rdi], rsp   ; prev->rsp = rsp
    
    mov rsp, [rsi]   ; rsp = next->rsp
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
