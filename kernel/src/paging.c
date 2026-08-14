#include "paging.h"

#include <stdint.h>

#include "pmm.h"

/* Static boot page directory defined in boot.s (higher-half virtual address). */
extern uint32_t boot_page_directory[];

uint32_t g_kernel_page_directory_phys = 0u;
uint32_t g_user_page_directory_phys = 0u;

static uint32_t g_user_page_table_phys = 0u;

#ifdef PAGING_ENABLE_TEST_HOOKS
/* Host-side test hooks (see tests/paging_test.c). The test provides a fake
   physical-memory mapping and records CR3 writes instead of executing them. */
extern void* paging_test_phys_to_ptr(uint32_t phys);
extern void paging_test_record_cr3_switch_kernel(uint32_t pd_phys);
extern void paging_test_record_cr3_switch_user(uint32_t pd_phys);
#endif

/* Page directory/page table frames are physical addresses; the kernel's
   1:1 identity map makes them directly dereferenceable. Test hooks redirect
   the dereference through the test's fake physical memory. */
#ifdef PAGING_ENABLE_TEST_HOOKS
static inline uint32_t* page_table_ptr(uint32_t phys) {
    return (uint32_t*)paging_test_phys_to_ptr(phys);
}
#else
static inline uint32_t* page_table_ptr(uint32_t phys) {
    return (uint32_t*)(uintptr_t)phys;
}
#endif

enum {
    PAGE_PRESENT  = 0x01u,
    PAGE_WRITABLE = 0x02u,
    PAGE_USER     = 0x04u,

    PAGE_TABLE_ENTRIES = 1024u,

    /* VGA text buffer. Kept supervisor-only in the user page table so ring-0
       syscall handlers can still draw while the user directory is active. */
    PAGING_VGA_ADDRESS = 0x000B8000u
};

void paging_initialize(void) {
    /* boot.s builds the kernel directory (a 1:1 identity map of all 4 GiB via
       4 MiB PSE pages, which also covers the higher-half kernel link). Store
       its physical address for CR3 reloads. */
    g_kernel_page_directory_phys =
        (uint32_t)(uintptr_t)boot_page_directory - PAGING_KERNEL_VIRTUAL_BASE;
}

void paging_switch_kernel(void) {
#ifdef PAGING_ENABLE_TEST_HOOKS
    paging_test_record_cr3_switch_kernel(g_kernel_page_directory_phys);
#else
    __asm__ volatile("movl %0, %%cr3" : : "r"(g_kernel_page_directory_phys) : "memory");
#endif
}

void paging_switch_user(void) {
#ifdef PAGING_ENABLE_TEST_HOOKS
    paging_test_record_cr3_switch_user(g_user_page_directory_phys);
#else
    __asm__ volatile("movl %0, %%cr3" : : "r"(g_user_page_directory_phys) : "memory");
#endif
}

int paging_prepare_user_space(uint32_t code_va, uint32_t code_pa, uint32_t stack_va, uint32_t stack_pa) {
    if (code_va == 0u || code_pa == 0u || stack_va == 0u || stack_pa == 0u) {
        return 0;
    }

    /* Both user virtual addresses must live in the same 4 MiB window so a
       single page table can back them. */
    if ((code_va >> 22) != (stack_va >> 22)) {
        return 0;
    }

    const uint32_t window_pde = code_va >> 22;

    /* A page table covers exactly one 4 MiB window, so its entries are
       indexed by the low 10 bits of the virtual page number. */
    const uint32_t code_pte = (code_va >> 12) & (PAGE_TABLE_ENTRIES - 1u);
    const uint32_t stack_pte = (stack_va >> 12) & (PAGE_TABLE_ENTRIES - 1u);

    uint32_t pd_frame = pmm_alloc_frame();
    uint32_t pt_frame = pmm_alloc_frame();
    if (pd_frame == 0u || pt_frame == 0u) {
        if (pd_frame != 0u) {
            pmm_free_frame(pd_frame);
        }
        if (pt_frame != 0u) {
            pmm_free_frame(pt_frame);
        }
        return 0;
    }

    uint32_t* pd = page_table_ptr(pd_frame);
    uint32_t* pt = page_table_ptr(pt_frame);

    /* Clone the kernel mappings as supervisor-only pages so interrupt and
       syscall handlers (which run with this directory active) keep working. */
    for (uint32_t i = 0u; i < PAGE_TABLE_ENTRIES; ++i) {
        pd[i] = boot_page_directory[i];
    }

    /* Replace the user window's 4 MiB identity page with a 4 KiB page table. */
    for (uint32_t i = 0u; i < PAGE_TABLE_ENTRIES; ++i) {
        pt[i] = 0u;
    }

    pt[code_pte] = code_pa | (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    pt[stack_pte] = stack_pa | (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    /* Keep the VGA text buffer reachable from ring 0 while the user directory
       is active. When the user window covers VGA (window 0) this page table is
       the only mapping; for any other window the cloned kernel identity PDE
       already maps it supervisor-only. */
    if ((PAGING_VGA_ADDRESS >> 22) == window_pde) {
        pt[PAGING_VGA_ADDRESS >> 12] = PAGING_VGA_ADDRESS | (PAGE_PRESENT | PAGE_WRITABLE);
    }

    pd[window_pde] = pt_frame | (PAGE_PRESENT | PAGE_WRITABLE);

    g_user_page_table_phys = pt_frame;
    g_user_page_directory_phys = pd_frame;
    return 1;
}

/* Release the user page directory and its page table back to the PMM. Call
   with the kernel directory active, after the ring-3 demo has returned. */
void paging_cleanup_user_space(void) {
    if (g_user_page_table_phys != 0u) {
        pmm_free_frame(g_user_page_table_phys);
        g_user_page_table_phys = 0u;
    }
    if (g_user_page_directory_phys != 0u) {
        pmm_free_frame(g_user_page_directory_phys);
        g_user_page_directory_phys = 0u;
    }
}
