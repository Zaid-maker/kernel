#include "sbuf.h"

void sbuf_reset(char* buffer) {
    buffer[0] = '\0';
}

void sbuf_append_char(char* buffer, uint32_t cap, uint32_t* len, char c) {
    if (*len + 1u >= cap) {
        return;
    }

    buffer[*len] = c;
    ++(*len);
    buffer[*len] = '\0';
}

void sbuf_append_str(char* buffer, uint32_t cap, uint32_t* len, const char* text) {
    uint32_t i = 0;
    while (text[i] != '\0') {
        if (*len + 1u >= cap) {
            return;
        }

        buffer[*len] = text[i];
        ++(*len);
        ++i;
    }

    buffer[*len] = '\0';
}

void sbuf_append_dec_u32(char* buffer, uint32_t cap, uint32_t* len, uint32_t value) {
    if (value == 0u) {
        sbuf_append_char(buffer, cap, len, '0');
        return;
    }

    char digits[10];
    uint32_t pos = 0;
    while (value != 0u && pos < 10u) {
        digits[pos++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (pos > 0u) {
        sbuf_append_char(buffer, cap, len, digits[--pos]);
    }
}

void sbuf_append_hex_u32(char* buffer, uint32_t cap, uint32_t* len, uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";

    sbuf_append_str(buffer, cap, len, "0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        const uint32_t nibble = (value >> (uint32_t)shift) & 0xFu;
        sbuf_append_char(buffer, cap, len, hex[nibble]);
    }
}

void sbuf_append_hex64(char* buffer, uint32_t cap, uint32_t* len, uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";

    sbuf_append_str(buffer, cap, len, "0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        const uint32_t nibble = (uint32_t)((value >> (uint32_t)shift) & 0xFu);
        sbuf_append_char(buffer, cap, len, hex[nibble]);
    }
}
