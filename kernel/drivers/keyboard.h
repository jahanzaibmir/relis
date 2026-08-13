#pragma once
#include <stdint.h>

#define KB_BUF_SIZE 256

void keyboard_init(void);
char keyboard_getchar(void);
char keyboard_poll(void);
int  keyboard_ctrl(void);
int  keyboard_shift(void);
int  keyboard_alt(void);
int  keyboard_caps(void);