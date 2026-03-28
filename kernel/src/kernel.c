#include <stdint.h>

#include "interrupts.h"
#include "keyboard.h"
#include "print.h"
#include "timer.h"
#include "terminal.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION "v0.0.0-dev"
#endif

enum {
    SHELL_INPUT_MAX = 128
};

static int str_equal(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static void print_uptime_line(void) {
    kprint("Uptime: ");
    kprint_dec(timer_seconds());
    terminal_write_char('.');

    const uint32_t centis = timer_centiseconds();
    terminal_write_char((char)('0' + ((centis / 10u) % 10u)));
    terminal_write_char((char)('0' + (centis % 10u)));
    kprintln("s");
}

static void print_lock_line(void) {
    kprint("CAPS=");
    kprint(keyboard_is_caps_lock_on() ? "ON" : "OFF");
    kprint(" NUM=");
    kprint(keyboard_is_num_lock_on() ? "ON" : "OFF");
    kprint(" SCRL=");
    kprintln(keyboard_is_scroll_lock_on() ? "ON" : "OFF");
}

static void shell_print_help(void) {
    kprintln("Commands:");
    kprintln(" - help");
    kprintln(" - clear");
    kprintln(" - version");
    kprintln(" - locks");
    kprintln(" - uptime");
}

static void shell_run_command(const char* cmd) {
    if (cmd[0] == '\0') {
        return;
    }

    if (str_equal(cmd, "help")) {
        shell_print_help();
        return;
    }

    if (str_equal(cmd, "clear")) {
        terminal_initialize();
        return;
    }

    if (str_equal(cmd, "version")) {
        kprint("Kernel version: ");
        kprintln(KERNEL_VERSION);
        return;
    }

    if (str_equal(cmd, "locks")) {
        print_lock_line();
        return;
    }

    if (str_equal(cmd, "uptime")) {
        print_uptime_line();
        return;
    }

    kprint("Unknown command: ");
    kprintln(cmd);
}

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

    const uint32_t seconds = timer_seconds();
    const uint32_t centis = timer_centiseconds();

    char uptime[16];
    uptime[0] = 'T';
    uptime[1] = '+';

    uint32_t temp = seconds;
    char digits[10];
    uint32_t pos = 0;
    do {
        digits[pos++] = (char)('0' + (temp % 10u));
        temp /= 10u;
    } while (temp != 0u && pos < 10u);

    uint32_t out = 2;
    while (pos > 0) {
        uptime[out++] = digits[--pos];
    }

    uptime[out++] = '.';
    uptime[out++] = (char)('0' + ((centis / 10u) % 10u));
    uptime[out++] = (char)('0' + (centis % 10u));
    uptime[out++] = 's';
    uptime[out] = '\0';

    terminal_write_at(uptime, 24, 65, VGA_COLOR_WHITE, VGA_COLOR_BLUE);
}

void kernel_main(void) {
    terminal_initialize();
    interrupts_initialize();
    timer_initialize(100u);
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
    kprintln("Type below (help, clear, version, locks, uptime):");
    kprint("> ");

    char input[SHELL_INPUT_MAX];
    uint32_t input_len = 0;

    uint8_t prev_caps = keyboard_is_caps_lock_on();
    uint8_t prev_num = keyboard_is_num_lock_on();
    uint8_t prev_scroll = keyboard_is_scroll_lock_on();
    uint32_t prev_uptime_seconds = timer_seconds();

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

        const uint32_t uptime_seconds = timer_seconds();
        if (uptime_seconds != prev_uptime_seconds) {
            draw_lock_status_bar();
            prev_uptime_seconds = uptime_seconds;
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
            input[input_len] = '\0';
            shell_run_command(input);
            input_len = 0;

            draw_lock_status_bar();
            prev_caps = keyboard_is_caps_lock_on();
            prev_num = keyboard_is_num_lock_on();
            prev_scroll = keyboard_is_scroll_lock_on();
            prev_uptime_seconds = timer_seconds();

            kprint("> ");
            continue;
        }

        if (key == '\b') {
            if (input_len > 0u) {
                --input_len;
                terminal_write_char('\b');
            }
            continue;
        }

        if ((unsigned char)key < 32u || (unsigned char)key > 126u) {
            continue;
        }

        if (input_len + 1u >= SHELL_INPUT_MAX) {
            continue;
        }

        input[input_len++] = key;

        terminal_write_char(key);
    }
}
