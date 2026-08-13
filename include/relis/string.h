#pragma once
#include <stddef.h>
#include <stdint.h>
#include "relis/types.h"

/* ── Memory Operations ──────────────────────────────── */
void  *kmemset(void *s, int c, size_t n);
void  *kmemcpy(void *dest, const void *src, size_t n);
void  *kmemmove(void *dest, const void *src, size_t n);
int    kmemcmp(const void *s1, const void *s2, size_t n);

/* ── String Length & Comparison ─────────────────────── */
size_t kstrlen(const char *s);
size_t kstrnlen(const char *s, size_t maxlen);
int    kstrcmp(const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);

/* ── String Copying & Concatenation ─────────────────── */
char  *kstrcpy(char *dest, const char *src);
char  *kstrncpy(char *dest, const char *src, size_t n);
ssize_t kstrscpy(char *dest, const char *src, size_t count);
char  *kstrcat(char *dest, const char *src);
char  *kstrncat(char *dest, const char *src, size_t n);

/* ── String Searching ───────────────────────────────── */
char  *kstrchr(const char *s, int c);
char  *kstrrchr(const char *s, int c);
char  *kstrstr(const char *haystack, const char *needle);

/* ── String Parsing ─────────────────────────────────── */
char  *kstrsep(char **stringp, const char *delim);