#include <stdint.h>

#include "interrupts.h"
#include "keyboard.h"
#include "print.h"
#include "terminal.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION "v0.0.0-dev"
#endif

static void draw_lock_status_bar(void) {
    terminal_fill_row(24, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    terminal_write_at("LOCKS", 24, 1, VGA_COLOR_WHITE, VGA_COLOR_BLUE);

    terminal_write_at(
        keyboard_is_caps_lock_on() ? "CAPS:ON " : "CAPS:OFF",
        24,
        10,
        VGA_COLOR_WHITE,
        VGA_COLOR_BLUE
    );
    terminal_write_at(
        keyboard_is_num_lock_on() ? "NUM:ON " : "NUM:OFF",
        24,
        24,
        VGA_COLOR_WHITE,
        VGA_COLOR_BLUE
    );
    terminal_write_at(
        keyboard_is_scroll_lock_on() ? "SCRL:ON " : "SCRL:OFF",
        24,
        37,
        VGA_COLOR_WHITE,
        VGA_COLOR_BLUE
    );
}

void kernel_main(void) {
    terminal_initialize();
    interrupts_initialize();
    interrupts_enable();
    draw_lock_status_bar();

    terminal_set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREEN);
    kprintln("  MINIMAL KERNEL  ");

    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprint("Kernel version: ");
    kprintln(KERNEL_VERSION);
    kprintln("Kernel booted successfully.");
    kprintln("Features:");
    kprintln(" - VGA terminal driver");
    kprintln(" - Newline, tab, and automatic scrolling");
    kprintln(" - Decimal and hexadecimal output helpers");
    kprint("Magic number: ");
    kprint_hex(0x1BADB002u);
    terminal_write_char('\n');

    kprint("Build year: ");
    kprint_dec(2026);
    terminal_write_char('\n');

    kprintln("Keyboard input is ready (IRQ1 interrupt-driven, US scancodes).");
    kprintln("Type below:");
    kprint("> ");

    uint8_t prev_caps = keyboard_is_caps_lock_on();
    uint8_t prev_num = keyboard_is_num_lock_on();
    uint8_t prev_scroll = keyboard_is_scroll_lock_on();

    for (;;) {
        const char key = keyboard_read_char();

        const uint8_t caps = keyboard_is_caps_lock_on();
        const uint8_t num = keyboard_is_num_lock_on();
        const uint8_t scroll = keyboard_is_scroll_lock_on();
        if (caps != prev_caps || num != prev_num || scroll != prev_scroll) {
            draw_lock_status_bar();
            prev_caps = caps;
            prev_num = num;
            prev_scroll = scroll;
        }

        if (key == 0) {
            __asm__ volatile("pause");
            continue;
        }

        if (key == '\r') {
            continue;
        }

        if (key == '\n') {
            terminal_write_char('\n');
            kprint("> ");
            continue;
        }

        terminal_write_char(key);
    }
}
