#include <stdint.h>

#include "../src/pmm.h"

void pmm_initialize(uint32_t multiboot_magic, const struct multiboot_info* mbi) {
    (void)multiboot_magic;
    (void)mbi;
}

uint32_t pmm_alloc_frame(void) {
    return 0u;
}

void pmm_free_frame(uint32_t physical_addr) {
    (void)physical_addr;
}

struct pmm_stats pmm_get_stats(void) {
    struct pmm_stats stats;
    stats.total_frames = 0u;
    stats.used_frames = 0u;
    stats.free_frames = 0u;
    return stats;
}
