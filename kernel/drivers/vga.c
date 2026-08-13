// kernel/drivers/vga.c

#include "vga.h"
#include "io.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEM     ((volatile uint16_t *)0xB8000)

static inline uint8_t  mkcolor(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)(fg | (bg << 4));
}
static inline uint16_t mkentry(char c, uint8_t col) {
    return (uint16_t)(uint8_t)c | ((uint16_t)col << 8);
}

static size_t  row, col;
static uint8_t color;

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_init(void) {
    row = col = 0;
    color = mkcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_clear();
}

void terminal_clear(void) {
    /* Check if fbterm is active — if so, delegate */
    extern int fbterm_active(void);
    extern void fbterm_clear(void);
    if (fbterm_active()) { fbterm_clear(); return; }

    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = mkentry(' ', color);
    row = col = 0;
    update_cursor();
}

void terminal_setcolor(vga_color_t fg, vga_color_t bg) {
    extern int fbterm_active(void);
    extern void fbterm_setcolor(uint32_t fg2, uint32_t bg2);
    if (fbterm_active()) {
        /* Map VGA colours to RGB for fbterm */
        static const uint32_t vga_rgb[16] = {
            0x0E0E12, 0x185AB4, 0x10A020, 0x10A0A0,
            0xC02020, 0xA020A0, 0xA05010, 0xD0D0D0,
            0x505060, 0x4A9EFF, 0x40C070, 0x40C0C0,
            0xFF4A4A, 0xFF40FF, 0xFFB030, 0xF0F0F0,
        };
        fbterm_setcolor(vga_rgb[fg & 0xF], vga_rgb[bg & 0xF]);
        return;
    }
    color = mkcolor(fg, bg);
}

static void vga_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT-1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y*VGA_WIDTH+x] = VGA_MEM[(y+1)*VGA_WIDTH+x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT-1)*VGA_WIDTH+x] = mkentry(' ', color);
    row = VGA_HEIGHT-1;
}

void terminal_putchar(char c) {
    extern int fbterm_active(void);
    extern void fbterm_putchar(char c2);
    if (fbterm_active()) { fbterm_putchar(c); return; }

    if (c == '\n') {
        col = 0;
        if (++row == VGA_HEIGHT) vga_scroll();
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\t') {
        col = (col + 8) & ~7u;
        if (col >= VGA_WIDTH) { col = 0; if (++row == VGA_HEIGHT) vga_scroll(); }
    } else if (c == '\b') {
        if (col > 0) { col--; VGA_MEM[row*VGA_WIDTH+col] = mkentry(' ', color); }
    } else {
        VGA_MEM[row*VGA_WIDTH+col] = mkentry(c, color);
        if (++col == VGA_WIDTH) { col = 0; if (++row == VGA_HEIGHT) vga_scroll(); }
    }
    update_cursor();
}

void terminal_write(const char *s) {
    for (size_t i = 0; s[i]; i++) terminal_putchar(s[i]);
}

void terminal_writeline(const char *s) {
    terminal_write(s); terminal_putchar('\n');
}

/* fbterm dummy stubs to satisfy linking until fbterm is implemented */
int fbterm_active(void) {
    return 0;
}

void fbterm_clear(void) {}

void fbterm_setcolor(uint32_t fg, uint32_t bg) {
    (void)fg; (void)bg;
}

void fbterm_putchar(char c) {
    (void)c;
}