# RELIS Kernel - Linux-style Makefile
.RECIPEPREFIX := >

CC      := gcc
AS      := nasm
LD      := gcc

CFLAGS  := -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
           -fno-exceptions -fno-pie -m32 -Wall -Wextra \
           -I . -I include -I arch/x86 -I kernel -I kernel/drivers -I drivers -I mm -I kernel/mm -I fs -I kernel/fs

ASFLAGS := -f elf32
LDFLAGS := -T arch/x86/linker.ld -ffreestanding -nostdlib -m32 -no-pie -e _start -lgcc

BUILD   := build
KERNEL  := $(BUILD)/relis.elf
ISO_DIR := $(BUILD)/iso
ISO     := $(BUILD)/relis.iso

KERNEL_C_SRCS := \
    init/main.c \
    kernel/printk/printk.c \
    kernel/sched/core.c \
    kernel/irq/manage.c \
    kernel/entry/syscall.c \
    lib/string.c \
    mm/page_alloc.c \
    mm/slab.c \
    arch/x86/gdt.c \
    arch/x86/idt.c \
    kernel/drivers/vga.c \
    kernel/drivers/serial.c \
    kernel/drivers/timer.c \
    kernel/drivers/keyboard.c

KERNEL_ASM_SRCS := \
    arch/x86/entry.asm \
    arch/x86/gdt_flush.asm \
    arch/x86/isr_stubs.asm \
    kernel/sched/switch.asm

KERNEL_OBJS := $(patsubst %.c, $(BUILD)/%.o, $(KERNEL_C_SRCS)) \
               $(patsubst %.asm, $(BUILD)/%.o, $(KERNEL_ASM_SRCS))

.PHONY: all
all: $(KERNEL)

 $(KERNEL): $(KERNEL_OBJS) | $(BUILD)
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
>qemu-system-i386 -cdrom $(ISO) -m 256M -drive file=$(BUILD)/disk.img,format=raw,if=ide -serial stdio -vga std -net nic,model=e1000 -net user

.PHONY: clean
clean:
>rm -rf $(BUILD)
>@echo "[clean] done"