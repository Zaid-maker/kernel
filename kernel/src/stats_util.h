#ifndef STATS_UTIL_H
#define STATS_UTIL_H

#include <stdint.h>

void stats_format_label_text(char* buffer, uint32_t cap, const char* label, const char* value);
void stats_format_label_dec(char* buffer, uint32_t cap, const char* label, uint32_t value);
void stats_format_label_hex32(char* buffer, uint32_t cap, const char* label, uint32_t value);
void stats_format_label_hex64(char* buffer, uint32_t cap, const char* label, uint64_t value);
void stats_format_vector_line(char* buffer, uint32_t cap, uint32_t vector, const char* name);
void stats_format_memmap_entry_line(char* buffer, uint32_t cap, uint32_t index, const char* type_name, uint64_t base, uint64_t length);

#endif
