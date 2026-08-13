/* RELIS — kernel/drivers/keyboard.h */
#pragma once
#include <stdint.h>
void keyboard_init(void);
char keyboard_getchar(void);
char keyboard_poll(void);
int  keyboard_ctrl(void);
int  keyboard_shift(void);
int  keyboard_alt(void);
int  keyboard_caps(void);
