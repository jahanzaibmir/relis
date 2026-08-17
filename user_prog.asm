; user_prog.asm
section .text
global _start
_start:
    ; sys_write(stdout, msg, 15)
    mov rax, 1          ; syscall number (1 = SYS_WRITE)
    mov rdi, 1          ; fd (1 = stdout)
    mov rsi, msg        ; buffer pointer
    mov rdx, 15         ; length ("ring3 compiled\n" = 15 bytes)
    int 0x80

    ; sys_exit(0)
    mov rax, 0          ; syscall number (0 = SYS_EXIT)
    mov rdi, 0          ; status
    int 0x80

.hang:
    pause
    jmp .hang

msg db "ring3 compiled", 10
