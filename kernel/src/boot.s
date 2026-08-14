/* Multiboot header, early paging setup, and higher-half entry glue for 32-bit x86 */

.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.set KERNEL_VIRTUAL_BASE, 0xC0000000

/* Page flags: the boot page directory uses 4 MiB pages (PSE) for a full
   1:1 identity map of physical memory. */
.set PAGE_PRESENT,  0x01
.set PAGE_WRITABLE, 0x02
.set PAGE_PSE,      0x80
.set PAGE_4M,       PAGE_PRESENT | PAGE_WRITABLE | PAGE_PSE

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/*
 * Early entry point. This section is linked at physical 1 MiB (VMA == LMA),
 * so GRUB jumps straight here with paging still disabled:
 *   eax = multiboot magic, ebx = physical address of multiboot info.
 */
.section .boot
.global _start
_start:
    cli

    /* The stack lives in .bss, which is linked in the higher half; use its
       physical address for now (the identity map covers it after paging). */
    movl $(stack_top - KERNEL_VIRTUAL_BASE), %esp

    /* Load the boot page directory (physical address) into CR3. */
    movl $(boot_page_directory - KERNEL_VIRTUAL_BASE), %ecx
    movl %ecx, %cr3

    /* Enable Page Size Extension so PDEs can map 4 MiB pages. */
    movl %cr4, %ecx
    orl $0x10, %ecx
    movl %ecx, %cr4

    /* Enable paging. */
    movl %cr0, %ecx
    orl $0x80000000, %ecx
    movl %ecx, %cr0

    /* Jump to the higher-half mapping of the kernel. */
    ljmp $0x08, $boot_higher_half

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .data
.align 4096
.global boot_page_directory
boot_page_directory:
    /*
     * 1:1 identity map of physical memory using 4 MiB pages. The kernel is
     * linked at 0xC0000000 + 1M, so PDE 768 (VA 0xC0000000) is pointed at
     * physical 0 MiB instead: the kernel's higher-half virtual addresses
     * resolve to its physical load range while the rest of the map stays a
     * straight identity map.
     */
    .set i, 0
    .rept 768
    .long (i << 22) | PAGE_4M
    .set i, i + 1
    .endr
    .long PAGE_4M                    /* PDE 768: 0xC0000000 -> 0x00000000 */
    .set i, 769
    .rept 255
    .long (i << 22) | PAGE_4M
    .set i, i + 1
    .endr

/*
 * Kernel proper. Everything below runs at higher-half virtual addresses.
 */
.section .text
boot_higher_half:
    /* Hand the multiboot magic (eax) and info pointer (ebx) to kernel_main.
       ebx is a physical address; the identity map keeps it valid. */
    pushl %ebx
    pushl %eax
    call kernel_main
    addl $8, %esp
.hang:
    hlt
    jmp .hang
