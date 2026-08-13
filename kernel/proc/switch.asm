; =============================================================================
; RELIS— kernel/proc/switch.asm
; void switch_context(cpu_state_t *old_cpu, cpu_state_t *new_cpu)
; Offsets: edi=0 esi=4 ebp=8 ebx=12 edx=16 ecx=20 eax=24 eip=28 eflags=32 esp=36
; =============================================================================

section .text
global switch_context

switch_context:
    mov  eax, [esp + 4]
    mov  edx, [esp + 8]

    mov  [eax +  0], edi
    mov  [eax +  4], esi
    mov  [eax +  8], ebp
    mov  [eax + 12], ebx

    mov  ecx, [esp]
    mov  [eax + 28], ecx

    pushf
    pop  ecx
    mov  [eax + 32], ecx

    lea  ecx, [esp + 4]
    mov  [eax + 36], ecx

    mov  edi, [edx +  0]
    mov  esi, [edx +  4]
    mov  ebp, [edx +  8]
    mov  ebx, [edx + 12]

    push dword [edx + 32]
    popf

    mov  esp, [edx + 36]
    push dword [edx + 28]
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
