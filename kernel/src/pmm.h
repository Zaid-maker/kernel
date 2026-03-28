#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#include "multiboot.h"

struct pmm_stats {
    uint32_t total_frames;
    uint32_t used_frames;
    uint32_t free_frames;
};

void pmm_initialize(uint32_t multiboot_magic, const struct multiboot_info* mbi);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t physical_addr);
struct pmm_stats pmm_get_stats(void);

#endif
