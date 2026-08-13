/*
 * RELIS — kernel/kprintf.c
 * Kernel formatted print — supports: %c %s %d %u %x %X %p %%
 */
#include "kprintf.h"
#include "drivers/vga.h"
#include <stdint.h>
#include <stddef.h>

/* Print an unsigned integer in the given base */
static void print_uint(uint32_t n, int base, int uppercase) {
    if (base < 2 || base > 16) return;
    const char *digits = uppercase ? "0123456789ABCDEF"
                                   : "0123456789abcdef";
    if (n == 0) { terminal_putchar('0'); return; }

    char buf[32];
    int  pos = 0;
    while (n) {
        buf[pos++] = digits[n % (uint32_t)base];
        n /= (uint32_t)base;
    }
    /* digits are reversed in buf — print backwards */
    while (pos-- > 0) terminal_putchar(buf[pos]);
}

static void print_int(int32_t n) {
    if (n < 0) { terminal_putchar('-'); n = -n; }
    print_uint((uint32_t)n, 10, 0);
}

void kputs(const char *s) {
    terminal_write(s);
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (size_t i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') {
            terminal_putchar(fmt[i]);
            continue;
        }

        i++;
        switch (fmt[i]) {
            case 'c':
                terminal_putchar((char)va_arg(args, int));
                break;
            case 's': {
                const char *s = va_arg(args, const char *);
                terminal_write(s ? s : "(null)");
                break;
            }
            case 'd':
                print_int(va_arg(args, int32_t));
                break;
            case 'u':
                print_uint(va_arg(args, uint32_t), 10, 0);
                break;
            case 'x':
                print_uint(va_arg(args, uint32_t), 16, 0);
                break;
            case 'X':
                print_uint(va_arg(args, uint32_t), 16, 1);
                break;
            case 'p':
                terminal_write("0x");
                print_uint((uint32_t)(uintptr_t)va_arg(args, void *), 16, 0);
                break;
            case '%':
                terminal_putchar('%');
                break;
            default:
                terminal_putchar('%');
                terminal_putchar(fmt[i]);
                break;
        }
    }

    va_end(args);
}
