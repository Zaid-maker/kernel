#ifndef SBUF_H
#define SBUF_H

#include <stdint.h>

void sbuf_reset(char* buffer);
void sbuf_append_char(char* buffer, uint32_t cap, uint32_t* len, char c);
void sbuf_append_str(char* buffer, uint32_t cap, uint32_t* len, const char* text);
void sbuf_append_dec_u32(char* buffer, uint32_t cap, uint32_t* len, uint32_t value);
void sbuf_append_hex_u32(char* buffer, uint32_t cap, uint32_t* len, uint32_t value);
void sbuf_append_hex64(char* buffer, uint32_t cap, uint32_t* len, uint64_t value);

#endif
