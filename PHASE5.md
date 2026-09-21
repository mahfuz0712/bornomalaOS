# BornomalaOS - Phase 5: visual GUI foundation (repaired & verified)

This is a complete, self-consistent Phase 5. It builds with **zero warnings**, passes
`grub-file --is-x86-multiboot2`, and was boot-tested in QEMU (lock screen -> desktop -> apps).

## What you get

| Layer | Files |
|-------|-------|
| Boot | `boot/boot.asm` (Multiboot2, long mode, 4 GiB identity map), `boot/isr.asm` (vectors 0-47), `boot/linker.ld` |
| Core | `klib`, `console` (VGA + serial `kprintf`), `mb2` (boot-info parser), `memory` (page allocator), `alloc` (heap) |
| Drivers | `interrupts` (IDT/PIC/PIT), `ps2`, `keyboard`, `mouse`, `input` (event queue), `rtc`, `power` |
| Accounts | `users`, `lockscreen` (logic) |
| GUI | `gui/gfx` (rasteriser), `gui/font_*` (baked Noto Sans + Spleen), `gui/icons`, `gui/compositor` (back buffer + damage rectangles), `gui/menu`, `gui/wm` (window manager), `gui/apps`, `gui/lock`, `gui/desktop`, `gui/session` |

Design source: `bornomalaos_lock_screen.html` -> `gui/lock.c`, `windows7_aero_desktop-1.html` -> `gui/desktop.c`, `gui/wm.c`, `gui/apps.c`.
(The HTML is only a *design reference*; everything is drawn by the kernel itself.)

Working features: lock screen (password `bornomala`, error state, power menu: Restart / Shut Down / Sleep),
Aero wallpaper, top bar with system menu (Lock Screen, Restart, Shut Down), desktop icons (double-click),
right-click context menu, floating Dock with hover magnification + tooltips + running dots, Launchpad with live search,
draggable / minimizable / maximizable (double-click title) / closable windows with focus + z-order,
Computer, Files, **Notepad** (real editing), **Calculator** (mouse + keyboard), toasts, sleep + wake, mouse cursor.
Shortcuts: `F4` or the Windows key = Launchpad, `Ctrl+L` = lock, `Esc` closes menus / Launchpad.

## What was broken in the ChatGPT Phase 5 (and how it is fixed)

**Build-stoppers**
1. `Makefile` had no rule for `boot/interrupts.o` -> `make` failed immediately. (Rules for `boot/%.o` added.)
2. Phase 4/5 GUI objects (`desktop.c`, `gui/*.c`) were never in `OBJS`, and `kernel.c` never called them. (Whole GUI is now linked and started.)
3. `kernel/gui/draw.c`: unbalanced parenthesis in `draw_blend_rect` = compile error.
4. `keyboard_init()` was declared and called but **never defined** = link error.
5. `make iso` copied a `grub-theme/` directory that is not in the zip. (Now optional.)
6. Two different `gui.h` / `gui.c` (`kernel/` and `kernel/gui/`) with the same include guard and overlapping names.

**Runtime bugs**
7. Phase 3's mouse driver and memory manager were silently dropped in phase 4/5 (phase 4 was built from phase 2). Both are back, rewritten.
8. Keyboard queue indices were `uint16_t` but the array had 256 entries -> out-of-bounds write after 256 keys.
9. Arrow / Home / Delete keys (`0xE0` prefix) were decoded as keypad digits.
10. Left-Shift release (`0xAA`) was discarded as "BAT complete" -> **Shift stuck** (found during this test run).
11. No CPU-exception handlers: any fault triple-faulted or looped; error-code exceptions did `iretq` on a wrong stack. (Stubs for vectors 0-47 with correct error-code handling + a diagnostic screen.)
12. ISR stub called C with a misaligned stack; no `cld`.
13. `pitch` was treated as pixels but Multiboot reports bytes -> skewed image on the framebuffer.
14. `draw_frame` filled the whole window with solid black before drawing the frame.
15. Window buttons overlapped (8 px apart, 8 px radius). Only a 5x7 font. All drawing straight to video memory (flicker).
16. Heap was hard-coded at `0x200000` (could overlap the bootloader's info block); the free-list never coalesced and refused exact-size reuse.
17. `users.c`: lookups only scanned `i < count`, so after removing a user the ones behind the hole became unreachable; ids were reused.
18. RTC: 12-hour PM bit (0x80) was BCD-decoded -> wrong hour; no weekday.
19. Multiboot header checksum overflowed a `dd` (warning), page tables relied on an implicit 4 KiB `.bss` alignment, upper register halves were not cleared after the mode switch, linker script only matched `.rodata` / `.text` (not `.rodata.*`), no `-mno-sse` guarantees.
20. The bundled 8x16 font was corrupted (glyphs unreadable). Replaced with Spleen (BSD-2) + Noto Sans (OFL), baked by `tools/bake_font.py`.
21. i8042: IRQ12 was never enabled in the controller config byte, mouse bytes could be read as keyboard bytes.

## Build & run

```sh
sudo apt install build-essential nasm grub-pc-bin xorriso mtools qemu-system-x86
make            # mykernel.bin
make check      # optional: verifies the Multiboot2 header
make iso        # bornomalaOS.iso
./run-qemu.sh   # click the window to capture the mouse, Ctrl+Alt+G to release
```

`run-qemu.sh` uses `-rtc base=localtime` so the clock shows *your* time (QEMU's default RTC is UTC).
Give the VM >= 128 MiB (the back buffer + wallpaper layer use 2 x width x height x 4 bytes).
Kernel log goes to the serial port (`-serial stdio`).

## Known limits (deliberately left for later phases)

* Text is ASCII only in the UI (the Bengali brand mark is a baked glyph). Bengali text shaping is a future task.
* Glass panels are translucent but not blurred.
* Password hashing is still the bootstrap digest in `users.c`; replace it before real accounts.
* 32-bpp direct-colour framebuffers below 4 GiB only; otherwise a text-mode login is shown.
* Files/Computer windows are visual only; the disk layer (`disk.c`) is compiled but not used yet.
