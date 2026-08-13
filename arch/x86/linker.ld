/*
 * RELIS — kernel/arch/x86/linker.ld
 *
 * Loads the kernel at the 1 MiB physical mark (0x100000), which is the
 * standard protected-mode load address for Multiboot-compliant kernels.
 * The multiboot header MUST be in the first 8 KiB of the binary.
 *
 * kernel_end is exported to C so the PMM knows where free physical RAM starts.
 */

ENTRY(_start)

SECTIONS {
    . = 0x00100000;         /* 1 MiB load address */

    /* Multiboot header — linker places it first, before .text */
    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    /* Executable code */
    .text ALIGN(4096) : {
        *(.text)
        *(.text.*)
    }

    /* Read-only data: string literals, const arrays, vtables */
    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialised read-write data */
    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }

    /* Zero-initialised data (BSS) — no space in binary, zeroed at load time */
    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* kernel_end marks the first byte after the kernel image.
     * pmm_init() uses this to know where free physical memory begins. */
    . = ALIGN(4096);
    PROVIDE(kernel_end = .);
}
