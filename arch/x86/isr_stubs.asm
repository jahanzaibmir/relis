; RELIS — kernel/arch/x86/isr_stubs.asm
;
; CPU exception stubs (ISR 0-31), hardware IRQ stubs (IRQ 0-15 → vectors 32-47),
; and the system call gate (int 0x80 → vector 128).
;
; All stubs save the full CPU state into a registers_t struct on the stack,
; switch to kernel data segments, call the C handler, then restore and iret.
;
; registers_t layout (from idt.h, low → high address):
;   ds, edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax,
;   int_no, err_code, eip, cs, eflags, [useresp, ss if ring-3]

section .text

global idt_load
extern isr_handler
extern irq_handler
extern syscall_dispatch

; ── Load IDT register — called from idt_init() ───────────────────────────
idt_load:
    mov  eax, [esp + 4]
    lidt [eax]
    ret

; ── ISR stubs — CPU exceptions ───────────────────────────────────────────
; Some exceptions push an error code; for those that don't we push 0 so
; the stack layout is always uniform.

%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0        ; dummy error code
    push dword %1       ; interrupt number
    jmp  isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1       ; interrupt number (CPU already pushed error code)
    jmp  isr_common
%endmacro

ISR_NOERR  0    ; #DE  divide by zero
ISR_NOERR  1    ; #DB  debug
ISR_NOERR  2    ;      NMI
ISR_NOERR  3    ; #BP  breakpoint
ISR_NOERR  4    ; #OF  overflow
ISR_NOERR  5    ; #BR  bound range exceeded
ISR_NOERR  6    ; #UD  invalid opcode
ISR_NOERR  7    ; #NM  device not available
ISR_ERR    8    ; #DF  double fault          (error code = 0)
ISR_NOERR  9    ;      coprocessor segment overrun
ISR_ERR   10    ; #TS  invalid TSS
ISR_ERR   11    ; #NP  segment not present
ISR_ERR   12    ; #SS  stack fault
ISR_ERR   13    ; #GP  general protection fault
ISR_ERR   14    ; #PF  page fault
ISR_NOERR 15    ;      reserved
ISR_NOERR 16    ; #MF  x87 FPU error
ISR_ERR   17    ; #AC  alignment check
ISR_NOERR 18    ; #MC  machine check
ISR_NOERR 19    ; #XF  SIMD floating-point
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ── Common exception handler trampoline ──────────────────────────────────
; Stack on entry: [ss, useresp], eflags, cs, eip, err_code, int_no
isr_common:
    pusha                   ; edi esi ebp esp ebx edx ecx eax

    mov  ax, ds
    push eax                ; save current DS

    mov  ax, 0x10           ; kernel data segment
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    push esp                ; arg: pointer to registers_t on stack
    call isr_handler
    add  esp, 4

    pop  eax                ; restore DS
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa
    add  esp, 8             ; discard int_no and err_code
    sti
    iret

; ── IRQ stubs — hardware interrupts ──────────────────────────────────────
; IRQs 0-7  → PIC1 → vectors 32-39
; IRQs 8-15 → PIC2 → vectors 40-47

%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0            ; no error code for IRQs
    push dword %2           ; vector number
    jmp  irq_common
%endmacro

IRQ  0, 32    ; PIT timer
IRQ  1, 33    ; PS/2 keyboard
IRQ  2, 34    ; PIC2 cascade
IRQ  3, 35    ; COM2
IRQ  4, 36    ; COM1
IRQ  5, 37    ; LPT2
IRQ  6, 38    ; floppy disk
IRQ  7, 39    ; LPT1 / spurious
IRQ  8, 40    ; RTC
IRQ  9, 41    ; ACPI / free
IRQ 10, 42    ; free
IRQ 11, 43    ; free
IRQ 12, 44    ; PS/2 mouse
IRQ 13, 45    ; FPU / coprocessor
IRQ 14, 46    ; primary ATA
IRQ 15, 47    ; secondary ATA

; ── Common IRQ handler trampoline ────────────────────────────────────────
irq_common:
    pusha

    mov  ax, ds
    push eax

    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    push esp
    call irq_handler
    add  esp, 4

    pop  eax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa
    add  esp, 8
    sti
    iret

; ── System call gate — int 0x80 ───────────────────────────────────────────
; User programs invoke 'int 0x80'; we route to syscall_dispatch in C.
global isr128
isr128:
    cli
    push dword 0            ; dummy error code
    push dword 128          ; vector number

    pusha
    mov  ax, ds
    push eax

    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    push esp
    call syscall_dispatch
    add  esp, 4

    pop  eax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa
    add  esp, 8
    sti
    iret

; Mark stack as non-executable (required by some linkers / security scanners)
section .note.GNU-stack noalloc noexec nowrite progbits
