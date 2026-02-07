ProOS v0.0.2a
================

Summary
-------
ProOS is a small hobby 32-bit toy operating system (bootable ISO) demonstrating a basic kernel, a framebuffer-based desktop UI, a tiny ramfs, and a minimal syscall/ELF userland loader.

What works (current release)
----------------------------
- Bootable ISO: iso/myos.iso (built from this repo)
- Desktop UI: taskbar, start menu, desktop icons
- Cursor: keyboard-driven cursor (WASD to move, E to click)
- Terminal: toggleable terminal (T) and terminal icon
- File manager: kernel-space file manager window; can create folders from the GUI
- Userland: simple userland GUI daemons in `userland/*.elf` (gfxd, displayd, hello_gui)
- Syscalls: basic syscall trap (int 0x80) and framebuffer syscalls (info/ptr/swap)

Limitations / Known issues
--------------------------
- No mouse driver yet — cursor is keyboard-driven.
- Very small/placeholder font and graphics (block-based rendering).
- The framebuffer is a simple RAM buffer; no full VESA LFB mapping.
- File manager is implemented in-kernel for the MVP (not yet full userland GUI app).

Quick usage
-----------
Build everything (from WSL Ubuntu 22.04 or equivalent cross-build environment):

```bash
cd /home/dev/myos
make -j4
```

Generate and run the ISO in QEMU (headful):

```bash
qemu-system-i386 -cdrom iso/myos.iso -vga std -m 512
```

Capture serial boot log (optional):

```bash
qemu-system-i386 -cdrom iso/myos.iso -vga std -m 512 -serial file:/path/to/qemu_kernel.log
```

Controls (keyboard-driven)
-------------------------
- Move desktop cursor: `W` / `A` / `S` / `D`
- Click / select: `E`
- Toggle terminal: `T` (or click Terminal icon)
- Desktop icons (left→right): Terminal, Create Folder, File Manager

Filesystem
----------
- Simple in-memory ramfs implemented in `kernel/filesystem.c`.
- You can create folders from the desktop (Create Folder icon).
- The kernel exposes a tiny `fs_*` API for file and directory operations.

Developer notes
---------------
- Kernel source: `kernel/`
- Userland source: `userland/` (includes a small `Makefile` to build ELF binaries)
- To rebuild only userland: `cd userland && make`
- The kernel embeds `displayd.elf` (if present) into the ramfs on boot as a convenience fallback.

License
-------
This project is experimental and provided as-is. Credit me if you fork it.


