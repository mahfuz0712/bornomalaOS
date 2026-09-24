#include "clipboard.h"
#include "klib.h"
static char   buf[CLIPBOARD_MAX + 1];
static size_t used;
void clipboard_set(const char *data, size_t len) {
    if (len > CLIPBOARD_MAX) len = CLIPBOARD_MAX;
    memcpy(buf, data, len);
    buf[len] = '\0';
    used = len;
}
const char *clipboard_get(size_t *len) { if (len) *len = used; return buf; }
