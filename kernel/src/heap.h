#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

struct heap_stats {
    uint32_t total_bytes;
    uint32_t used_bytes;
    uint32_t free_bytes;
    uint32_t block_count;
    uint32_t free_blocks;
    uint32_t largest_free_block;
    uint32_t smallest_free_block;
};

enum {
    HEAP_HIST_BUCKETS = 8,
    HEAP_TRACE_MAX_RECORDS = 128
};

struct heap_diag_counters {
    uint32_t alloc_calls;
    uint32_t free_calls;
    uint32_t failed_alloc_calls;
    uint32_t invalid_free_calls;
    uint32_t live_allocations;
    uint32_t peak_live_allocations;
    uint32_t trace_overflow_events;
    uint32_t hist_live_blocks[HEAP_HIST_BUCKETS];
    uint32_t hist_live_bytes[HEAP_HIST_BUCKETS];
};

struct heap_trace_record {
    uint32_t addr;
    uint32_t size;
};

void heap_initialize(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
struct heap_stats heap_get_stats(void);
void heap_get_diag_counters(struct heap_diag_counters* out);
uint32_t heap_trace_snapshot(struct heap_trace_record* out_records, uint32_t max_records);
const uint32_t* heap_hist_bucket_limits(void);
uint32_t heap_hist_bucket_count(void);

#endif
