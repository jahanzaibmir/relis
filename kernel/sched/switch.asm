section .text
global switch_to
switch_to:
    push ebx
    push esi
    push edi
    push ebp
    
    mov eax, [esp + 20]      ; prev struct
    mov [eax], esp           ; save esp into prev->esp
    
    mov eax, [esp + 24]      ; next struct
    mov esp, [eax]           ; load next->esp
    
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret

section .note.GNU-stack noalloc noexec nowrite progbits