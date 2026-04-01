#include <stdint.h>
#include <stddef.h>

#ifdef __linux__
#include <sys/mman.h>
#endif

#include "../src/pmm.h"

enum {
    PMM_STUB_FRAME_COUNT = 8,
    PMM_STUB_FRAME_SIZE = 4096u
};

static uint8_t g_frame_used[PMM_STUB_FRAME_COUNT];
static uint32_t g_frame_addrs[PMM_STUB_FRAME_COUNT];
static uint32_t g_total_frames = 0u;
static int g_pool_initialized = 0;

static void pmm_stub_init_pool(void) {
    if (g_pool_initialized) {
        return;
    }

    g_pool_initialized = 1;
    g_total_frames = 0u;

#ifdef __linux__
    const size_t pool_size = PMM_STUB_FRAME_COUNT * PMM_STUB_FRAME_SIZE;
    void* pool = MAP_FAILED;

#if defined(__x86_64__) && defined(MAP_32BIT)
    pool = mmap((void*)0, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
#else
    pool = mmap((void*)0, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

    if (pool == MAP_FAILED) {
        return;
    }

    if ((uintptr_t)pool > 0xFFFFFFFFu || ((uintptr_t)pool + pool_size - 1u) > 0xFFFFFFFFu) {
        (void)munmap(pool, pool_size);
        return;
    }

    for (size_t i = 0; i < PMM_STUB_FRAME_COUNT; ++i) {
        g_frame_addrs[i] = (uint32_t)((uintptr_t)pool + (i * PMM_STUB_FRAME_SIZE));
    }
    g_total_frames = PMM_STUB_FRAME_COUNT;
#endif
}

static void pmm_stub_reset(void) {
    for (size_t i = 0; i < g_total_frames; ++i) {
        g_frame_used[i] = 0u;
    }
}

void pmm_initialize(uint32_t multiboot_magic, const struct multiboot_info* mbi) {
    (void)multiboot_magic;
    (void)mbi;
    pmm_stub_init_pool();
    pmm_stub_reset();
}

uint32_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < g_total_frames; ++i) {
        if (!g_frame_used[i]) {
            g_frame_used[i] = 1u;
            return g_frame_addrs[i];
        }
    }

    return 0u;
}

void pmm_free_frame(uint32_t physical_addr) {
    for (size_t i = 0; i < g_total_frames; ++i) {
        if (g_frame_addrs[i] == physical_addr) {
            g_frame_used[i] = 0u;
            return;
        }
    }
}

struct pmm_stats pmm_get_stats(void) {
    struct pmm_stats stats;
    stats.total_frames = g_total_frames;
    stats.used_frames = 0u;
    stats.free_frames = 0u;

    for (size_t i = 0; i < g_total_frames; ++i) {
        if (g_frame_used[i]) {
            ++stats.used_frames;
        }
    }

    stats.free_frames = stats.total_frames - stats.used_frames;
    return stats;
}
