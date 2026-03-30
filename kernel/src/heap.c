#include "heap.h"

#include <stdint.h>

#include "pmm.h"

enum {
    HEAP_ALIGN = 8u,
    HEAP_GROW_FRAME_COUNT = 1u,
    HEAP_MAGIC = 0xC0DEFACEu
};

struct heap_block {
    uint32_t magic;
    uint32_t size;
    uint8_t free;
    struct heap_block* next;
};

static struct heap_block* g_heap_head = (struct heap_block*)0;
static const uint32_t g_hist_bucket_limits[HEAP_HIST_BUCKETS] = {16u, 32u, 64u, 128u, 256u, 512u, 1024u, 0xFFFFFFFFu};

struct heap_trace_slot {
    uint8_t used;
    uint32_t addr;
    uint32_t size;
};

static struct heap_diag_counters g_diag;
static struct heap_trace_slot g_trace_slots[HEAP_TRACE_MAX_RECORDS];

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1u) & ~(align - 1u);
}

static uint32_t block_overhead(void) {
    return (uint32_t)sizeof(struct heap_block);
}

static void heap_diag_reset(void) {
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

static uint32_t bucket_index_for_size(uint32_t size) {
    for (uint32_t i = 0; i < HEAP_HIST_BUCKETS; ++i) {
        if (size <= g_hist_bucket_limits[i]) {
            return i;
        }
    }
    return HEAP_HIST_BUCKETS - 1u;
}

static void heap_diag_on_alloc(uint32_t addr, uint32_t size) {
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

static void heap_diag_on_free(uint32_t addr, uint32_t size) {
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

static void append_block(struct heap_block* block) {
    block->next = (struct heap_block*)0;

    if (g_heap_head == 0) {
        g_heap_head = block;
        return;
    }

    struct heap_block* cur = g_heap_head;
    while (cur->next != 0) {
        cur = cur->next;
    }
    cur->next = block;
}

static void merge_free_neighbors(void) {
    struct heap_block* cur = g_heap_head;
    while (cur != 0 && cur->next != 0) {
        struct heap_block* next = cur->next;

        uintptr_t cur_end = (uintptr_t)cur + block_overhead() + cur->size;
        if (cur->free && next->free && cur_end == (uintptr_t)next) {
            cur->size += block_overhead() + next->size;
            cur->next = next->next;
            continue;
        }

        cur = cur->next;
    }
}

static int heap_grow(void) {
    for (uint32_t i = 0; i < HEAP_GROW_FRAME_COUNT; ++i) {
        uint32_t frame_addr = pmm_alloc_frame();
        if (frame_addr == 0u) {
            return 0;
        }

        struct heap_block* block = (struct heap_block*)(uintptr_t)frame_addr;
        block->magic = HEAP_MAGIC;
        block->free = 1;
        block->next = (struct heap_block*)0;

        if (4096u <= block_overhead()) {
            block->size = 0;
        } else {
            block->size = 4096u - block_overhead();
        }

        append_block(block);
    }

    merge_free_neighbors();
    return 1;
}

void heap_initialize(void) {
    g_heap_head = (struct heap_block*)0;
    heap_diag_reset();
}

void* kmalloc(uint32_t size) {
    if (size == 0u) {
        return (void*)0;
    }

    uint32_t need = align_up(size, HEAP_ALIGN);

    for (;;) {
        struct heap_block* cur = g_heap_head;
        while (cur != 0) {
            if (cur->magic == HEAP_MAGIC && cur->free && cur->size >= need) {
                uint32_t remaining = cur->size - need;
                if (remaining > block_overhead() + HEAP_ALIGN) {
                    struct heap_block* split = (struct heap_block*)((uintptr_t)cur + block_overhead() + need);
                    split->magic = HEAP_MAGIC;
                    split->size = remaining - block_overhead();
                    split->free = 1;
                    split->next = cur->next;

                    cur->size = need;
                    cur->next = split;
                }

                cur->free = 0;
                void* out = (void*)((uintptr_t)cur + block_overhead());
                heap_diag_on_alloc((uint32_t)(uintptr_t)out, cur->size);
                return out;
            }
            cur = cur->next;
        }

        if (!heap_grow()) {
            ++g_diag.failed_alloc_calls;
            return (void*)0;
        }
    }
}

void kfree(void* ptr) {
    if (ptr == 0) {
        return;
    }

    struct heap_block* block = (struct heap_block*)((uintptr_t)ptr - block_overhead());
    if (block->magic != HEAP_MAGIC) {
        ++g_diag.invalid_free_calls;
        return;
    }

    if (block->free) {
        ++g_diag.invalid_free_calls;
        return;
    }

    heap_diag_on_free((uint32_t)(uintptr_t)ptr, block->size);
    block->free = 1;
    merge_free_neighbors();
}

void heap_get_diag_counters(struct heap_diag_counters* out) {
    if (out == 0) {
        return;
    }

    *out = g_diag;
}

uint32_t heap_trace_snapshot(struct heap_trace_record* out_records, uint32_t max_records) {
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

const uint32_t* heap_hist_bucket_limits(void) {
    return g_hist_bucket_limits;
}

uint32_t heap_hist_bucket_count(void) {
    return HEAP_HIST_BUCKETS;
}

struct heap_stats heap_get_stats(void) {
    struct heap_stats stats;
    stats.total_bytes = 0u;
    stats.used_bytes = 0u;
    stats.free_bytes = 0u;
    stats.block_count = 0u;
    stats.free_blocks = 0u;
    stats.largest_free_block = 0u;
    stats.smallest_free_block = 0u;

    struct heap_block* cur = g_heap_head;
    while (cur != 0) {
        if (cur->magic == HEAP_MAGIC) {
            ++stats.block_count;
            stats.total_bytes += cur->size;
            if (cur->free) {
                ++stats.free_blocks;
                stats.free_bytes += cur->size;
                if (cur->size > stats.largest_free_block) {
                    stats.largest_free_block = cur->size;
                }
                if (stats.smallest_free_block == 0u || cur->size < stats.smallest_free_block) {
                    stats.smallest_free_block = cur->size;
                }
            } else {
                stats.used_bytes += cur->size;
            }
        }
        cur = cur->next;
    }

    return stats;
}
