# RELIS Kernel - 64-bit Makefile
.RECIPEPREFIX := >

CC      := gcc
AS      := nasm
LD      := gcc

CFLAGS  := -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
           -fno-exceptions -fno-pie -m64 -mno-red-zone -mcmodel=kernel \
           -Wall -Wextra \
           -I . -I include -I include/asm -I arch -I arch/mm -I arch/entry -I arch/smp -I kernel -I kernel/drivers -I drivers -I drivers/block -I drivers/net/ethernet/intel/relis_nic -I mm -I fs -I net

ASFLAGS := -f elf64
LDFLAGS := -T arch/linker.ld -ffreestanding -nostdlib -m64 -no-pie -e _start -lgcc -z noexecstack

BUILD   := build
KERNEL  := $(BUILD)/relis.elf
ISO_DIR := $(BUILD)/iso
ISO     := $(BUILD)/relis.iso

KERNEL_C_SRCS := \
    init/main.c \
    kernel/kprintf.c \
    kernel/printk/printk.c \
    kernel/sched/core.c \
    kernel/sched/fair.c \
    kernel/sched/rt.c \
    kernel/sched/idle.c \
    kernel/sched/clock.c \
    kernel/sched/wait.c \
    kernel/sched/syscalls.c \
    kernel/irq/manage.c \
    kernel/entry/syscall.c \
    kernel/fork.c \
    kernel/signal.c \
    kernel/ipc.c \
    kernel/smp/percpu.c \
    arch/smp/apic.c \
    lib/string.c \
    mm/page_alloc.c \
    mm/slab.c \
    mm/memory.c \
    mm/vmalloc.c \
    mm/mmap.c \
    mm/mprotect.c \
    mm/vmscan.c \
    mm/oom_kill.c \
    arch/mm/init.c \
    arch/gdt.c \
    arch/idt.c \
    kernel/drivers/vga.c \
    kernel/drivers/serial.c \
    kernel/drivers/timer.c \
    kernel/drivers/keyboard.c \
    drivers/pci/pci.c \
    drivers/net/ethernet/intel/relis_nic/nic_main.c \
    drivers/net/ethernet/intel/relis_nic/nic_hw.c \
    drivers/net/ethernet/intel/relis_nic/nic_netdev.c \
    drivers/block/ata.c \
    fs/vfs.c \
    fs/proc/proc.c \
    fs/relisfs/relisfs.c \
    fs/exec.c \
    fs/binfmt_elf.c \
    net/net.c \
    net/core/dev.c \
    net/ipv4/ip.c \
    net/ipv4/udp.c

KERNEL_ASM_SRCS := \
    arch/entry.asm \
    arch/gdt_flush.asm \
    arch/isr_stubs.asm \
    arch/entry/syscall_entry.asm \
    arch/entry/drop_to_user.asm \
    kernel/sched/switch.asm

KERNEL_OBJS := $(patsubst %.c, $(BUILD)/%.o, $(KERNEL_C_SRCS)) \
               $(patsubst %.asm, $(BUILD)/%.o, $(KERNEL_ASM_SRCS)) \
               build/user_prog.o

.PHONY: all
all: $(KERNEL)

build/user_prog.o: user_prog.asm
>mkdir -p $(BUILD)
>nasm -f elf64 -o $(BUILD)/user_prog.o $<
>ld -m elf_x86_64 -Ttext 0x400000 -e _start -o $(BUILD)/user_prog.elf $(BUILD)/user_prog.o
>cp $(BUILD)/user_prog.elf user_prog.elf
>objcopy -I binary -O elf64-x86-64 -B i386:x86-64 user_prog.elf $@
>rm user_prog.elf

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
>echo '  multiboot2 /boot/relis.elf'         >> $(ISO_DIR)/boot/grub/grub.cfg
>echo '  set gfxpayload=1024x768x32'         >> $(ISO_DIR)/boot/grub/grub.cfg
>echo '}'                                    >> $(ISO_DIR)/boot/grub/grub.cfg
>grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null
>@echo "[iso] $(ISO)"

.PHONY: run
run: iso $(BUILD)/disk.img
>qemu-system-x86_64 -cdrom $(ISO) -m 256M -drive file=$(BUILD)/disk.img,format=raw,if=ide -serial stdio -vga std -net nic,model=e1000 -net user -vnc :0 -smp 4

.PHONY: clean
clean:
>rm -rf $(BUILD)
>@echo "[clean] done"
