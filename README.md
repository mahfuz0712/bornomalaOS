# BornomalaOS

64-bit x86_64 hobby OS written in C + NASM. **Current milestone: Phase 5** (see `PHASE5.md` for the full list of
features and the bug-fix log). Older prototype code lives in `legacy/` and is not part of the active build.

```
boot/       Multiboot2 entry, ISR stubs, linker script
kernel/     core, drivers, accounts        kernel/gui/   compositor, window manager, lock screen, desktop
tools/      bake_font.py + source fonts    third_party/  font licences (OFL, BSD-2)
iso/        GRUB config
```

Quick start: `make iso && ./run-qemu.sh`  -  login password: `bornomala`.
