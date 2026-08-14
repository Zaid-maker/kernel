#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "../src/paging.h"
#include "../src/pmm.h"

/* ------------------------------------------------------------------ */
/* Constants mirroring paging.c internals                              */
/* ------------------------------------------------------------------ */

enum {
    PAGE_PRESENT  = 0x01u,
    PAGE_WRITABLE = 0x02u,
    PAGE_USER     = 0x04u,
    PAGE_PSE      = 0x80u,

    PAGE_TABLE_ENTRIES = 1024u,
    PAGE_4M_FLAGS = (PAGE_PRESENT | PAGE_WRITABLE | PAGE_PSE),

    /* Same values user_mode.c uses for the ring-3 demo (window 0). */
    TEST_CODE_VA = 0x00100000u,
    TEST_STACK_VA = 0x00200000u,

    /* A nonzero user window (PDE 4) exercises the page-table-local PTE
       indexing: full virtual page numbers 4096/4352, local indices 0/256. */
    TEST_NONZERO_WINDOW_CODE_VA = 0x01000000u,
    TEST_NONZERO_WINDOW_STACK_VA = 0x01100000u,

    PAGING_VGA_ADDRESS = 0x000B8000u,
    PAGING_KERNEL_WINDOW_PDE = 768u
};

/* ------------------------------------------------------------------ */
/* Fake boot page directory (boot.s normally provides this symbol)     */
/* ------------------------------------------------------------------ */

uint32_t boot_page_directory[PAGE_TABLE_ENTRIES];

/* ------------------------------------------------------------------ */
/* Fake physical memory + PMM stub                                     */
/* ------------------------------------------------------------------ */

enum {
    PMM_TEST_PHYS_BASE = 0x00100000u,
    PMM_TEST_FRAME_COUNT = 16u,
    PMM_TEST_FRAME_SIZE = 4096u,
    PMM_TEST_RAM_SIZE = 0x00200000u /* 2 MiB of fake physical RAM */
};

/* g_phys_ram is indexed by fake physical address, so every frame the PMM
   hands out is real, dereferenceable host memory. Aligned to uint32_t so
   paging.c's page-table writes through phys_to_ptr() stay well-aligned. */
static _Alignas(uint32_t) uint8_t g_phys_ram[PMM_TEST_RAM_SIZE];
static uint8_t g_frame_used[PMM_TEST_FRAME_COUNT];
static uint32_t g_frame_addrs[PMM_TEST_FRAME_COUNT];

/* When not UINT32_MAX, this many upcoming allocations succeed before the
   allocator starts failing (lets tests hit partial-failure paths). */
static uint32_t g_alloc_successes_before_fail = 0xFFFFFFFFu;

/* paging.c calls this under PAGING_ENABLE_TEST_HOOKS instead of treating
   physical addresses as identity-mapped pointers. */
void* paging_test_phys_to_ptr(uint32_t phys) {
    if (phys >= PMM_TEST_RAM_SIZE) {
        return 0;
    }

    return (void*)&g_phys_ram[phys];
}

uint32_t pmm_alloc_frame(void) {
    if (g_alloc_successes_before_fail == 0u) {
        return 0u;
    }

    if (g_alloc_successes_before_fail != 0xFFFFFFFFu) {
        --g_alloc_successes_before_fail;
    }

    for (uint32_t i = 0u; i < PMM_TEST_FRAME_COUNT; ++i) {
        if (!g_frame_used[i]) {
            g_frame_used[i] = 1u;
            return g_frame_addrs[i];
        }
    }

    return 0u;
}

void pmm_free_frame(uint32_t physical_addr) {
    for (uint32_t i = 0u; i < PMM_TEST_FRAME_COUNT; ++i) {
        if (g_frame_addrs[i] == physical_addr) {
            g_frame_used[i] = 0u;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* CR3 switch recorders (paging.c calls these under test hooks)        */
/* ------------------------------------------------------------------ */

static uint32_t g_last_cr3_kernel = 0u;
static uint32_t g_last_cr3_user = 0u;
static uint32_t g_switch_kernel_count = 0u;
static uint32_t g_switch_user_count = 0u;

void paging_test_record_cr3_switch_kernel(uint32_t pd_phys) {
    g_last_cr3_kernel = pd_phys;
    ++g_switch_kernel_count;
}

void paging_test_record_cr3_switch_user(uint32_t pd_phys) {
    g_last_cr3_user = pd_phys;
    ++g_switch_user_count;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static uint32_t used_frame_count(void) {
    uint32_t count = 0u;
    for (uint32_t i = 0u; i < PMM_TEST_FRAME_COUNT; ++i) {
        if (g_frame_used[i]) {
            ++count;
        }
    }
    return count;
}

static void reset_pmm(void) {
    g_alloc_successes_before_fail = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < PMM_TEST_FRAME_COUNT; ++i) {
        g_frame_used[i] = 0u;
    }
}

static void reset_hooks(void) {
    g_last_cr3_kernel = 0u;
    g_last_cr3_user = 0u;
    g_switch_kernel_count = 0u;
    g_switch_user_count = 0u;
}

static void init_boot_page_directory(void) {
    for (uint32_t i = 0u; i < PAGE_TABLE_ENTRIES; ++i) {
        boot_page_directory[i] = (i << 22) | PAGE_4M_FLAGS;
    }

    /* Higher-half kernel window: VA 0xC0000000 -> physical 0 MiB. */
    boot_page_directory[PAGING_KERNEL_WINDOW_PDE] = PAGE_4M_FLAGS;
}

static int expect_true(const char* name, int value) {
    if (!value) {
        printf("FAIL: %s\n", name);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static int expect_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual != expected) {
        printf("FAIL: %s (actual=0x%08X expected=0x%08X)\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

int main(void) {
    int ok = 1;

    init_boot_page_directory();

    for (uint32_t i = 0u; i < PMM_TEST_FRAME_COUNT; ++i) {
        g_frame_addrs[i] = PMM_TEST_PHYS_BASE + (i * PMM_TEST_FRAME_SIZE);
    }

    /* --- paging_initialize records the kernel directory's physical address --- */
    {
        paging_initialize();

        const uint32_t expected =
            (uint32_t)(uintptr_t)boot_page_directory - PAGING_KERNEL_VIRTUAL_BASE;
        ok &= expect_u32("kernel pd phys computed", g_kernel_page_directory_phys, expected);
    }

    /* --- paging_switch_kernel loads the kernel directory --- */
    {
        reset_hooks();

        paging_switch_kernel();

        ok &= expect_u32("kernel switch count", g_switch_kernel_count, 1u);
        ok &= expect_u32("kernel switch target", g_last_cr3_kernel, g_kernel_page_directory_phys);
    }

    /* --- invalid arguments are rejected without leaking frames --- */
    {
        reset_pmm();

        const uint32_t code_pa = pmm_alloc_frame();
        const uint32_t stack_pa = pmm_alloc_frame();
        const uint32_t baseline = used_frame_count();

        ok &= expect_u32("zero code va rejected",
                         paging_prepare_user_space(0u, code_pa, TEST_STACK_VA, stack_pa), 0u);
        ok &= expect_u32("zero code pa rejected",
                         paging_prepare_user_space(TEST_CODE_VA, 0u, TEST_STACK_VA, stack_pa), 0u);
        ok &= expect_u32("zero stack va rejected",
                         paging_prepare_user_space(TEST_CODE_VA, code_pa, 0u, stack_pa), 0u);
        ok &= expect_u32("zero stack pa rejected",
                         paging_prepare_user_space(TEST_CODE_VA, code_pa, TEST_STACK_VA, 0u), 0u);
        ok &= expect_u32("mismatched windows rejected",
                         paging_prepare_user_space(TEST_CODE_VA, code_pa, 0x01000000u, stack_pa), 0u);
        ok &= expect_u32("no frames leaked by rejected prepares", used_frame_count(), baseline);

        pmm_free_frame(code_pa);
        pmm_free_frame(stack_pa);
    }

    /* --- allocator failure paths release the frames they took --- */
    {
        reset_pmm();

        const uint32_t code_pa = pmm_alloc_frame();
        const uint32_t stack_pa = pmm_alloc_frame();

        /* Page table allocation fails after the directory succeeded. */
        g_alloc_successes_before_fail = 1u;
        ok &= expect_u32("partial alloc failure rejected",
                         paging_prepare_user_space(TEST_CODE_VA, code_pa, TEST_STACK_VA, stack_pa), 0u);
        ok &= expect_u32("pd frame freed after partial failure", used_frame_count(), 2u);

        /* Both allocations fail up front. */
        g_alloc_successes_before_fail = 0u;
        ok &= expect_u32("full alloc failure rejected",
                         paging_prepare_user_space(TEST_CODE_VA, code_pa, TEST_STACK_VA, stack_pa), 0u);
        ok &= expect_u32("no extra frames after full failure", used_frame_count(), 2u);

        pmm_free_frame(code_pa);
        pmm_free_frame(stack_pa);
    }

    /* --- happy path builds a correct user directory --- */
    {
        reset_pmm();
        reset_hooks();

        const uint32_t code_pa = pmm_alloc_frame();
        const uint32_t stack_pa = pmm_alloc_frame();

        ok &= expect_u32("prepare succeeds",
                         paging_prepare_user_space(TEST_CODE_VA, code_pa, TEST_STACK_VA, stack_pa), 1u);

        const uint32_t pd_phys = g_user_page_directory_phys;
        ok &= expect_u32("user pd phys recorded", pd_phys, PMM_TEST_PHYS_BASE + 2u * PMM_TEST_FRAME_SIZE);
        ok &= expect_u32("four frames in use", used_frame_count(), 4u);

        const uint32_t* pd = (const uint32_t*)paging_test_phys_to_ptr(pd_phys);
        ok &= expect_true("user pd readable", pd != 0);
        if (pd != 0) {
            const uint32_t pt_phys = pd[0] & ~0xFFFu;

            /* The user window PDE points at the page table, supervisor-only. */
            ok &= expect_u32("window pde flags", pd[0] & 0x3u, PAGE_PRESENT | PAGE_WRITABLE);
            ok &= expect_u32("window pde has no user bit", pd[0] & PAGE_USER, 0u);
            ok &= expect_u32("window pde targets page table", pt_phys,
                             PMM_TEST_PHYS_BASE + 3u * PMM_TEST_FRAME_SIZE);

            /* Every other PDE is cloned from the kernel directory. */
            int clone_ok = 1;
            for (uint32_t i = 1u; i < PAGE_TABLE_ENTRIES; ++i) {
                if (pd[i] != boot_page_directory[i]) {
                    clone_ok = 0;
                    break;
                }
            }
            ok &= expect_true("kernel pdes cloned", clone_ok);

            const uint32_t* pt = (const uint32_t*)paging_test_phys_to_ptr(pt_phys);
            ok &= expect_true("user page table readable", pt != 0);
            if (pt != 0) {
                /* Code and stack pages are user-accessible. */
                ok &= expect_u32("code pte", pt[TEST_CODE_VA >> 12],
                                 code_pa | (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER));
                ok &= expect_u32("stack pte", pt[TEST_STACK_VA >> 12],
                                 stack_pa | (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER));

                /* VGA text buffer stays supervisor-only for ring-0 handlers. */
                ok &= expect_u32("vga pte target", pt[PAGING_VGA_ADDRESS >> 12] & ~0xFFFu,
                                 PAGING_VGA_ADDRESS);
                ok &= expect_u32("vga pte flags", pt[PAGING_VGA_ADDRESS >> 12] & 0x3u,
                                 PAGE_PRESENT | PAGE_WRITABLE);
                ok &= expect_u32("vga pte has no user bit", pt[PAGING_VGA_ADDRESS >> 12] & PAGE_USER, 0u);

                /* Nothing else is mapped inside the user window. */
                ok &= expect_u32("null page unmapped", pt[0u], 0u);
                ok &= expect_u32("unused window pte unmapped", pt[0x00300000u >> 12], 0u);
            }
        }

        /* --- paging_switch_user loads the user directory --- */
        paging_switch_user();
        ok &= expect_u32("user switch count", g_switch_user_count, 1u);
        ok &= expect_u32("user switch target", g_last_cr3_user, pd_phys);

        /* --- cleanup releases the page directory and table --- */
        paging_cleanup_user_space();
        ok &= expect_u32("user pd phys cleared", g_user_page_directory_phys, 0u);
        ok &= expect_u32("pd and pt freed by cleanup", used_frame_count(), 2u);

        reset_hooks();
        paging_switch_user();
        ok &= expect_u32("switch after cleanup records zero", g_last_cr3_user, 0u);
    }

    /* --- a nonzero user window maps the correct PDE and local PTE indices --- */
    {
        reset_pmm();

        const uint32_t code_pa = pmm_alloc_frame();
        const uint32_t stack_pa = pmm_alloc_frame();

        ok &= expect_u32("prepare nonzero window succeeds",
                         paging_prepare_user_space(TEST_NONZERO_WINDOW_CODE_VA, code_pa,
                                                   TEST_NONZERO_WINDOW_STACK_VA, stack_pa), 1u);

        const uint32_t pd_phys = g_user_page_directory_phys;
        const uint32_t* pd = (const uint32_t*)paging_test_phys_to_ptr(pd_phys);
        ok &= expect_true("nonzero window pd readable", pd != 0);
        if (pd != 0) {
            const uint32_t window_pde = TEST_NONZERO_WINDOW_CODE_VA >> 22;
            const uint32_t pt_phys = pd[window_pde] & ~0xFFFu;

            ok &= expect_u32("nonzero window pde selected", window_pde, 4u);
            ok &= expect_u32("nonzero window pde flags", pd[window_pde] & 0x3u,
                             PAGE_PRESENT | PAGE_WRITABLE);
            ok &= expect_u32("nonzero window pde no user bit", pd[window_pde] & PAGE_USER, 0u);
            ok &= expect_u32("nonzero window pde targets page table", pt_phys,
                             PMM_TEST_PHYS_BASE + 3u * PMM_TEST_FRAME_SIZE);

            /* The VGA window's PDE is left as the cloned kernel identity page,
               so VGA stays reachable from ring 0 without a VGA entry here. */
            ok &= expect_u32("vga window pde cloned", pd[0], boot_page_directory[0]);

            const uint32_t* pt = (const uint32_t*)paging_test_phys_to_ptr(pt_phys);
            ok &= expect_true("nonzero window pt readable", pt != 0);
            if (pt != 0) {
                const uint32_t code_local =
                    (TEST_NONZERO_WINDOW_CODE_VA >> 12) & (PAGE_TABLE_ENTRIES - 1u);
                const uint32_t stack_local =
                    (TEST_NONZERO_WINDOW_STACK_VA >> 12) & (PAGE_TABLE_ENTRIES - 1u);

                ok &= expect_u32("nonzero window code pte", pt[code_local],
                                 code_pa | (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER));
                ok &= expect_u32("nonzero window stack pte", pt[stack_local],
                                 stack_pa | (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER));

                /* No VGA entry lives in this window's page table. */
                ok &= expect_u32("nonzero window has no vga pte",
                                 pt[PAGING_VGA_ADDRESS >> 12], 0u);
            }
        }

        paging_cleanup_user_space();
        pmm_free_frame(code_pa);
        pmm_free_frame(stack_pa);
    }

    /* --- a second run reuses the released frames --- */
    {
        reset_pmm();

        const uint32_t code_pa = pmm_alloc_frame();
        const uint32_t stack_pa = pmm_alloc_frame();

        ok &= expect_u32("prepare after cleanup succeeds",
                         paging_prepare_user_space(TEST_CODE_VA, code_pa, TEST_STACK_VA, stack_pa), 1u);
        ok &= expect_u32("second run reuses pd frame", g_user_page_directory_phys,
                         PMM_TEST_PHYS_BASE + 2u * PMM_TEST_FRAME_SIZE);

        paging_cleanup_user_space();
        pmm_free_frame(code_pa);
        pmm_free_frame(stack_pa);
        ok &= expect_u32("all frames released", used_frame_count(), 0u);
    }

    return ok ? 0 : 1;
}
