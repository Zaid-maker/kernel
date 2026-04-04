#include "terminal.h"

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static volatile uint16_t* const terminal_buffer = (uint16_t*)0xB8000;

static inline uint8_t vga_make_color(uint8_t fg, uint8_t bg) {
    return fg | (uint8_t)(bg << 4);
}

static inline uint16_t vga_make_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

static void terminal_clear_row(size_t row) {
    for (size_t col = 0; col < VGA_WIDTH; ++col) {
        const size_t index = row * VGA_WIDTH + col;
        terminal_buffer[index] = vga_make_entry(' ', terminal_color);
    }
}

static void terminal_scroll(void) {
    for (size_t row = 1; row < VGA_TEXT_HEIGHT; ++row) {
        for (size_t col = 0; col < VGA_WIDTH; ++col) {
            const size_t from = row * VGA_WIDTH + col;
            const size_t to = (row - 1) * VGA_WIDTH + col;
            terminal_buffer[to] = terminal_buffer[from];
        }
    }

    terminal_clear_row(VGA_TEXT_HEIGHT - 1);
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (size_t y = 0; y < VGA_HEIGHT; ++y) {
        terminal_clear_row(y);
    }
}

void terminal_set_color(uint8_t fg, uint8_t bg) {
    terminal_color = vga_make_color(fg, bg);
}

void terminal_write_char(char c) {
    if (c == '\n') {
        terminal_column = 0;
        ++terminal_row;
    } else if (c == '\b') {
        if (terminal_column > 0) {
            --terminal_column;
        } else if (terminal_row > 0) {
            --terminal_row;
            terminal_column = VGA_WIDTH - 1;
        } else {
            return;
        }

        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = vga_make_entry(' ', terminal_color);
        return;
    } else if (c == '\r') {
        terminal_column = 0;
    } else if (c == '\t') {
        const size_t spaces = 4 - (terminal_column % 4);
        for (size_t i = 0; i < spaces; ++i) {
            terminal_write_char(' ');
        }
        return;
    } else {
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = vga_make_entry((unsigned char)c, terminal_color);
        ++terminal_column;
    }

    if (terminal_column >= VGA_WIDTH) {
        terminal_column = 0;
        ++terminal_row;
    }

    if (terminal_row >= VGA_TEXT_HEIGHT) {
        terminal_scroll();
        terminal_row = VGA_TEXT_HEIGHT - 1;
    }
}

void terminal_write(const char* data) {
    size_t i = 0;
    while (data[i] != '\0') {
        terminal_write_char(data[i]);
        ++i;
    }
}

void terminal_writeln(const char* data) {
    terminal_write(data);
    terminal_write_char('\n');
}

void terminal_write_at(const char* data, size_t row, size_t col, uint8_t fg, uint8_t bg) {
    if (row >= VGA_HEIGHT || col >= VGA_WIDTH) {
        return;
    }

    const uint8_t color = vga_make_color(fg, bg);
    size_t i = 0;
    while (data[i] != '\0' && (col + i) < VGA_WIDTH) {
        const size_t index = row * VGA_WIDTH + (col + i);
        terminal_buffer[index] = vga_make_entry((unsigned char)data[i], color);
        ++i;
    }
}

void terminal_fill_row(size_t row, char c, uint8_t fg, uint8_t bg) {
    if (row >= VGA_HEIGHT) {
        return;
    }

    const uint8_t color = vga_make_color(fg, bg);
    for (size_t col = 0; col < VGA_WIDTH; ++col) {
        const size_t index = row * VGA_WIDTH + col;
        terminal_buffer[index] = vga_make_entry((unsigned char)c, color);
    }
}

uint16_t terminal_getentry_at(size_t row, size_t col) {
    if (row >= VGA_HEIGHT || col >= VGA_WIDTH) {
        return 0u;
    }

    const size_t index = row * VGA_WIDTH + col;
    return terminal_buffer[index];
}
