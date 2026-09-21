/* kernel/klib.h - tiny freestanding C library */
#ifndef BORNOMALA_KLIB_H
#define BORNOMALA_KLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Standard names: GCC may emit calls to these even with -ffreestanding
   (struct copies, large zero-initialisers), so they MUST exist. */
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int value, size_t n);
int   memcmp(const void *a, const void *b, size_t n);

/* Historical k_ names used by earlier phases. */
#define k_memcpy  memcpy
#define k_memset  memset
#define k_memcmp  memcmp

size_t k_strlen(const char *s);
int    k_strcmp(const char *a, const char *b);
int    k_strncmp(const char *a, const char *b, size_t n);
char  *k_strcpy(char *dst, const char *src);
char  *k_strncpy(char *dst, const char *src, size_t n);   /* always NUL-pads, like strncpy */
void   k_strtrim(char *s);
int    k_tolower(int c);

/* Fast 32-bit-pixel helpers used by the compositor. */
void   mem_copy32(uint32_t *dst, const uint32_t *src, size_t count);
void   mem_fill32(uint32_t *dst, uint32_t value, size_t count);

uint32_t k_isqrt(uint32_t v);   /* floor(sqrt(v)) */

#endif
