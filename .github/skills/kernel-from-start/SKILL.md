---
name: kernel-from-start
description: 'Build and evolve a minimal x86 kernel from scratch with iterative milestones: scaffold, terminal/keyboard features, IRQ migration, CI + release assets, and website/docs. Use when creating or extending this kernel project end-to-end with validation at each stage.'
argument-hint: 'What milestone or target should be delivered next?'
---

# Kernel From Start Workflow

## What This Skill Produces
A repeatable workflow to take this kernel project from an empty repo to a releasable build with:
- Bootable kernel + ISO
- Iterative feature milestones
- Side-by-side host unit tests for shared runtime helpers
- CI and release artifact automation
- Project website and docs
- Validation and regression checks at each step

## When To Use
Use this skill when you need to:
- Bootstrap the kernel project from scratch
- Add a next milestone feature safely
- Troubleshoot build/release/site pipeline regressions
- Keep code, docs, and automation in sync

## Inputs
- Requested milestone (example: "keyboard LEDs", "IRQ1", "docs page")
- Release tag (date style only)
- Repository URL for hardcoded links in site content

## Scope
- Workspace skill only (`.github/skills/kernel-from-start/`).
- Do not suggest or create personal-scope copies unless explicitly requested.

## Workflow
1. Baseline discovery
- Inspect workspace tree and existing files.
- Identify missing foundation pieces (source, linker, GRUB config, Makefile, README).
- Decide whether this is bootstrap mode (empty/minimal) or extension mode (existing code).

2. Implement smallest complete milestone
- Add only the files needed for a working slice.
- Keep modules separate (boot, terminal, keyboard, interrupts, printing).
- Preserve existing APIs unless the milestone requires change.

3. Wire build and run path
- Ensure Makefile source lists match new files.
- Keep targets for binary, ISO, and emulator run.
- If toolchain mismatch appears, prefer low-risk compatibility changes.

4. Validate immediately after edits
- Run static diagnostics on all touched files.
- Run side-by-side host tests via `make -C kernel test` for shared modules (for example formatter helpers).
- Resolve introduced errors before proceeding.
- If runtime/compile tools are unavailable in environment, report that limitation explicitly and provide exact commands to run locally/CI.

5. Update docs with every milestone
- Keep README feature list and file map current.
- Explain usage changes (for example: "do not extract ISO to boot").
- For website work, keep landing + docs pages aligned with code reality.

6. Automate distribution
- CI workflow: run `make -C kernel test` before kernel build and ISO steps.
- CI workflow: build and upload artifacts (`kernel.bin`, `kernel.elf`, `kernel.iso`, checksums).
- Release workflow: run `make -C kernel test` before rebuilding release assets.
- Release workflow: rebuild and attach assets to published releases.
- Pages workflow: deploy static site and docs.

7. Release hygiene
- Enforce date-style tags only: `v0.0.YYYYMMDD` (or `.N` for same-day follow-up releases).
- Confirm release asset behavior (boot ISO as image, not extracted files).
- Keep release links hardwired when requested.

## Decision Points
- If workspace is empty:
  - Create scaffold first, then feature milestones.
- If build fails in CI due to toolchain syntax differences:
  - Apply compatibility fix in source (example: assembler mnemonic support), then revalidate.
- If GitHub Pages reports not enabled:
  - Enable Pages through workflow configuration or instruct one-time manual enablement.
- If user wants date tags:
  - Use `v0.0.YYYYMMDD` (or `.N` suffix for multiple same-day releases).

## Quality Gates (Done Criteria)
A milestone is complete only when all are true:
- Code changes compile conceptually and diagnostics show no new errors in touched files.
- Side-by-side host tests pass (or an explicit blocker/skip reason is documented).
- Build/release/site workflows reflect current project structure.
- README and site/docs are updated to match behavior.
- User-facing commands and links are explicit and correct.
- Known environment limitations are clearly stated (for example missing `make` locally).

## Output Format For Each Milestone (Required)
- Implemented: concise summary of delivered behavior
- Changed files: explicit file list
- Validation: diagnostics + `make -C kernel test` + build/status notes
- Next step: exact next action command or release step

## Example Prompts
- `/kernel-from-start add exception ISR fault screen`
- `/kernel-from-start harden release pipeline and checksums`
- `/kernel-from-start update docs and website for current architecture`
