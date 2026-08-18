[bits 16]
[org 0x8000]

    jmp short start
    nop
    nop
    nop
    nop
    nop
    nop

align 8
cr3_ptr:   dq 0
stack_ptr: dq 0
entry_ptr: dq 0
gdt_desc:  dw 0
           dd 0

start:
    cli
    cld

    ;  Initialize Segment Registers to 0!
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov dx, 0x3F8
    mov al, '1'
    out dx, al

.wait_cr3:
    mov eax, [cr3_ptr]
    test eax, eax
    jz .wait_cr3

    mov dx, 0x3F8
    mov al, '2'
    out dx, al

    o32 lgdt [gdt_desc]

    mov dx, 0x3F8
    mov al, '3'
    out dx, al

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov eax, [cr3_ptr]
    mov cr3, eax

    mov dx, 0x3F8
    mov al, '4'
    out dx, al

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; FIX: Enable Protected Mode AND Paging (CR0.PE | CR0.PG)
    mov eax, cr0
    or eax, 0x1 | 0x80000000
    mov cr0, eax

    mov dx, 0x3F8
    mov al, '5'
    out dx, al

    jmp 0x08:long_mode_entry

[bits 64]
long_mode_entry:
    mov dx, 0x3F8
    mov al, '6'
    out dx, al

    mov rsp, [stack_ptr]
    test rsp, rsp
    jz .halt

    mov rax, [entry_ptr]
    jmp rax

.halt:
    hlt
    jmp .halt
