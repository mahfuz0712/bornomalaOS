#!/bin/sh
# Boots bornomalaOS.iso in QEMU under UEFI firmware (OVMF), to verify the hybrid
# BIOS+UEFI ISO actually boots the way real UEFI-only hardware will.
set -e
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd
VARS_COPY=$(mktemp)
cp "$OVMF_VARS" "$VARS_COPY"
exec qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$VARS_COPY" \
    -cdrom bornomalaOS.iso \
    -m 256 \
    -rtc base=localtime \
    -serial stdio \
    -vga std \
    "$@"
