/*
 * kernel/kprintf.h
 * 
 * This file defines the core kernel logging utilities Because standard C library 
 * functions like printf are unavailable in kernel space, these routines provide 
 * a way to display diagnostic messages. All outputs are processed and routed 
 * directly to the low level VGA text mode buffer or the framebuffer terminal.
 */

#pragma once

#include <stdarg.h>

/*
 * This function prints a formatted string to the active system console. 
 * It behaves similarly to the standard library printf but is optimized for 
 * minimum memory footprint and kernel stability. 
 *
 * Supported format specifiers:
 *   %c   prints a single ASCII character
 *   %s   prints a null-terminated string
 *   %d   prints a signed 32-bit decimal integer
 *   %u   prints an unsigned 32-bit decimal integer
 *   %x   prints an unsigned 32-bit integer in lowercase hexadecimal
 *   %X   prints an unsigned 32-bit integer in uppercase hexadecimal
 *   %p   prints a pointer address prefixed with 0x
 *   %%   prints a literal percent sign
 *
 * Important limitation:
 *   This implementation lacks a full parser. It does not support advanced 
 *   formatting flags, field width, or precision specifiers. If you need padding 
 *   or specific layouts, you must format the text manually before printing.
 */
void kprintf(const char *fmt, ...);

/* 
 * This function writes a basic unformatted, null terminated string to the 
 * terminal screen. It is much faster and lighter than kprintf because it completely 
 * skips the token parsing phase. Use this when you only need to output plain text.
 */
void kputs(const char *s);
