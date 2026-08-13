#include "serial.h"
#include "io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);  /* disable interrupts        */
    outb(COM1 + 3, 0x80);  /* enable DLAB               */
    outb(COM1 + 0, 0x03);  /* 38400 baud low byte       */
    outb(COM1 + 1, 0x00);  /* 38400 baud high byte      */
    outb(COM1 + 3, 0x03);  /* 8 bits, no parity, 1 stop */
    outb(COM1 + 2, 0xC7);  /* enable FIFO               */
    outb(COM1 + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

static void serial_wait(void) {
    while (!(inb(COM1 + 5) & 0x20));
}

void serial_putchar(char c) {
    serial_wait();
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    while (*s) { serial_putchar(*s++); }
}
