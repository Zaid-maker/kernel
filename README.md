# Minimal x86 Kernel

A tiny 32-bit freestanding kernel that boots with GRUB and provides a basic text terminal layer.

## Features

- Multiboot-compliant boot path with GRUB.
- VGA text-mode terminal with color support.
- Newline, tab handling, and automatic scroll when output reaches the screen bottom.
- Small decimal and hexadecimal print helpers for kernel diagnostics.
- PS/2 keyboard input with basic US scancode translation.
- Caps Lock handling for alphabetic keys and Num Lock handling for keypad digits.
- Lock key LED synchronization for Caps Lock, Num Lock, and Scroll Lock.
- Persistent bottom-row lock status bar showing CAPS/NUM/SCRL states.
- Interrupt-driven keyboard input via IRQ1 using IDT + PIC remap.

## Project Website

- Source files are in `site/`.
- Open `site/index.html` locally to preview.
- Documentation page is `site/docs.html`.
- GitHub Pages deployment is automated by `.github/workflows/deploy-site.yml`.

## Project Layout

- `src/boot.s`: Multiboot header and assembly entrypoint.
- `src/kernel.c`: C kernel entry logic and boot demo output.
- `src/terminal.c`, `src/terminal.h`: VGA terminal driver.
- `src/print.c`, `src/print.h`: tiny printing helpers.
- `src/keyboard.c`, `src/keyboard.h`: PS/2 keyboard IRQ handling, queueing, and scancode mapping.
- `src/interrupts.c`, `src/interrupts.h`: IDT setup, PIC remap, and IRQ dispatch.
- `src/isr.s`: interrupt service routine stubs.
- `linker.ld`: Links the kernel at 1 MiB.
- `grub/grub.cfg`: GRUB menu entry.
- `Makefile`: Build, ISO, and QEMU run targets.

## Prerequisites

Install the following tools:

- `i686-elf-gcc`, `i686-elf-as`, `i686-elf-ld`
- `grub-mkrescue`
- `xorriso`
- `qemu-system-i386`

## Build

```bash
make all
```

## Build Bootable ISO

```bash
make iso
```

## Run in QEMU

```bash
make run
```

## Clean

```bash
make clean
```
