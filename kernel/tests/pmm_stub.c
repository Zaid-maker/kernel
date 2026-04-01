#include <stdint.h>
#include <stddef.h>

#include "../src/pmm.h"

enum {
    PMM_STUB_FRAME_COUNT = 8,
    PMM_STUB_FRAME_SIZE = 4096u
};

static uint8_t g_frames[PMM_STUB_FRAME_COUNT][PMM_STUB_FRAME_SIZE] __attribute__((aligned(4096)));
static uint8_t g_frame_used[PMM_STUB_FRAME_COUNT];

static void pmm_stub_reset(void) {
    for (size_t i = 0; i < PMM_STUB_FRAME_COUNT; ++i) {
        g_frame_used[i] = 0u;
    }
}

void pmm_initialize(uint32_t multiboot_magic, const struct multiboot_info* mbi) {
    (void)multiboot_magic;
    (void)mbi;
    pmm_stub_reset();
}

uint32_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < PMM_STUB_FRAME_COUNT; ++i) {
        if (!g_frame_used[i]) {
            g_frame_used[i] = 1u;
            return (uint32_t)(uintptr_t)&g_frames[i][0];
        }
    }

    return 0u;
}

void pmm_free_frame(uint32_t physical_addr) {
    for (size_t i = 0; i < PMM_STUB_FRAME_COUNT; ++i) {
        if ((uint32_t)(uintptr_t)&g_frames[i][0] == physical_addr) {
            g_frame_used[i] = 0u;
            return;
        }
    }
}

struct pmm_stats pmm_get_stats(void) {
    struct pmm_stats stats;
    stats.total_frames = PMM_STUB_FRAME_COUNT;
    stats.used_frames = 0u;
    stats.free_frames = 0u;

    for (size_t i = 0; i < PMM_STUB_FRAME_COUNT; ++i) {
        if (g_frame_used[i]) {
            ++stats.used_frames;
        }
    }

    stats.free_frames = stats.total_frames - stats.used_frames;
    return stats;
}
