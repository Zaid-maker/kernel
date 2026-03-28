#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

struct heap_stats {
    uint32_t total_bytes;
    uint32_t used_bytes;
    uint32_t free_bytes;
    uint32_t block_count;
    uint32_t free_blocks;
};

void heap_initialize(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
struct heap_stats heap_get_stats(void);

#endif
