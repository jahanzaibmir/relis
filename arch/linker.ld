ENTRY(_start)

SECTIONS {
    . = 1M;

    .boot :
    {
        KEEP(*(.multiboot))
        *(.boot.text)
        *(.boot.data)
        *(.boot.rodata)
        *(.boot.bss)
    }

    /* Jump to higher half virtually, but keep physical LMA continuing from .boot */
    . += 0xFFFFFFFF80000000;

    .text ALIGN(4096) : AT(ADDR(.text) - 0xFFFFFFFF80000000)
    {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - 0xFFFFFFFF80000000) {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) : AT(ADDR(.data) - 0xFFFFFFFF80000000) {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4096) : AT(ADDR(.bss) - 0xFFFFFFFF80000000) {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    . = ALIGN(4096);
    PROVIDE(kernel_end = .);
}