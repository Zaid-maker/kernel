# Minimal x86 Kernel

[![Release](https://img.shields.io/github/v/release/Zaid-maker/kernel?label=release)](https://github.com/Zaid-maker/kernel/releases/latest)
[![Status: Alpha](https://img.shields.io/badge/status-alpha-orange)](https://github.com/Zaid-maker/kernel)
[![Build Kernel](https://img.shields.io/github/actions/workflow/status/Zaid-maker/kernel/build-kernel.yml?branch=main&label=build)](https://github.com/Zaid-maker/kernel/actions/workflows/build-kernel.yml)
[![Release Pipeline](https://img.shields.io/github/actions/workflow/status/Zaid-maker/kernel/release-kernel.yml?label=release%20pipeline)](https://github.com/Zaid-maker/kernel/actions/workflows/release-kernel.yml)
[![Deploy Website](https://img.shields.io/github/actions/workflow/status/Zaid-maker/kernel/deploy-site.yml?branch=main&label=pages)](https://github.com/Zaid-maker/kernel/actions/workflows/deploy-site.yml)

A tiny 32-bit freestanding kernel that boots with GRUB and provides a basic text terminal layer.

> [!WARNING]
> This project is **experimental alpha software**.
> Breaking changes can happen between releases, features may be incomplete or unstable,
> and it is not intended for production or critical systems.

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
- CPU exception ISRs (0-31) with fault diagnostics screen showing vector, name, error code, EIP, CS, and EFLAGS.
- PIT timer IRQ0 support with uptime display in the status bar.
- Tiny interactive shell commands: `help`, `clear`, `version`, `locks`, `uptime`.
- Kernel version string embedded at build time and shown on boot.

## Versioning

- Canonical kernel version is stored in `kernel/VERSION`.
- Local builds use `kernel/VERSION` automatically.
- Release pipeline overrides with release tag so shipped assets match the tag exactly.
- Optional manual override:
  - `make -C kernel all KERNEL_VERSION=v0.0.20260328.1`

## Project Website

- Source files are in `site/`.
- Open `site/index.html` locally to preview.
- Documentation page is `site/docs.html`.
- GitHub Pages deployment is automated by `.github/workflows/deploy-site.yml`.

## Project Layout

- `kernel/src/boot.s`: Multiboot header and assembly entrypoint.
- `kernel/src/kernel.c`: C kernel entry logic and boot demo output.
- `kernel/src/terminal.c`, `kernel/src/terminal.h`: VGA terminal driver.
- `kernel/src/print.c`, `kernel/src/print.h`: tiny printing helpers.
- `kernel/src/keyboard.c`, `kernel/src/keyboard.h`: PS/2 keyboard IRQ handling, queueing, and scancode mapping.
- `kernel/src/interrupts.c`, `kernel/src/interrupts.h`: IDT setup, PIC remap, and IRQ dispatch.
- `kernel/src/timer.c`, `kernel/src/timer.h`: PIT configuration and uptime counters.
- `kernel/src/isr.s`: interrupt service routine stubs.
- `kernel/linker.ld`: Links the kernel at 1 MiB.
- `kernel/grub/grub.cfg`: GRUB menu entry.
- `kernel/Makefile`: Build, ISO, and QEMU run targets.

## Prerequisites

Install the following tools:

- `i686-elf-gcc`, `i686-elf-as`, `i686-elf-ld`
- `grub-mkrescue`
- `xorriso`
- `qemu-system-i386`

## Build

```bash
make -C kernel all
```

## Build Bootable ISO

```bash
make -C kernel iso
```

## Run in QEMU

```bash
make -C kernel run
```

## Clean

```bash
make -C kernel clean
```
