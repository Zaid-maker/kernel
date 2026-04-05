# Project SigmaBoot

[![Release](https://img.shields.io/github/v/release/Zaid-maker/kernel?label=release)](https://github.com/Zaid-maker/kernel/releases/latest)
[![Status: Alpha](https://img.shields.io/badge/status-alpha-orange)](https://github.com/Zaid-maker/kernel)
[![Build Kernel](https://img.shields.io/github/actions/workflow/status/Zaid-maker/kernel/build-kernel.yml?branch=main&label=build)](https://github.com/Zaid-maker/kernel/actions/workflows/build-kernel.yml)
[![Release Pipeline](https://img.shields.io/github/actions/workflow/status/Zaid-maker/kernel/release-kernel.yml?label=release%20pipeline)](https://github.com/Zaid-maker/kernel/actions/workflows/release-kernel.yml)
[![Deploy Website](https://img.shields.io/github/actions/workflow/status/Zaid-maker/kernel/deploy-site.yml?branch=main&label=pages)](https://github.com/Zaid-maker/kernel/actions/workflows/deploy-site.yml)
[![Tests: Enforced](https://img.shields.io/badge/tests-enforced-success)](https://github.com/Zaid-maker/kernel/actions/workflows/build-kernel.yml)
[![Coverage](https://codecov.io/gh/Zaid-maker/kernel/branch/main/graph/badge.svg)](https://codecov.io/gh/Zaid-maker/kernel)

SigmaBoot is a tiny 32-bit freestanding kernel that boots with GRUB and provides a basic text terminal layer.

> [!WARNING]
> This project is **experimental alpha software**.
> Breaking changes can happen between releases, features may be incomplete or unstable,
> and it is not intended for production or critical systems.

## FAQ

### Why does this project exist?

Project SigmaBoot exists as a learning-first bare metal kernel project.
The goal is to build core OS components from scratch in small, understandable steps.

### Who is this for?

Anyone learning low-level systems programming, x86 boot flow, interrupts, and early kernel architecture.

### Is this production ready?

No. This is alpha-stage experimental software and is expected to change frequently.

### Why date-style versions?

Date-style versions make milestone history easy to track and keep release progression clear during rapid iteration.

### What can I do with it today?

You can boot in QEMU, use the shell commands, inspect lock state and uptime, view PS/2 mouse state, and view the Multiboot memory map.

## Features

- Multiboot-compliant boot path with GRUB.
- VGA text-mode terminal with color support.
- Newline, tab handling, and automatic scroll when output reaches the screen bottom.
- Small decimal and hexadecimal print helpers for kernel diagnostics.
- Heap-backed decimal print formatting buffer with static fallback.
- PS/2 keyboard input with basic US scancode translation.
- Caps Lock handling for alphabetic keys and Num Lock handling for keypad digits.
- Lock key LED synchronization for Caps Lock, Num Lock, and Scroll Lock.
- Persistent bottom-row lock status bar showing CAPS/NUM/SCRL states plus live mouse IRQ activity (`M:... P:<packets>`).
- Interrupt-driven keyboard input via IRQ1 using IDT + PIC remap.
- PS/2 mouse driver with IRQ12 packet handling and basic position/button tracking.
- Heap-backed keyboard IRQ queue with static fallback when allocation is unavailable.
- CPU exception ISRs (0-31) with fault diagnostics screen showing vector, name, error code, EIP, CS, and EFLAGS.
- Heap-backed exception diagnostics workspace buffer with static fallback.
- PIT timer IRQ0 support with uptime display in the status bar.
- Tiny interactive shell commands: `help`, `clear`, `version`, `locks`, `uptime`, `memmap`, `pmm`, `heap`, `heapcheck`, `heaptriage`, `heapfrag`, `heapstress [rounds]`, `heaphist`, `heapleaks [max]`, `history`, `mouse`.
- Multiboot memory map viewer command (`memmap`) for physical layout inspection.
- Heap-backed memmap line scratch buffer with stack fallback.
- Early physical memory manager (bitmap-based frame tracking) with `pmm` shell stats command.
- Heap-backed PMM stats line buffer with stack fallback.
- Heap allocator groundwork with `kmalloc`/`kfree` plus shell diagnostics (`heap`, `heapcheck`, `heaptriage`, `heapfrag`, `heapstress`, `heaphist`, `heapleaks`).
- Heap-backed heap stats line buffer with stack fallback.
- Heap-backed dynamic shell input buffer with growth and last-command history (`history`).
- Dedicated stats utility module (`stats_util`) to consolidate shared stats and diagnostics line formatting.
- Kernel version string embedded at build time and shown on boot.

## Versioning

- Canonical kernel version is stored in `kernel/VERSION`.
- Local builds use `kernel/VERSION` automatically.
- Release pipeline overrides with release tag so shipped assets match the tag exactly.
- Optional manual override:
  - `make -C kernel all KERNEL_VERSION=v0.0.20260405`

## Project Website

- Source files are in `site/`.
- Open `site/index.html` locally to preview.
- Documentation page is `site/docs.html`.
- Architecture diagram is included in `site/docs.html`.
- GitHub Pages deployment is automated by `.github/workflows/deploy-site.yml`.

## Project Layout

- `kernel/src/boot.s`: Multiboot header and assembly entrypoint.
- `kernel/src/kernel.c`: C kernel entry logic and boot demo output.
- `kernel/src/terminal.c`, `kernel/src/terminal.h`: VGA terminal driver.
- `kernel/src/print.c`, `kernel/src/print.h`: tiny printing helpers.
- `kernel/src/stats_util.c`, `kernel/src/stats_util.h`: shared stats/diagnostics line formatting helpers.
- `kernel/src/drivers/keyboard.c`, `kernel/src/drivers/keyboard.h`: PS/2 keyboard IRQ handling, queueing, and scancode mapping.
- `kernel/src/drivers/mouse.c`, `kernel/src/drivers/mouse.h`: PS/2 mouse initialization and IRQ12 packet decoding.
- `kernel/src/interrupts.c`, `kernel/src/interrupts.h`: IDT setup, PIC remap, and IRQ dispatch.
- `kernel/src/timer.c`, `kernel/src/timer.h`: PIT configuration and uptime counters.
- `kernel/src/multiboot.h`: Multiboot data structures used for boot-time memory map parsing.
- `kernel/src/pmm.c`, `kernel/src/pmm.h`: Physical memory manager bitmap and frame stats APIs.
- `kernel/src/heap.c`, `kernel/src/heap.h`: Heap allocator (`kmalloc`/`kfree`) with integrity checks, compact triage output, fragmentation stats, live histogram telemetry, and leak-trace snapshots.
- `kernel/src/heap_diag.c`, `kernel/src/heap_diag.h`: Heap diagnostics telemetry backend (histograms, trace snapshots, counters).
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

## Run Unit Tests (Side-By-Side)

```bash
make -C kernel test
```

This runs host-side unit tests for shared formatting helpers (`src/sbuf.c`) in parallel with kernel feature work.
It also runs heap diagnostics tests (`heap_diag_test`), corruption-injection integrity tests (`heap_integrity_test`), and the PS/2 mouse driver suite (`mouse_test`).
It also runs a runtime heap allocator test (`heap_runtime_test`) against a deterministic PMM stub so `kmalloc`/`kfree` and merge/split paths contribute to coverage.
Both CI (`build-kernel.yml`) and release (`release-kernel.yml`) pipelines run this test gate before build/release assets.

## Run Coverage

```bash
make -C kernel coverage
```

This generates an LCOV report at `kernel/build/coverage/lcov.info` for host-side formatter and heap diagnostics/integrity/runtime test suites.
Current milestone: host-side coverage is at 100% for the tracked kernel utility and heap diagnostics paths.

Latest release milestone: first PS/2 mouse driver landed with IRQ12 packet decoding, a shell-visible `mouse` command, a live status-bar movement indicator, and dedicated host-side tests.

## Codecov Bundle Analysis (Ready)

- Repo includes `codecov.yml` with Bundle Analysis defaults:
  - PR comment only when bundle changes (`require_bundle_changes: true`)
  - 1Kb minimum change threshold for bundle comments
  - informational bundle status with 5% warning threshold
- To activate Bundle Analysis output, add a supported bundler plugin (Vite/Webpack/Rollup) in the frontend build path and run that build in CI.

## Quick Regression Checklist

- Boot reaches shell prompt and prints kernel version.
- `help` lists `heapcheck`, `heaptriage`, `heapfrag`, `heapstress [rounds]`, `heaphist`, and `heapleaks [max]` in the command list.
- Enter more than 128 characters in one command without crashing (dynamic input growth).
- Use backspace during long input; cursor and command state remain in sync.
- Run several commands, then `history` prints recent commands in order.
- Run more than 16 commands; `history` keeps only the most recent entries.
- Run `heapfrag` and verify largest/smallest free blocks and external fragmentation are printed.
- Run `heapcheck` and verify status is `OK` with zero integrity error counters.
- Run `heaptriage` and verify the one-line summary prints counts for scanned blocks and fault categories.
- Run `heapstress` (and `heapstress 512`) and verify stress summary counters print without hanging.
- Run `heaphist` and verify live block/byte buckets are printed.
- Run `heapleaks` (and `heapleaks 32`) and verify active allocation trace rows are printed.
- Run `make -C kernel test` and verify `heap_diag_test`, `heap_runtime_test`, `heap_integrity_test`, and `mouse_test` pass.
- Status bar continues updating lock states, mouse activity (`M:... P:...`), and uptime while typing commands.
- Keyboard input remains functional if heap queue allocation fails (fallback queue path).
- Decimal printing remains functional if print-buffer heap allocation fails (fallback path).
- `memmap` output remains readable if scratch-buffer heap allocation fails (fallback path).
- Exception diagnostics output remains readable if workspace-buffer heap allocation fails (fallback path).
- PMM stats output remains readable if stats-buffer heap allocation fails (fallback path).
- Heap stats output remains readable if stats-buffer heap allocation fails (fallback path).
- Shared formatting helpers (`sbuf` + `stats_util`) pass host-side unit tests (`make -C kernel test`).

## Clean

```bash
make -C kernel clean
```
