[bits 64]
global syscall_entry
extern syscall_dispatch
extern tss_rsp0

section .data
user_rsp:    dq 0

section .text
syscall_entry:
    mov [rel user_rsp], rsp
    mov rsp, [rel tss_rsp0]   ; Use the current task's kernel stack
    
    push r11             ; RFLAGS
    push rcx             ; RIP
    push r9              ; arg6
    push r8              ; arg5
    push r10             ; arg4
    push rdx             ; arg3
    push rsi             ; arg2
    push rdi             ; arg1
    push rax             ; syscall number
    
    mov rdi, rax
    mov rsi, [rsp+8]     ; a1 (rdi)
    mov rdx, [rsp+16]    ; a2 (rsi)
    mov rcx, [rsp+24]    ; a3 (rdx)
    mov r8,  [rsp+32]    ; a4 (r10)
    mov r9,  [rsp+40]    ; a5 (r8)
    
    sub rsp, 8           ; ALIGN STACK TO 16 BYTES (CRITICAL FOR GCC)
    call syscall_dispatch
    add rsp, 8
    
    mov [rsp], rax
    
    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9
    pop rcx              ; RIP
    pop r11              ; RFLAGS
    
    mov rsp, [rel user_rsp]
    sysret
    
section .note.GNU-stack noalloc noexec nowrite progbits
