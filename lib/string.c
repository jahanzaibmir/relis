#include "relis/string.h"

//These would be Memory operations

void *kmemset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) {
        *p++ = (uint8_t)c;
    }
    return s;
}

void *kmemcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

// memmove safely handles overlapping memory regions
void *kmemmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    if (s < d && s + n > d) {
        // Copy backwards to prevent corruption
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    } else {
        // Normal copy
        while (n--) {
            *d++ = *s++;
        }
    }
    return dest;
}

int kmemcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

/* ── String Length & Comparison ─────────────────────── */

size_t kstrlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

size_t kstrnlen(const char *s, size_t maxlen) {
    const char *p = s;
    while (maxlen-- && *p) p++;
    return p - s;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

/* ── String Copying & Concatenation ─────────────────── */

char *kstrcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *kstrncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n && *src) {
        *d++ = *src++;
        n--;
    }
    while (n--) {
        *d++ = '\0';
    }
    return dest;
}

// Linux-style safe string copy. Guarantees null-termination.
ssize_t kstrscpy(char *dest, const char *src, size_t count) {
    const char *original_src = src;
    if (count == 0) return 0;
    
    while (count > 1 && *src) {
        *dest++ = *src++;
        count--;
    }
    *dest = '\0';
    
    return src - original_src;
}

char *kstrcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *kstrncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (*d) d++;
    while (n-- && *src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}

/* ── String Searching ───────────────────────────────── */

char *kstrchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return (c == '\0') ? (char *)s : 0;
}

char *kstrrchr(const char *s, int c) {
    const char *last = 0;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return (c == '\0') ? (char *)s : (char *)last;
}

char *kstrstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
    }
    return 0;
}

/* ── String Parsing ─────────────────────────────────── */

// Linux-style strsep. Better than strtok because it's re-entrant.
char *kstrsep(char **stringp, const char *delim) {
    char *s = *stringp;
    if (!s) return 0;
    
    char *end = s;
    while (*end) {
        if (kstrchr(delim, *end)) {
            *end = '\0';
            *stringp = end + 1;
            return s;
        }
        end++;
    }
    
    *stringp = 0;
    return s;
}
