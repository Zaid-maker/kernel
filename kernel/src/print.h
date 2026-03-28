#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>

void kprint(const char* data);
void kprintln(const char* data);
void kprint_dec(uint32_t value);
void kprint_hex(uint32_t value);

#endif
