ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xC0000000;

SECTIONS {
    /*
     * The boot stub (multiboot header + early assembly) is linked at its
     * physical address (1 MiB) because it runs before paging is enabled.
     * GRUB loads ELF segments at their LMA, so this section is placed with
     * VMA == LMA == 1 MiB and the entry point (_start) stays a physical
     * address the CPU can execute immediately.
     */
    . = 1M;
    _kernel_physical_start = .;

    .boot : ALIGN(4K) {
        *(.multiboot)
        *(.boot)
    }

    /*
     * Everything from here on is linked in the higher half (VMA) but loaded
     * at the corresponding physical address (LMA), so `physical = virtual -
     * KERNEL_VIRTUAL_BASE` holds for every kernel symbol.
     */
    . = ALIGN(4K);
    . += KERNEL_VIRTUAL_BASE;
    _kernel_virtual_start = .;

    .text : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE) {
        *(.text*)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE) {
        *(.rodata*)
    }

    .data : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE) {
        *(.data*)
    }

    .bss : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE) {
        *(COMMON)
        *(.bss*)
    }

    _kernel_virtual_end = .;
    _kernel_physical_end = _kernel_virtual_end - KERNEL_VIRTUAL_BASE;
}
