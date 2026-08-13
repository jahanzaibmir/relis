#include "drivers/vga.h"
#include "drivers/keyboard.h"

static int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

void shell_run(void) {
    terminal_writeline("\nWelcome :)!\n");
    
    char cmd_buf[256];
    int buf_idx = 0;

    while (1) {
        terminal_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_write("relis> ");
        terminal_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        buf_idx = 0;
        while (1) {
            char c = keyboard_getchar();

            if (c == '\n') {
                terminal_putchar('\n');
                cmd_buf[buf_idx] = '\0';
                
                if (buf_idx > 0) {
                    if (kstrcmp(cmd_buf, "help") == 0) {
                        terminal_writeline("Commands: help, clear, echo");
                    } else if (kstrcmp(cmd_buf, "clear") == 0) {
                        terminal_clear();
                    } else {
                        terminal_write("Unknown command: ");
                        terminal_writeline(cmd_buf);
                    }
                }
                break; // Break inner loop to draw prompt again
            } else if (c == '\b') {
                if (buf_idx > 0) {
                    buf_idx--;
                    terminal_putchar('\b');
                }
            } else if (c >= 32 && c < 127) {
                if (buf_idx < 255) {
                    cmd_buf[buf_idx++] = c;
                    terminal_putchar(c);
                }
            }
        }
    }
}