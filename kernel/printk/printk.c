// kernel/printk/printk.c
#include "relis/printk.h"
#include "drivers/serial.h"
#include "drivers/vga.h"
#include "relis/spinlock.h"
#include "relis/string.h"
#include "arch/io.h"
#include <stdarg.h>

static spinlock_t console_lock = SPIN_LOCK_UNLOCKED;

void console_init(void) {
    serial_init();
    terminal_init();
}

static void print_char(char c) {
    terminal_putchar(c);
    outb(0x3F8, c);
}

static void print_string(const char *s) {
    for (int i = 0; s[i]; i++) print_char(s[i]);
}

static void print_int(int n) {
    char buf[16];
    int i = 0;
    if (n == 0) { print_char('0'); return; }
    if (n < 0) { print_char('-'); n = -n; }
    while (n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while (i > 0) print_char(buf[--i]);
}

static void print_hex(uint32_t n) {
    char buf[16];
    int i = 0;
    if (n == 0) { print_string("0x0"); return; }
    while (n > 0) {
        uint32_t rem = n % 16;
        buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        n /= 16;
    }
    print_string("0x");
    while (i > 0) print_char(buf[--i]);
}

void printk(const char *fmt, ...) {
    spin_lock(&console_lock);
    
    terminal_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    print_string("[RELIS] ");
    terminal_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    va_list args;
    va_start(args, fmt);
    
    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
                case 'd': print_int(va_arg(args, int)); break;
                case 's': print_string(va_arg(args, const char*)); break;
                case 'x': print_hex(va_arg(args, uint32_t)); break;
                case 'c': print_char((char)va_arg(args, int)); break;
                case '%': print_char('%'); break;
                default: print_char('%'); print_char(fmt[i]); break;
            }
        } else {
            print_char(fmt[i]);
        }
    }
    
    va_end(args);
    
    // FIX: Add the newline at the end!
    print_char('\n');
    
    spin_unlock(&console_lock);
}