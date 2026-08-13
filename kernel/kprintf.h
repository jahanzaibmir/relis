/*
 * RELIS — kernel/kprintf.h
 * Kernel formatted print — routes to VGA/fbterm terminal output.
 */
#pragma once
#include <stdarg.h>

/*
 * kprintf — supports: %c %s %d %u %x %X %p %%
 * Does NOT support width/precision specifiers (use fixed-width strings if needed).
 */
void kprintf(const char *fmt, ...);

/* kputs — write a plain string to the terminal */
void kputs(const char *s);
