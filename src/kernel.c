#include <stdint.h>

#include "keyboard.h"
#include "print.h"
#include "terminal.h"

void kernel_main(void) {
    terminal_initialize();

    terminal_set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREEN);
    kprintln("  MINIMAL KERNEL  ");

    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
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

    kprintln("Keyboard input is ready (PS/2 polling, US scancodes).");
    kprintln("Type below:");
    kprint("> ");

    for (;;) {
        const char key = keyboard_read_char();
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
