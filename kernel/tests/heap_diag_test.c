#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/heap_diag.h"

static int expect_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual != expected) {
        printf("FAIL: %s (actual=%u expected=%u)\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static int find_record(uint32_t addr, uint32_t size) {
    struct heap_trace_record records[HEAP_TRACE_MAX_RECORDS];
    const uint32_t count = heap_diag_trace_snapshot(records, HEAP_TRACE_MAX_RECORDS);
    for (uint32_t i = 0; i < count; ++i) {
        if (records[i].addr == addr && records[i].size == size) {
            return 1;
        }
    }

    return 0;
}

int main(void) {
    int ok = 1;

    heap_diag_reset();

    {
        struct heap_diag_counters diag;
        heap_diag_get_counters(&diag);
        ok &= expect_u32("initial alloc calls", diag.alloc_calls, 0u);
        ok &= expect_u32("initial free calls", diag.free_calls, 0u);
        ok &= expect_u32("initial live allocations", diag.live_allocations, 0u);
        ok &= expect_u32("initial peak live", diag.peak_live_allocations, 0u);
    }

    {
        const uint32_t* limits = heap_diag_hist_bucket_limits();
        ok &= expect_u32("bucket count", heap_diag_hist_bucket_count(), HEAP_HIST_BUCKETS);
        ok &= expect_u32("bucket[0]", limits[0], 16u);
        ok &= expect_u32("bucket[6]", limits[6], 1024u);
    }

    {
        heap_diag_record_alloc(0x1000u, 16u);
        heap_diag_record_alloc(0x2000u, 17u);
        heap_diag_record_alloc(0x3000u, 1030u);

        struct heap_diag_counters diag;
        heap_diag_get_counters(&diag);
        ok &= expect_u32("alloc calls after 3", diag.alloc_calls, 3u);
        ok &= expect_u32("live allocations after 3", diag.live_allocations, 3u);
        ok &= expect_u32("peak live after 3", diag.peak_live_allocations, 3u);
        ok &= expect_u32("hist blocks <=16", diag.hist_live_blocks[0], 1u);
        ok &= expect_u32("hist blocks <=32", diag.hist_live_blocks[1], 1u);
        ok &= expect_u32("hist blocks >1024", diag.hist_live_blocks[7], 1u);
        ok &= expect_u32("hist bytes <=16", diag.hist_live_bytes[0], 16u);
        ok &= expect_u32("hist bytes <=32", diag.hist_live_bytes[1], 17u);
        ok &= expect_u32("hist bytes >1024", diag.hist_live_bytes[7], 1030u);
        ok &= expect_u32("snapshot count after 3", heap_diag_trace_snapshot((struct heap_trace_record[4]){{0}}, 4u), 3u);
        ok &= find_record(0x1000u, 16u);
        ok &= find_record(0x2000u, 17u);
        ok &= find_record(0x3000u, 1030u);
    }

    {
        heap_diag_record_free(0x2000u, 17u);

        struct heap_diag_counters diag;
        heap_diag_get_counters(&diag);
        ok &= expect_u32("free calls after one free", diag.free_calls, 1u);
        ok &= expect_u32("live allocations after one free", diag.live_allocations, 2u);
        ok &= expect_u32("hist blocks <=32 after free", diag.hist_live_blocks[1], 0u);
        ok &= expect_u32("hist bytes <=32 after free", diag.hist_live_bytes[1], 0u);
        ok &= expect_u32("snapshot count after free", heap_diag_trace_snapshot((struct heap_trace_record[4]){{0}}, 4u), 2u);
        ok &= (find_record(0x2000u, 17u) == 0);
    }

    {
        heap_diag_record_failed_alloc();
        heap_diag_record_invalid_free();

        struct heap_diag_counters diag;
        heap_diag_get_counters(&diag);
        ok &= expect_u32("failed alloc count", diag.failed_alloc_calls, 1u);
        ok &= expect_u32("invalid free count", diag.invalid_free_calls, 1u);
    }

    {
        struct heap_integrity_report report;
        report.blocks_scanned = 7u;
        report.corrupted_headers = 1u;
        report.split_alignment_issues = 2u;
        report.adjacent_unmerged_free_pairs = 3u;
        report.next_pointer_regressions = 4u;

        char line[128];
        heap_diag_format_triage_line(line, sizeof(line), &report, 0);
        ok &= expect_u32("triage line match", strcmp(line, "Heap triage: FAIL scanned=7 bad=1 align=2 unmerged=3 next=4") == 0 ? 1u : 0u, 1u);
    }

    {
        heap_diag_reset();

        for (uint32_t i = 0; i < HEAP_TRACE_MAX_RECORDS + 3u; ++i) {
            heap_diag_record_alloc(0x5000u + i * 8u, 24u);
        }

        struct heap_diag_counters diag;
        heap_diag_get_counters(&diag);
        ok &= expect_u32("overflow alloc calls", diag.alloc_calls, HEAP_TRACE_MAX_RECORDS + 3u);
        ok &= expect_u32("overflow live allocations", diag.live_allocations, HEAP_TRACE_MAX_RECORDS + 3u);
        ok &= expect_u32("overflow trace events", diag.trace_overflow_events, 3u);
        ok &= expect_u32("overflow snapshot clipped", heap_diag_trace_snapshot((struct heap_trace_record[HEAP_TRACE_MAX_RECORDS]){{0}}, HEAP_TRACE_MAX_RECORDS), HEAP_TRACE_MAX_RECORDS);
    }

    return ok ? 0 : 1;
}