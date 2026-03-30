#include "heap_diag.h"

#include <stdint.h>

enum {
    HEAP_HIST_LAST_BUCKET = HEAP_HIST_BUCKETS - 1u
};

struct heap_trace_slot {
    uint8_t used;
    uint32_t addr;
    uint32_t size;
};

static const uint32_t g_hist_bucket_limits[HEAP_HIST_BUCKETS] = {16u, 32u, 64u, 128u, 256u, 512u, 1024u, 0xFFFFFFFFu};
static struct heap_diag_counters g_diag;
static struct heap_trace_slot g_trace_slots[HEAP_TRACE_MAX_RECORDS];

static uint32_t bucket_index_for_size(uint32_t size) {
    for (uint32_t i = 0; i < HEAP_HIST_BUCKETS; ++i) {
        if (size <= g_hist_bucket_limits[i]) {
            return i;
        }
    }

    return HEAP_HIST_LAST_BUCKET;
}

void heap_diag_reset(void) {
    g_diag.alloc_calls = 0u;
    g_diag.free_calls = 0u;
    g_diag.failed_alloc_calls = 0u;
    g_diag.invalid_free_calls = 0u;
    g_diag.live_allocations = 0u;
    g_diag.peak_live_allocations = 0u;
    g_diag.trace_overflow_events = 0u;

    for (uint32_t i = 0; i < HEAP_HIST_BUCKETS; ++i) {
        g_diag.hist_live_blocks[i] = 0u;
        g_diag.hist_live_bytes[i] = 0u;
    }

    for (uint32_t i = 0; i < HEAP_TRACE_MAX_RECORDS; ++i) {
        g_trace_slots[i].used = 0u;
        g_trace_slots[i].addr = 0u;
        g_trace_slots[i].size = 0u;
    }
}

void heap_diag_record_alloc(uint32_t addr, uint32_t size) {
    const uint32_t bucket = bucket_index_for_size(size);

    ++g_diag.alloc_calls;
    ++g_diag.live_allocations;
    if (g_diag.live_allocations > g_diag.peak_live_allocations) {
        g_diag.peak_live_allocations = g_diag.live_allocations;
    }

    ++g_diag.hist_live_blocks[bucket];
    g_diag.hist_live_bytes[bucket] += size;

    for (uint32_t i = 0; i < HEAP_TRACE_MAX_RECORDS; ++i) {
        if (!g_trace_slots[i].used) {
            g_trace_slots[i].used = 1u;
            g_trace_slots[i].addr = addr;
            g_trace_slots[i].size = size;
            return;
        }
    }

    ++g_diag.trace_overflow_events;
}

void heap_diag_record_failed_alloc(void) {
    ++g_diag.failed_alloc_calls;
}

void heap_diag_record_invalid_free(void) {
    ++g_diag.invalid_free_calls;
}

void heap_diag_record_free(uint32_t addr, uint32_t size) {
    const uint32_t bucket = bucket_index_for_size(size);

    ++g_diag.free_calls;

    if (g_diag.live_allocations > 0u) {
        --g_diag.live_allocations;
    }

    if (g_diag.hist_live_blocks[bucket] > 0u) {
        --g_diag.hist_live_blocks[bucket];
    }

    if (g_diag.hist_live_bytes[bucket] >= size) {
        g_diag.hist_live_bytes[bucket] -= size;
    } else {
        g_diag.hist_live_bytes[bucket] = 0u;
    }

    for (uint32_t i = 0; i < HEAP_TRACE_MAX_RECORDS; ++i) {
        if (g_trace_slots[i].used && g_trace_slots[i].addr == addr) {
            g_trace_slots[i].used = 0u;
            g_trace_slots[i].addr = 0u;
            g_trace_slots[i].size = 0u;
            return;
        }
    }
}

void heap_diag_get_counters(struct heap_diag_counters* out) {
    if (out == 0) {
        return;
    }

    *out = g_diag;
}

uint32_t heap_diag_trace_snapshot(struct heap_trace_record* out_records, uint32_t max_records) {
    if (out_records == 0 || max_records == 0u) {
        return 0u;
    }

    uint32_t out_count = 0u;
    for (uint32_t i = 0; i < HEAP_TRACE_MAX_RECORDS && out_count < max_records; ++i) {
        if (g_trace_slots[i].used) {
            out_records[out_count].addr = g_trace_slots[i].addr;
            out_records[out_count].size = g_trace_slots[i].size;
            ++out_count;
        }
    }

    return out_count;
}

const uint32_t* heap_diag_hist_bucket_limits(void) {
    return g_hist_bucket_limits;
}

uint32_t heap_diag_hist_bucket_count(void) {
    return HEAP_HIST_BUCKETS;
}
