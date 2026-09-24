#include "bootmode.h"
#include "klib.h"

static bool has_token(const char *cmdline, const char *token) {
    if (!cmdline) return false;
    size_t n = k_strlen(token);
    for (const char *p = cmdline; *p; p++)
        if (k_strncmp(p, token, n) == 0) return true;
    return false;
}

boot_mode_t boot_mode_from_cmdline(const char *cmdline) {
    if (has_token(cmdline, "boot_mode=installer")) return BOOT_MODE_INSTALLER;
    if (has_token(cmdline, "boot_mode=persistent")) return BOOT_MODE_PERSISTENT;
    return BOOT_MODE_LIVE;
}
