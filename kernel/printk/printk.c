#include "relis/printk.h"
#include "drivers/serial.h"
#include "drivers/vga.h"
#include "relis/string.h"

void console_init(void) {
    serial_init();
    terminal_init();
}

void printk(const char *fmt, ...) {
    // For now, printk just forwards to serial and terminal.
    // In a real kernel, this parses %d, %s, etc.
    serial_write("[RELIS] ");
    serial_write(fmt);
    serial_write("\n");
    terminal_write(fmt);
    terminal_putchar('\n');
}