#ifndef HEAP_DIAG_H
#define HEAP_DIAG_H

#include <stdint.h>

#include "heap.h"

void heap_diag_reset(void);
void heap_diag_record_alloc(uint32_t addr, uint32_t size);
void heap_diag_record_failed_alloc(void);
void heap_diag_record_invalid_free(void);
void heap_diag_record_free(uint32_t addr, uint32_t size);
void heap_diag_get_counters(struct heap_diag_counters* out);
uint32_t heap_diag_trace_snapshot(struct heap_trace_record* out_records, uint32_t max_records);
const uint32_t* heap_diag_hist_bucket_limits(void);
uint32_t heap_diag_hist_bucket_count(void);

#endif
