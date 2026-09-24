/* kernel/bootmode.h - what GRUB's boot menu asked us to do (Live / Installer / Persistent) */
#ifndef BORNOMALA_BOOTMODE_H
#define BORNOMALA_BOOTMODE_H

typedef enum { BOOT_MODE_LIVE = 0, BOOT_MODE_INSTALLER, BOOT_MODE_PERSISTENT } boot_mode_t;

/* Reads "boot_mode=live|installer|persistent" out of the Multiboot2 command line.
   Defaults to BOOT_MODE_LIVE when absent or unrecognised. */
boot_mode_t boot_mode_from_cmdline(const char *cmdline);

#endif
