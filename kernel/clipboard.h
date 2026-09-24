/* kernel/clipboard.h - system clipboard (plain text) */
#ifndef BORNOMALA_CLIPBOARD_H
#define BORNOMALA_CLIPBOARD_H
#include <stddef.h>
#define CLIPBOARD_MAX 8192
void        clipboard_set(const char *data, size_t len);
const char *clipboard_get(size_t *len);      /* never NULL; NUL-terminated */
#endif
