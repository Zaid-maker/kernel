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
        const struct pmm_stats pmm_stats = pmm_get_stats();
        if (pmm_stats.total_frames == 0u) {
            printf("SKIP: runtime heap test (no 32-bit-safe PMM stub frames on this host)\n");
            return 0;
        }
    }

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        struct heap_diag_counters initial_diag;
        heap_get_diag_counters(&initial_diag);
        ok &= expect_u32("initial diag alloc calls", initial_diag.alloc_calls, 0u);

        ok &= expect_u32("hist bucket count wrapper", heap_hist_bucket_count(), HEAP_HIST_BUCKETS);
        ok &= expect_u32("hist bucket first limit wrapper", heap_hist_bucket_limits()[0], 16u);

        struct heap_trace_record empty_records[4];
        ok &= expect_u32("trace snapshot wrapper empty", heap_trace_snapshot(empty_records, 4u), 0u);

        void* null_alloc = kmalloc(0u);
        ok &= expect_true("zero-size allocation returns null", null_alloc == 0);

        void* first = kmalloc(16u);
        void* second = kmalloc(64u);
        void* third = kmalloc(128u);
        ok &= expect_true("first allocation", first != 0);
        ok &= expect_true("second allocation", second != 0);
        ok &= expect_true("third allocation", third != 0);
        ok &= expect_true("first alignment", ((uintptr_t)first & 7u) == 0u);
        ok &= expect_true("second alignment", ((uintptr_t)second & 7u) == 0u);
        ok &= expect_true("third alignment", ((uintptr_t)third & 7u) == 0u);

        struct heap_stats stats = heap_get_stats();
        ok &= expect_true("stats after alloc used", stats.used_bytes > 0u);
        ok &= expect_true("stats after alloc blocks", stats.block_count > 0u);
        ok &= expect_true("stats after alloc free blocks", stats.free_blocks > 0u);

        kfree(second);
        kfree(first);
        kfree(third);

        struct heap_stats merged_stats = heap_get_stats();
        ok &= expect_true("merged free blocks", merged_stats.free_blocks > 0u);
        ok &= expect_true("largest free block present", merged_stats.largest_free_block > 0u);
        ok &= expect_true("free bytes present", merged_stats.free_bytes > 0u);

        /* Invalid free: pointer not from heap should increment invalid-free diagnostics. */
        uint32_t bogus = 0u;
        kfree(&bogus);

        /* Double free should also increment invalid-free diagnostics. */
        void* once = kmalloc(24u);
        ok &= expect_true("double-free setup alloc", once != 0);
        kfree(once);
        kfree(once);

        struct heap_diag_counters diag_after_invalid;
        heap_get_diag_counters(&diag_after_invalid);
        ok &= expect_true("invalid free counter updated", diag_after_invalid.invalid_free_calls >= 2u);
    }

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        void* ptr = kmalloc(32u);
        ok &= expect_true("integrity setup alloc", ptr != 0);
        kfree(ptr);

        struct heap_integrity_report report;
        ok &= expect_true("runtime integrity after cleanup", heap_check_integrity(&report));
        ok &= expect_u32("runtime corrupted headers", report.corrupted_headers, 0u);
        ok &= expect_u32("runtime unmerged pairs", report.adjacent_unmerged_free_pairs, 0u);
        ok &= expect_u32("runtime next regressions", report.next_pointer_regressions, 0u);
    }

    {
        pmm_initialize(0u, (const struct multiboot_info*)0);
        heap_initialize();

        /* Consume all PMM-backed heap growth to hit kmalloc grow-failure path. */
        void* allocations[256];
        uint32_t alloc_count = 0u;
        for (; alloc_count < 256u; ++alloc_count) {
            allocations[alloc_count] = kmalloc(2048u);
            if (allocations[alloc_count] == 0) {
                break;
            }
        }

        ok &= expect_true("grow failure reached", alloc_count < 256u);

        struct heap_diag_counters diag;
        heap_get_diag_counters(&diag);
        ok &= expect_true("failed alloc recorded", diag.failed_alloc_calls > 0u);

        for (uint32_t i = 0; i < alloc_count; ++i) {
            kfree(allocations[i]);
        }
    }

    return ok ? 0 : 1;
}
