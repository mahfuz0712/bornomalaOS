#include "klib.h"

void *memcpy(void *dst, const void *src, size_t n) {
    void *ret = dst;
    __asm__ volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    return ret;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s || d >= s + n) {
        return memcpy(dst, src, n);
    }
    /* overlapping, copy backwards */
    while (n--) d[n] = s[n];
    return dst;
}

void *memset(void *dst, int value, size_t n) {
    void *ret = dst;
    __asm__ volatile("rep stosb" : "+D"(dst), "+c"(n) : "a"(value) : "memory");
    return ret;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

int k_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int k_strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

char *k_strcpy(char *dst, const char *src) {
    char *p = dst;
    while ((*p++ = *src++)) {}
    return dst;
}

char *k_strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

void k_strtrim(char *s) {
    if (!s || !*s) return;
    size_t n = k_strlen(s), start = 0, end = n;
    while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) start++;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) end--;
    size_t j = 0;
    for (size_t i = start; i < end; i++) s[j++] = s[i];
    s[j] = '\0';
}

int k_tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

void mem_copy32(uint32_t *dst, const uint32_t *src, size_t count) {
    __asm__ volatile("rep movsl" : "+D"(dst), "+S"(src), "+c"(count) : : "memory");
}

void mem_fill32(uint32_t *dst, uint32_t value, size_t count) {
    __asm__ volatile("rep stosl" : "+D"(dst), "+c"(count) : "a"(value) : "memory");
}

uint32_t k_isqrt(uint32_t v) {
    uint32_t r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return r;
}
