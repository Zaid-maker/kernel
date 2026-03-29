#include "print.h"

#include "heap.h"
#include "terminal.h"

enum {
    PRINT_DEC_BUFFER_SIZE = 11
};

static char g_dec_buffer_fallback[PRINT_DEC_BUFFER_SIZE];
static char* g_dec_buffer = g_dec_buffer_fallback;

int print_initialize(void) {
    char* heap_buffer = (char*)kmalloc(PRINT_DEC_BUFFER_SIZE);
    if (heap_buffer == 0) {
        g_dec_buffer = g_dec_buffer_fallback;
        return 0;
    }

    g_dec_buffer = heap_buffer;
    return 1;
}

void kprint(const char* data) {
    terminal_write(data);
}

void kprintln(const char* data) {
    terminal_writeln(data);
}

void kprint_dec(uint32_t value) {
    uint32_t pos = 0;

    if (value == 0) {
        terminal_write_char('0');
        return;
    }

    while (value > 0) {
        g_dec_buffer[pos++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (pos > 0) {
        terminal_write_char(g_dec_buffer[--pos]);
    }
}

void kprint_hex(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";

    terminal_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        terminal_write_char(hex[(value >> (uint32_t)shift) & 0xF]);
    }
}

void kprint_hex64(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";

    terminal_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        terminal_write_char(hex[(value >> (uint32_t)shift) & 0xFu]);
    }
}
