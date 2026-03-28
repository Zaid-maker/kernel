#include <stdint.h>

#include "interrupts.h"
#include "keyboard.h"
#include "multiboot.h"
#include "pmm.h"
#include "print.h"
#include "timer.h"
#include "terminal.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION "v0.0.0-dev"
#endif

enum {
    SHELL_INPUT_MAX = 128
};

static uint32_t g_multiboot_magic = 0;
static const struct multiboot_info* g_multiboot_info = (const struct multiboot_info*)0;

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
    kprintln(" - memmap");
    kprintln(" - pmm");
}

static void shell_print_pmm(void) {
    const struct pmm_stats stats = pmm_get_stats();
    const uint64_t total_bytes = (uint64_t)stats.total_frames * 4096u;
    const uint64_t used_bytes = (uint64_t)stats.used_frames * 4096u;
    const uint64_t free_bytes = (uint64_t)stats.free_frames * 4096u;

    kprintln("PMM stats:");
    kprint(" total frames: ");
    kprint_dec(stats.total_frames);
    terminal_write_char('\n');
    kprint(" used frames : ");
    kprint_dec(stats.used_frames);
    terminal_write_char('\n');
    kprint(" free frames : ");
    kprint_dec(stats.free_frames);
    terminal_write_char('\n');

    kprint(" total bytes : ");
    kprint_hex64(total_bytes);
    terminal_write_char('\n');
    kprint(" used bytes  : ");
    kprint_hex64(used_bytes);
    terminal_write_char('\n');
    kprint(" free bytes  : ");
    kprint_hex64(free_bytes);
    terminal_write_char('\n');
}

static const char* mem_type_name(uint32_t type) {
    switch (type) {
        case 1u: return "available";
        case 2u: return "reserved";
        case 3u: return "acpi-reclaim";
        case 4u: return "acpi-nvs";
        case 5u: return "bad-ram";
        default: return "unknown";
    }
}

static void shell_print_memmap(void) {
    if (g_multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC || g_multiboot_info == 0) {
        kprintln("Multiboot info unavailable.");
        return;
    }

    const struct multiboot_info* mbi = g_multiboot_info;
    if ((mbi->flags & MULTIBOOT_INFO_MEM_MAP) == 0u) {
        kprintln("No memory map provided by bootloader.");
        return;
    }

    uint32_t cursor = mbi->mmap_addr;
    const uint32_t end = mbi->mmap_addr + mbi->mmap_length;
    uint32_t index = 0;
    uint32_t available_count = 0;
    uint64_t available_bytes = 0;

    kprintln("Memory map entries:");
    while (cursor < end) {
        const struct multiboot_mmap_entry* entry = (const struct multiboot_mmap_entry*)(uintptr_t)cursor;

        kprint(" #");
        kprint_dec(index);
        kprint(" ");
        kprint(mem_type_name(entry->type));
        kprint(" base=");
        kprint_hex64(entry->addr);
        kprint(" len=");
        kprint_hex64(entry->len);
        terminal_write_char('\n');

        if (entry->type == 1u) {
            ++available_count;
            available_bytes += entry->len;
        }

        cursor += entry->size + sizeof(entry->size);
        ++index;
    }

    kprint("Available regions: ");
    kprint_dec(available_count);
    terminal_write_char('\n');
    kprint("Available total bytes: ");
    kprint_hex64(available_bytes);
    terminal_write_char('\n');
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

    if (str_equal(cmd, "memmap")) {
        shell_print_memmap();
        return;
    }

    if (str_equal(cmd, "pmm")) {
        shell_print_pmm();
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

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    g_multiboot_magic = multiboot_magic;
    g_multiboot_info = (const struct multiboot_info*)(uintptr_t)multiboot_info_addr;

    terminal_initialize();
    pmm_initialize(g_multiboot_magic, g_multiboot_info);
    interrupts_initialize();
    timer_initialize(100u);
    interrupts_enable();
    draw_lock_status_bar();

    terminal_set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREEN);
    kprintln(" PROJECT SIGMABOOT ");

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
    kprintln("Type below (help, clear, version, locks, uptime, memmap, pmm):");
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
