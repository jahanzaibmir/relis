; arch/entry.asm
MB_MAGIC    equ 0xE85250D6
MB_ARCH     equ 0
MB_LEN      equ (mb_header_end - mb_header_start)
MB_CHECKSUM equ -(MB_MAGIC + MB_ARCH + MB_LEN)

section .multiboot
align 8
mb_header_start:
    dd MB_MAGIC
    dd MB_ARCH
    dd MB_LEN
    dd MB_CHECKSUM
    dw 0
    dw 0
    dd 8
mb_header_end:

section .boot.bss
align 16
global stack_top
stack_bottom:
    resb 16384
stack_top:

; DEDICATED TABLES FOR LOWER HALF
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096

; DEDICATED TABLES FOR HIGHER HALF (0xFFFFFFFF80000000)
align 4096
p3_table_high:
    resb 4096
p2_table_high:
    resb 4096

section .boot.text
bits 32
global _start
extern start_kernel

_start:
    mov esp, stack_top
    mov edi, eax
    mov esi, ebx

    ; --- Map Lower Half (0x0000000000000000) ---
    mov eax, p3_table
    or eax, 0b11
    mov [p4_table], eax

    mov eax, p2_table
    or eax, 0b11
    mov [p3_table], eax

    ; --- Map Higher Half (0xFFFFFFFF80000000) ---
    ; P4[511] MUST point to p3_table_high, or the CPU triple faults!
    mov eax, p3_table_high
    or eax, 0b11
    mov [p4_table + 511 * 8], eax

    mov eax, p2_table_high
    or eax, 0b11
    mov [p3_table_high + 510 * 8], eax

    ; --- Map 2MB Pages in Lower Half P2 (First 1GB) ---
    mov ecx, 0
.map_p2_low:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_low

    ; --- Map 2MB Pages in Higher Half P2 (First 1GB) ---
    mov ecx, 0
.map_p2_high:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [p2_table_high + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_high

    ; Load CR3 with the P4 table
    mov eax, p4_table
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

section .boot.data
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.data: equ $ - gdt64
    dq (1 << 44) | (1 << 47) | (1 << 41)
.pointer:
    dw $ - gdt64 - 1
    dd gdt64

section .boot.text
bits 64
long_mode_start:
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call start_kernel

    cli
.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits