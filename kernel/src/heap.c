#include "heap.h"

#include <stdint.h>

#include "heap_diag.h"
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

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1u) & ~(align - 1u);
}

static uint32_t block_overhead(void) {
    return (uint32_t)sizeof(struct heap_block);
}

static void heap_integrity_report_reset(struct heap_integrity_report* report) {
    report->blocks_scanned = 0u;
    report->corrupted_headers = 0u;
    report->split_alignment_issues = 0u;
    report->adjacent_unmerged_free_pairs = 0u;
    report->next_pointer_regressions = 0u;
}

static int heap_check_integrity_internal(struct heap_integrity_report* report) {
    struct heap_integrity_report local;
    struct heap_integrity_report* out = report != 0 ? report : &local;
    heap_integrity_report_reset(out);

    struct heap_block* cur = g_heap_head;
    while (cur != 0) {
        ++out->blocks_scanned;

        if (cur->magic != HEAP_MAGIC) {
            ++out->corrupted_headers;
            break;
        }

        if ((cur->size & (HEAP_ALIGN - 1u)) != 0u) {
            ++out->split_alignment_issues;
        }

        if (cur->next != 0) {
            if ((uintptr_t)cur->next <= (uintptr_t)cur) {
                ++out->next_pointer_regressions;
                break;
            }

            uintptr_t cur_end = (uintptr_t)cur + block_overhead() + cur->size;
            if (cur->free && cur_end == (uintptr_t)cur->next && cur->next->free) {
                ++out->adjacent_unmerged_free_pairs;
            }
        }

        cur = cur->next;
    }

    return out->corrupted_headers == 0u
        && out->split_alignment_issues == 0u
        && out->adjacent_unmerged_free_pairs == 0u
        && out->next_pointer_regressions == 0u;
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
    (void)heap_check_integrity_internal((struct heap_integrity_report*)0);
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
                heap_diag_record_alloc((uint32_t)(uintptr_t)out, cur->size);
                (void)heap_check_integrity_internal((struct heap_integrity_report*)0);
                return out;
            }
            cur = cur->next;
        }

        if (!heap_grow()) {
            heap_diag_record_failed_alloc();
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
        heap_diag_record_invalid_free();
        return;
    }

    if (block->free) {
        heap_diag_record_invalid_free();
        return;
    }

    heap_diag_record_free((uint32_t)(uintptr_t)ptr, block->size);
    block->free = 1;
    merge_free_neighbors();
    (void)heap_check_integrity_internal((struct heap_integrity_report*)0);
}

void heap_get_diag_counters(struct heap_diag_counters* out) {
    heap_diag_get_counters(out);
}

uint32_t heap_trace_snapshot(struct heap_trace_record* out_records, uint32_t max_records) {
    return heap_diag_trace_snapshot(out_records, max_records);
}

const uint32_t* heap_hist_bucket_limits(void) {
    return heap_diag_hist_bucket_limits();
}

uint32_t heap_hist_bucket_count(void) {
    return heap_diag_hist_bucket_count();
}

int heap_check_integrity(struct heap_integrity_report* out_report) {
    return heap_check_integrity_internal(out_report);
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
