; RELIS — kernel/arch/x86/gdt_flush.asm
; void gdt_flush(uint32_t gdtr_ptr)
;
; Loads the GDTR from the address passed in and reloads all segment
; registers. CS cannot be set by MOV so we use a far jump to force it.
;
; Segment selectors after flush:
;   0x08 = kernel code  (GDT entry 1)
;   0x10 = kernel data  (GDT entry 2)

section .text
global gdt_flush

gdt_flush:
    mov  eax, [esp + 4]     ; gdtr_ptr argument from C
    lgdt [eax]              ; load GDTR

    ; reload all data segment registers
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; far jump is the only way to reload CS
    jmp  0x08:.reload_cs
.reload_cs:
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
