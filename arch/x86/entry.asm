; RELIS Raw Elemental Low-level Instruction System
; arch/x86/entry.asm
; Multiboot1 header and entry point

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384     ; 16 KB initial kernel stack
stack_top:

section .text
global _start
extern start_kernel

_start:
    mov esp, stack_top
    mov ebp, stack_top

    ; Push multiboot info pointer (ebx) and magic (eax) to stack
    ; This matches the signature: void start_kernel(uint32_t mb_magic, multiboot_info_t *mb_info)
    push ebx
    push eax

    call start_kernel

    ; If the kernel ever returns, halt the CPU completely
    cli
.hang:
    hlt
    jmp .hang

; Mark stack as non-executable to satisfy modern linkers
section .note.GNU-stack noalloc noexec nowrite progbits