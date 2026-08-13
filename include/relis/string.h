#pragma once
#include <stddef.h>
#include <stdint.h>

int kstrcmp(const char *a, const char *b);
size_t kstrlen(const char *s);
void *kmemset(void *s, int c, size_t n);
char *kstrcpy(char *dest, const char *src);
