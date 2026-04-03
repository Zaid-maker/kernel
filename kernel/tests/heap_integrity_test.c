#include <stdint.h>
#include <stdio.h>

#ifndef HEAP_ENABLE_TEST_HOOKS
#define HEAP_ENABLE_TEST_HOOKS
#endif
#include "../src/heap.h"

static int expect_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual != expected) {
        printf("FAIL: %s (actual=%u expected=%u)\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static int expect_true(const char* name, int value) {
    if (!value) {
        printf("FAIL: %s\n", name);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

int main(void) {
    int ok = 1;

    {
        uint32_t sizes[] = {32u, 64u, 128u};
        uint8_t frees[] = {0u, 1u, 0u};
        ok &= expect_true("seed valid chain", heap_debug_seed_chain(sizes, frees, 3u));

        struct heap_integrity_report report;
        int pass = heap_check_integrity(&report);
        ok &= expect_true("valid chain integrity pass", pass);
        ok &= expect_u32("valid corrupted headers", report.corrupted_headers, 0u);
        ok &= expect_u32("valid split align", report.split_alignment_issues, 0u);
        ok &= expect_u32("valid unmerged free pairs", report.adjacent_unmerged_free_pairs, 0u);
        ok &= expect_u32("valid next regressions", report.next_pointer_regressions, 0u);
    }

    {
        uint32_t sizes[] = {32u, 64u};
        uint8_t frees[] = {0u, 0u};
        ok &= expect_true("seed for magic corruption", heap_debug_seed_chain(sizes, frees, 2u));
        heap_debug_corrupt_magic(1u);

        struct heap_integrity_report report;
        int pass = heap_check_integrity(&report);
        ok &= expect_true("magic corruption detected", !pass);
        ok &= expect_u32("magic corrupted headers count", report.corrupted_headers, 1u);
    }

    {
        uint32_t sizes[] = {32u, 64u};
        uint8_t frees[] = {0u, 0u};
        ok &= expect_true("seed for misalign", heap_debug_seed_chain(sizes, frees, 2u));
        heap_debug_misalign_size(0u);

        struct heap_integrity_report report;
        int pass = heap_check_integrity(&report);
        ok &= expect_true("misalign detected", !pass);
        ok &= expect_u32("misalign count", report.split_alignment_issues, 1u);
    }

    {
        uint32_t sizes[] = {32u, 64u};
        uint8_t frees[] = {1u, 1u};
        ok &= expect_true("seed for unmerged pair", heap_debug_seed_chain(sizes, frees, 2u));

        struct heap_integrity_report report;
        int pass = heap_check_integrity(&report);
        ok &= expect_true("unmerged pair detected", !pass);
        ok &= expect_u32("unmerged free pair count", report.adjacent_unmerged_free_pairs, 1u);
    }

    {
        uint32_t sizes[] = {32u, 64u, 96u};
        uint8_t frees[] = {0u, 0u, 0u};
        ok &= expect_true("seed for next regression", heap_debug_seed_chain(sizes, frees, 3u));
        heap_debug_make_next_regression(2u);

        struct heap_integrity_report report;
        int pass = heap_check_integrity(&report);
        ok &= expect_true("next regression detected", !pass);
        ok &= expect_u32("next regression count", report.next_pointer_regressions, 1u);
    }

    {
        uint32_t sizes[] = {20000u};
        uint8_t frees[] = {1u};
        ok &= expect_true("seed overflow rejected", !heap_debug_seed_chain(sizes, frees, 1u));
    }

    heap_debug_clear_chain();
    return ok ? 0 : 1;
}
