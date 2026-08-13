# RELIS Raw Elemental Low-level Instruction System
# Root Makefile
.RECIPEPREFIX := >

CC      := gcc
AS      := nasm
LD      := gcc

# Added -I . and -I arch/x86 so includes still work correctly
CFLAGS  := -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
           -fno-exceptions -fno-pie -m32 \
           -Wall -Wextra -Wno-unused-parameter \
           -I . -I kernel -I arch/x86

ASFLAGS := -f elf32

# Updated linker script path
LDFLAGS := -T arch/x86/linker.ld \
           -ffreestanding -nostdlib -m32 -no-pie -e _start \
           -lgcc

BUILD   := build
KERNEL  := $(BUILD)/relis.elf
ISO_DIR := $(BUILD)/iso
ISO     := $(BUILD)/relis.iso

QEMU         := qemu-system-i386
QEMU_FLAGS   := -m 256M \
                -drive file=$(BUILD)/disk.img,format=raw,if=ide \
                -serial stdio \
                -vga std \
                -net nic,model=e1000 \
                -net user

QEMU_NOX     := $(QEMU_FLAGS) -nographic

# Updated paths: kernel/arch/x86 -> arch/x86
KERNEL_C_SRCS := \
    kernel/kernel.c \
    kernel/kprintf.c \
    arch/x86/gdt.c \
    arch/x86/idt.c \
    kernel/mm/pmm.c \
    kernel/mm/heap.c \
    kernel/mm/paging.c \
    kernel/drivers/vga.c \
    kernel/drivers/serial.c \
    kernel/drivers/timer.c \
    kernel/drivers/keyboard.c \
    kernel/drivers/disk/ata.c \
    kernel/drivers/net/e1000.c \
    kernel/drivers/pci/pci.c \
    kernel/fs/vfs.c \
    kernel/fs/initrd.c \
    kernel/fs/stackfs.c \
    kernel/net/net.c \
    kernel/proc/process.c \
    kernel/syscall/syscall.c

# Updated paths: kernel/arch/x86 -> arch/x86
KERNEL_ASM_SRCS := \
    arch/x86/entry.asm \
    arch/x86/gdt_flush.asm \
    arch/x86/isr_stubs.asm \
    kernel/proc/switch.asm

KERNEL_C_OBJS   := $(patsubst %.c,  $(BUILD)/%.o, $(KERNEL_C_SRCS))
KERNEL_ASM_OBJS := $(patsubst %.asm,$(BUILD)/%.o, $(KERNEL_ASM_SRCS))

ALL_OBJS := $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

.PHONY: all
all: $(KERNEL)

 $(KERNEL): $(ALL_OBJS) | $(BUILD)
>$(LD) $(LDFLAGS) -o $@ $^
>@echo "[ld]  $@"

 $(BUILD)/%.o: %.c | $(BUILD)
>@mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -c $< -o $@
>@echo "[cc]  $<"

 $(BUILD)/%.o: %.asm | $(BUILD)
>@mkdir -p $(dir $@)
>$(AS) $(ASFLAGS) $< -o $@
>@echo "[as]  $<"

 $(BUILD):
>mkdir -p $(BUILD)

 $(BUILD)/disk.img:
>dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
>@echo "[img] $@ (64 MiB blank disk)"

.PHONY: iso
iso: $(KERNEL) $(BUILD)/disk.img
>mkdir -p $(ISO_DIR)/boot/grub
>cp $(KERNEL) $(ISO_DIR)/boot/relis.elf
>echo 'set timeout=0'                        >  $(ISO_DIR)/boot/grub/grub.cfg
>echo 'set default=0'                        >> $(ISO_DIR)/boot/grub/grub.cfg
>echo 'menuentry "RELIS" {'                  >> $(ISO_DIR)/boot/grub/grub.cfg
>echo '  multiboot /boot/relis.elf'          >> $(ISO_DIR)/boot/grub/grub.cfg
>echo '  set gfxpayload=1024x768x32'         >> $(ISO_DIR)/boot/grub/grub.cfg
>echo '}'                                    >> $(ISO_DIR)/boot/grub/grub.cfg
>grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null
>@echo "[iso] $(ISO)"

.PHONY: run
run: iso $(BUILD)/disk.img
>$(QEMU) -cdrom $(ISO) $(QEMU_FLAGS)

.PHONY: run-nox
run-nox: iso $(BUILD)/disk.img
>$(QEMU) -cdrom $(ISO) $(QEMU_NOX)

.PHONY: run-elf
run-elf: $(KERNEL) $(BUILD)/disk.img
>$(QEMU) -kernel $(KERNEL) $(QEMU_FLAGS)

.PHONY: clean
clean:
>rm -rf $(BUILD)
>@echo "[clean] done"

.PHONY: help
help:
>@echo "RELIS build targets:"
>@echo "  make          build kernel ELF"
>@echo "  make iso      build bootable ISO"
>@echo "  make run      build ISO + launch QEMU"
>@echo "  make run-nox  build ISO + launch QEMU (no display)"
>@echo "  make run-elf  launch QEMU with raw ELF"
>@echo "  make clean    remove build directory"