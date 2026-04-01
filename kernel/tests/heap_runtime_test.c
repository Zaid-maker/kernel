#include <stdint.h>
#include <stdio.h>

#include "../src/heap.h"
#include "../src/pmm.h"

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
        printf("FAIL: %s (actual=%u expected=%u)\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

int main(void) {
    int ok = 1;

    pmm_initialize(0u, (const struct multiboot_info*)0);
    heap_initialize();

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        void* first = kmalloc(16u);
        ok &= expect_true("first allocation", first != 0);
        ok &= expect_true("first alignment", ((uintptr_t)first & 7u) == 0u);

        struct heap_stats stats = heap_get_stats();
        ok &= expect_true("stats after first alloc used", stats.used_bytes > 0u);
        ok &= expect_true("stats after first alloc blocks", stats.block_count > 0u);
        ok &= expect_true("stats after first alloc free blocks", stats.free_blocks > 0u);
        kfree(first);
    }

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        void* first = kmalloc(24u);
        void* second = kmalloc(24u);
        ok &= expect_true("second allocation", first != 0 && second != 0);

        kfree(first);
        kfree(second);

        struct heap_stats stats = heap_get_stats();
        ok &= expect_u32("merged free blocks", stats.free_blocks, 1u);
        ok &= expect_true("largest free block present", stats.largest_free_block > 0u);
        ok &= expect_true("free bytes present", stats.free_bytes > 0u);
    }

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        void* ptr = kmalloc(32u);
        ok &= expect_true("double free setup alloc", ptr != 0);
        kfree(ptr);
        kfree(ptr);

        struct heap_diag_counters diag;
        heap_get_diag_counters(&diag);
        ok &= expect_u32("invalid free count after double free", diag.invalid_free_calls > 0u ? 1u : 0u, 1u);
    }

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        void* allocations[16];
        uint32_t count = 0u;
        while (count < 16u) {
            void* ptr = kmalloc(4000u);
            if (ptr == 0) {
                break;
            }
            allocations[count++] = ptr;
        }

        ok &= expect_true("allocator eventually exhausts frames", count > 0u);
        void* extra = kmalloc(4000u);
        ok &= expect_true("allocation failure after exhaustion", extra == 0);

        struct heap_diag_counters diag;
        heap_get_diag_counters(&diag);
        ok &= expect_true("failed alloc counter incremented", diag.failed_alloc_calls > 0u);

        for (uint32_t i = 0; i < count; ++i) {
            kfree(allocations[i]);
        }
    }

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        void* left = kmalloc(24u);
        void* right = kmalloc(24u);
        ok &= expect_true("final cleanup setup left", left != 0);
        ok &= expect_true("final cleanup setup right", right != 0);
        kfree(left);
        kfree(right);

        struct heap_integrity_report report;
        ok &= expect_true("runtime integrity after cleanup", heap_check_integrity(&report));
        ok &= expect_u32("runtime corrupted headers", report.corrupted_headers, 0u);
        ok &= expect_u32("runtime unmerged pairs", report.adjacent_unmerged_free_pairs, 0u);
        ok &= expect_u32("runtime next regressions", report.next_pointer_regressions, 0u);
    }

    return ok ? 0 : 1;
}
