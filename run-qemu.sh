#!/bin/sh
# Boots bornomalaOS.iso in QEMU.
#   -rtc base=localtime : lock-screen / top-bar clock shows your local time
#   -serial stdio       : kernel log (kprintf) appears in this terminal
#   -m 256              : the compositor needs ~2 x (width*height*4) bytes plus the heap
# Click inside the QEMU window to capture the mouse; Ctrl+Alt+G releases it.
exec qemu-system-x86_64 \
    -cdrom bornomalaOS.iso \
    -m 256 \
    -rtc base=localtime \
    -serial stdio \
    -vga std \
    "$@"
