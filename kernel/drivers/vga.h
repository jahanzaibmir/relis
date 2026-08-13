/* RELIS — kernel/drivers/vga.h */
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef enum {
    VGA_COLOR_BLACK=0,VGA_COLOR_BLUE=1,VGA_COLOR_GREEN=2,VGA_COLOR_CYAN=3,
    VGA_COLOR_RED=4,VGA_COLOR_MAGENTA=5,VGA_COLOR_BROWN=6,VGA_COLOR_LIGHT_GREY=7,
    VGA_COLOR_DARK_GREY=8,VGA_COLOR_LIGHT_BLUE=9,VGA_COLOR_LIGHT_GREEN=10,
    VGA_COLOR_LIGHT_CYAN=11,VGA_COLOR_LIGHT_RED=12,VGA_COLOR_LIGHT_MAGENTA=13,
    VGA_COLOR_LIGHT_BROWN=14,VGA_COLOR_WHITE=15,
} vga_color_t;
void terminal_init(void);
void terminal_setcolor(vga_color_t fg,vga_color_t bg);
void terminal_putchar(char c);
void terminal_write(const char *str);
void terminal_writeline(const char *str);
void terminal_clear(void);
void terminal_set_cursor(int x,int y);
