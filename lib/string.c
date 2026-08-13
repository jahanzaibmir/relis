#include "relis/string.h"

int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

size_t kstrlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *kstrcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

void *kmemset(void *s, int c, size_t n) {
    uint8_t *p = s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}