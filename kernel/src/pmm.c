#include "pmm.h"

#include <stdint.h>

enum {
    FRAME_SIZE = 4096u,
    MAX_FRAMES = 1048576u,
    BITMAP_WORDS = MAX_FRAMES / 32u
};

extern uint8_t _kernel_start;
extern uint8_t _kernel_end;

static uint32_t frame_bitmap[BITMAP_WORDS];
static uint32_t max_frame_seen = 0;
static uint32_t used_frames = MAX_FRAMES;

static inline void set_frame_used(uint32_t frame) {
    frame_bitmap[frame / 32u] |= (1u << (frame % 32u));
}

static inline void set_frame_free(uint32_t frame) {
    frame_bitmap[frame / 32u] &= ~(1u << (frame % 32u));
}

static inline int is_frame_used(uint32_t frame) {
    return (frame_bitmap[frame / 32u] & (1u << (frame % 32u))) != 0u;
}

static void reserve_range(uint64_t base, uint64_t length) {
    if (length == 0u) {
        return;
    }

    uint64_t start = base / FRAME_SIZE;
    uint64_t end = (base + length + FRAME_SIZE - 1u) / FRAME_SIZE;

    if (start >= MAX_FRAMES) {
        return;
    }

    if (end > MAX_FRAMES) {
        end = MAX_FRAMES;
    }

    for (uint64_t frame = start; frame < end; ++frame) {
        if (!is_frame_used((uint32_t)frame)) {
            set_frame_used((uint32_t)frame);
            ++used_frames;
        }
    }
}

static void free_range(uint64_t base, uint64_t length) {
    if (length == 0u) {
        return;
    }

    uint64_t start = base / FRAME_SIZE;
    uint64_t end = (base + length) / FRAME_SIZE;

    if (start >= MAX_FRAMES) {
        return;
    }

    if (end > MAX_FRAMES) {
        end = MAX_FRAMES;
    }

    if (end <= start) {
        return;
    }

    for (uint64_t frame = start; frame < end; ++frame) {
        if (is_frame_used((uint32_t)frame)) {
            set_frame_free((uint32_t)frame);
            --used_frames;
        }
    }

    if (end > max_frame_seen) {
        max_frame_seen = (uint32_t)end;
    }
}

void pmm_initialize(uint32_t multiboot_magic, const struct multiboot_info* mbi) {
    for (uint32_t i = 0; i < BITMAP_WORDS; ++i) {
        frame_bitmap[i] = 0xFFFFFFFFu;
    }

    max_frame_seen = 0;
    used_frames = MAX_FRAMES;

    if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC && mbi != 0 && (mbi->flags & MULTIBOOT_INFO_MEM_MAP) != 0u) {
        uint32_t cursor = mbi->mmap_addr;
        const uint32_t end = mbi->mmap_addr + mbi->mmap_length;

        while (cursor < end) {
            const struct multiboot_mmap_entry* entry = (const struct multiboot_mmap_entry*)(uintptr_t)cursor;
            if (entry->type == 1u) {
                free_range(entry->addr, entry->len);
            }
            cursor += entry->size + sizeof(entry->size);
        }
    }

    /* Keep low memory and kernel/boot metadata reserved. */
    reserve_range(0u, 0x100000u);

    const uint64_t kernel_base = (uint32_t)(uintptr_t)&_kernel_start;
    const uint64_t kernel_len = (uint32_t)((uintptr_t)&_kernel_end - (uintptr_t)&_kernel_start);
    reserve_range(kernel_base, kernel_len);

    if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC && mbi != 0) {
        reserve_range((uint32_t)(uintptr_t)mbi, sizeof(struct multiboot_info));

        if ((mbi->flags & MULTIBOOT_INFO_MEM_MAP) != 0u) {
            reserve_range(mbi->mmap_addr, mbi->mmap_length);
        }
    }

    if (max_frame_seen == 0u) {
        max_frame_seen = 256u;
    }
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t frame = 0u; frame < max_frame_seen; ++frame) {
        if (!is_frame_used(frame)) {
            set_frame_used(frame);
            ++used_frames;
            return frame * FRAME_SIZE;
        }
    }

    return 0u;
}

void pmm_free_frame(uint32_t physical_addr) {
    if ((physical_addr % FRAME_SIZE) != 0u) {
        return;
    }

    const uint32_t frame = physical_addr / FRAME_SIZE;
    if (frame >= max_frame_seen) {
        return;
    }

    if (is_frame_used(frame)) {
        set_frame_free(frame);
        --used_frames;
    }
}

struct pmm_stats pmm_get_stats(void) {
    const uint32_t total = max_frame_seen;
    const uint32_t used = used_frames > total ? total : used_frames;

    struct pmm_stats stats;
    stats.total_frames = total;
    stats.used_frames = used;
    stats.free_frames = total - used;
    return stats;
}
