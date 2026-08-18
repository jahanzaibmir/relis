// kernel/drivers/keyboard.c
#include "keyboard.h"
#include "relis/irq.h"
#include "arch/io.h"       
#include "relis/printk.h"
#include "relis/spinlock.h"
#include <stdint.h>

#define KB_DATA 0x60
#define KB_CMD  0x64

static const char normal[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6',
    '7',  '8', '9', '0', '-', '=', '\b','\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o',  'p', '[', ']', '\n', 0,  'a', 's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`',  0,  '\\','z', 'x', 'c', 'v',
    'b',  'n', 'm', ',', '.', '/',  0,  '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,  '7',
    '8',  '9', '-', '4', '5', '6', '+', '1',
    '2',  '3', '0', '.',  0,   0,   0,   0,
};

static const char shifted[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^',
    '&',  '*', '(', ')', '_', '+', '\b','\t',
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O',  'P', '{', '}', '\n', 0,  'A', 'S',
    'D',  'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"',  '~',  0,  '|', 'Z', 'X', 'C', 'V',
    'B',  'N', 'M', '<', '>', '?',  0,  '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,  '7',
    '8',  '9', '-', '4', '5', '6', '+', '1',
    '2',  '3', '0', '.',  0,   0,   0,   0,
};

static volatile char buf[KB_BUF_SIZE];
static volatile uint32_t rp = 0, wp = 0;
static volatile int shift = 0, ctrl = 0, alt = 0;
static volatile int caps = 0, num = 0, scroll = 0;
static volatile int extended = 0;

static spinlock_t kb_lock = SPIN_LOCK_UNLOCKED;

static void push(char c) {
    uint32_t next = (wp + 1) % KB_BUF_SIZE;
    if (next != rp) { // Prevent buffer overflow
        buf[wp] = c;
        wp = next;
    }
}

static void update_leds(void) {
    uint32_t timeout;

    // Wait for input buffer to be empty (with timeout to prevent kernel hang)
    timeout = 100000;
    while ((inb(KB_CMD) & 0x02) && --timeout);
    if (!timeout) return;

    outb(KB_DATA, 0xED); // "Set LEDs" command

    timeout = 100000;
    while ((inb(KB_CMD) & 0x02) && --timeout);
    if (!timeout) return;

    // bit0 = scroll, bit1 = num, bit2 = caps
    outb(KB_DATA, (uint8_t)((scroll & 1) | ((num & 1) << 1) | ((caps & 1) << 2)));
}

static int is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static void kb_irq(struct registers *regs) {
    (void)regs;
    uint8_t sc = inb(KB_DATA);

    // Extended key prefix (Arrow keys, etc.)
    if (sc == 0xE0) {
        extended = 1;
        return;
    }

    if (extended) {
        extended = 0;
        if (sc & 0x80) return; // Ignore release of extended keys for now
        // Push a 0xE0 marker so shells can parse arrow keys later
        push((char)0xE0);
        push((char)sc);
        return;
    }

    // Key release
    if (sc & 0x80) {
        uint8_t r = sc & 0x7F;
        if (r == 0x2A || r == 0x36) shift = 0;
        if (r == 0x1D)              ctrl  = 0;
        if (r == 0x38)              alt   = 0;
        return;
    }

    // Key press
    switch (sc) {
        case 0x2A: case 0x36: shift  = 1; return;
        case 0x1D:             ctrl   = 1; return;
        case 0x38:             alt    = 1; return;
        case 0x3A: caps   ^= 1; update_leds(); return;
        case 0x45: num    ^= 1; update_leds(); return;
        case 0x46: scroll ^= 1; update_leds(); return;
    }

    // Ctrl combos 
    if (ctrl) {
        if (sc == 0x2E) { push(0x03); return; }
        if (sc == 0x26) { push(0x0C); return; }
    }

    if (sc >= 128) return;
    char c = shift ? shifted[sc] : normal[sc];
    if (!c) return;

    // Apply Caps Lock to letters
    if (caps && is_letter(c)) {
        if (shift) { if (c >= 'A' && c <= 'Z') c += 32; }
        else       { if (c >= 'a' && c <= 'z') c -= 32; }
    }

    push(c);
}

void keyboard_init(void) {
    spin_lock(&kb_lock);
    rp = wp = 0;
    shift = ctrl = alt = 0;
    caps = num = scroll = 0;
    spin_unlock(&kb_lock);

    request_irq(33, kb_irq); // IRQ1 is vector 33
    printk("Keyboard initialized (Scancode Set 1, Buffer=%d)", KB_BUF_SIZE);
}

char keyboard_poll(void) {
    char c = 0;
    spin_lock(&kb_lock);
    if (rp != wp) {
        c = buf[rp];
        rp = (rp + 1) % KB_BUF_SIZE;
    }
    spin_unlock(&kb_lock);
    return c;
}

char keyboard_getchar(void) {
    char c;
    // Yield CPU while waiting for input (prevents locking up the system)
    while (!(c = keyboard_poll())) {
        __asm__ volatile("hlt");
    }
    return c;
}

int keyboard_ctrl(void)  { return ctrl;  }
int keyboard_shift(void) { return shift; }
int keyboard_alt(void)   { return alt;   }
int keyboard_caps(void)  { return caps;  }
