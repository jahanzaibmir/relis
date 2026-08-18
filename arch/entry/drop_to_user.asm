[bits 64]
global drop_to_user

drop_to_user:
    ; rdi = user_rsp, rsi = user_rip
    cli                 ; Disable interrupts while building the iretq frame
    
    push 0x1B          ;  SS (User Data segment, Index 3 | RPL=3 -> 0x18 | 0x03 = 0x1B)
    push rdi           ; RSP
    push 0x202         ; RFLAGS (Interrupts ENABLED, bit 1 reserved set)
    push 0x23          ; CS (User Code segment, Index 4 | RPL=3 -> 0x20 | 0x03 = 0x23)
    push rsi           ; RIP
    
    mov ax, 0x1B       ; Load User Data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    iretq              ; Pop the frame and jump to Ring 3!
    
section .note.GNU-stack noalloc noexec nowrite progbits
