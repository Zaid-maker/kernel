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

    return ok ? 0 : 1;
}
