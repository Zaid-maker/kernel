#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* The kernel lives in the top 1 GiB; user space is everything below it. */
#define PAGING_KERNEL_VIRTUAL_BASE 0xC0000000u

void paging_initialize(void);

/* Reload CR3 with the kernel page directory (full 1:1 identity map). */
void paging_switch_kernel(void);

/* Reload CR3 with the prepared user page directory. */
void paging_switch_user(void);

/*
 * Build the user page directory used by the ring-3 demo.
 *
 * Clones the kernel mappings as supervisor-only pages, then maps `code_va`
 * and `stack_va` (which must share one 4 MiB window) to `code_pa` and
 * `stack_pa` as user-accessible pages. The VGA text buffer is additionally
 * mapped supervisor-only so syscall/interrupt handlers can keep drawing
 * while the user directory is active.
 *
 * Returns 1 on success. On failure the stored user directory is left
 * untouched and any frames allocated are released.
 */
int paging_prepare_user_space(uint32_t code_va, uint32_t code_pa, uint32_t stack_va, uint32_t stack_pa);

/* Free the page directory/page table created by paging_prepare_user_space.
   Call with the kernel directory active and only after leaving ring 3. */
void paging_cleanup_user_space(void);

/* Physical addresses of the kernel and user page directories, used by the
   ring-3 entry/exit assembly in isr.s. */
extern uint32_t g_kernel_page_directory_phys;
extern uint32_t g_user_page_directory_phys;

#endif
