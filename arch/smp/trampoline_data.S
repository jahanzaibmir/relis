.section .rodata
.global trampoline_bin
.global trampoline_end
trampoline_bin:
.incbin "build/trampoline.bin"
trampoline_end:
