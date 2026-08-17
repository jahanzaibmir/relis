section .text
bits 64
global gdt_flush
global tss_flush

gdt_flush:
    lgdt [rdi]
    mov ax, 0x10       ; Kernel DS is 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    pop rax
    push qword 0x08    ; Kernel CS is 0x08
    push rax
    retfq

tss_flush:
    mov ax, 0x28       ; TSS is 0x28
    ltr ax
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
