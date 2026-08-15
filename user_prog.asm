; user_prog.asm
; A pure assembly user-space program that makes a sys_write syscall via int 0x80.

section .text
global _start
_start:
    ; sys_write(stdout, msg, 36)
    mov rax, 0          ; syscall number (0 = SYS_WRITE)
    mov rdi, 1          ; fd (1 = stdout)
    mov rsi, msg        ; buffer pointer
    mov rdx, 36         ; length
    int 0x80            ; FIX: Use int 0x80!

    ; Spin safely
.loop:
    pause
    jmp .loop

msg db "Hello from RELIS Ring 3 (Userspace)!", 10
