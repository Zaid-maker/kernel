#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>

int print_initialize(void);
void kprint(const char* data);
void kprintln(const char* data);
void kprint_dec(uint32_t value);
void kprint_hex(uint32_t value);
void kprint_hex64(uint64_t value);

#endif
