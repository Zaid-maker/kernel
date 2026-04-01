#include <stdint.h>

#include "interrupts.h"
#include "keyboard.h"
#include "multiboot.h"
#include "heap.h"
#include "pmm.h"
#include "print.h"
#include "sbuf.h"
#include "stats_util.h"
#include "timer.h"
#include "terminal.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION "v0.0.0-dev"
#endif

enum {
    SHELL_INPUT_INITIAL = 128,
    SHELL_INPUT_MAX = 1024,
    SHELL_HISTORY_SIZE = 16,
    HEAP_STRESS_SLOTS = 64,
    HEAP_STRESS_DEFAULT_ROUNDS = 256,
    HEAP_STRESS_MAX_ROUNDS = 8192,
    HEAP_LEAKS_DEFAULT_LIMIT = 16,
    HEAP_LEAKS_MAX_LIMIT = 64,
    STATUS_UPTIME_BUFFER_SIZE = 24,
    MEMMAP_LINE_BUFFER_SIZE = 160,
    PMM_STATS_BUFFER_SIZE = 160,
    HEAP_STATS_BUFFER_SIZE = 160
};

static uint32_t g_multiboot_magic = 0;
static const struct multiboot_info* g_multiboot_info = (const struct multiboot_info*)0;
static char* g_status_uptime_buffer = (char*)0;
static char* g_memmap_line_buffer = (char*)0;
static char* g_pmm_stats_buffer = (char*)0;
static char* g_heap_stats_buffer = (char*)0;
static char* g_command_history[SHELL_HISTORY_SIZE];
static uint32_t g_command_history_head = 0;
static uint32_t g_command_history_count = 0;

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

static int str_starts_with(const char* text, const char* prefix) {
    uint32_t i = 0;
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static const char* skip_spaces(const char* text) {
    while (*text == ' ') {
        ++text;
    }
    return text;
}

static int parse_u32_strict(const char* text, uint32_t* out_value) {
    const char* p = skip_spaces(text);
    uint32_t value = 0u;
    int have_digit = 0;

    while (*p >= '0' && *p <= '9') {
        const uint32_t digit = (uint32_t)(*p - '0');
        if (value > 429496729u || (value == 429496729u && digit > 5u)) {
            return 0;
        }
        value = value * 10u + digit;
        have_digit = 1;
        ++p;
    }

    if (!have_digit) {
        return 0;
    }

    p = skip_spaces(p);
    if (*p != '\0') {
        return 0;
    }

    *out_value = value;
    return 1;
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
    kprintln(" - heap");
    kprintln(" - heapcheck");
    kprintln(" - heaptriage");
    kprintln(" - heapfrag");
    kprintln(" - heapstress [rounds]");
    kprintln(" - heaphist");
    kprintln(" - heapleaks [max]");
    kprintln(" - history");
}

static uint32_t cstr_len(const char* s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static char* cstr_dup_heap(const char* s) {
    const uint32_t len = cstr_len(s);
    char* out = (char*)kmalloc(len + 1u);
    if (out == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < len; ++i) {
        out[i] = s[i];
    }
    out[len] = '\0';
    return out;
}

static int shell_store_history(const char* cmd) {
    uint32_t len = cstr_len(cmd);
    if (len == 0u) {
        return 1;
    }

    char* next = cstr_dup_heap(cmd);
    if (next == 0) {
        return 0;
    }

    if (g_command_history_count < SHELL_HISTORY_SIZE) {
        const uint32_t idx = (g_command_history_head + g_command_history_count) % SHELL_HISTORY_SIZE;
        g_command_history[idx] = next;
        ++g_command_history_count;
        return 1;
    }

    char* replaced = g_command_history[g_command_history_head];
    if (replaced != 0) {
        kfree(replaced);
    }
    g_command_history[g_command_history_head] = next;
    g_command_history_head = (g_command_history_head + 1u) % SHELL_HISTORY_SIZE;
    return 1;
}

static void shell_print_history(void) {
    if (g_command_history_count == 0u) {
        kprintln("No command history yet.");
        return;
    }

    kprintln("Recent commands:");
    for (uint32_t i = 0; i < g_command_history_count; ++i) {
        const uint32_t idx = (g_command_history_head + i) % SHELL_HISTORY_SIZE;
        kprint(" ");
        kprint_dec(i + 1u);
        kprint(": ");
        kprintln(g_command_history[idx]);
    }
}

static void shell_print_heap(void) {
    const struct heap_stats stats = heap_get_stats();

    char heap_line_fallback[HEAP_STATS_BUFFER_SIZE];
    char* line = g_heap_stats_buffer != 0 ? g_heap_stats_buffer : heap_line_fallback;

    kprintln("Heap stats:");

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " total bytes : ", stats.total_bytes);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " used bytes  : ", stats.used_bytes);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " free bytes  : ", stats.free_bytes);
    kprintln(line);

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " blocks      : ", stats.block_count);
    kprintln(line);

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " free blocks : ", stats.free_blocks);
    kprintln(line);
}

static void shell_print_heap_fragmentation(void) {
    const struct heap_stats stats = heap_get_stats();
    const uint32_t alloc_blocks = stats.block_count - stats.free_blocks;
    uint32_t external_frag_permille = 0u;

    if (stats.free_bytes > 0u && stats.largest_free_block < stats.free_bytes) {
        external_frag_permille = ((stats.free_bytes - stats.largest_free_block) * 1000u) / stats.free_bytes;
    }

    char heap_line_fallback[HEAP_STATS_BUFFER_SIZE];
    char* line = g_heap_stats_buffer != 0 ? g_heap_stats_buffer : heap_line_fallback;

    kprintln("Heap fragmentation:");

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " alloc blocks : ", alloc_blocks);
    kprintln(line);

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " free blocks  : ", stats.free_blocks);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " free bytes   : ", stats.free_bytes);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " largest free : ", stats.largest_free_block);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " smallest free: ", stats.smallest_free_block);
    kprintln(line);

    kprint(" external frag: ");
    kprint_dec(external_frag_permille / 10u);
    terminal_write_char('.');
    kprint_dec(external_frag_permille % 10u);
    kprintln("%");
}

static void shell_print_heap_integrity(void) {
    struct heap_integrity_report report;
    const int ok = heap_check_integrity(&report);

    char heap_line_fallback[HEAP_STATS_BUFFER_SIZE];
    char* line = g_heap_stats_buffer != 0 ? g_heap_stats_buffer : heap_line_fallback;

    kprintln("Heap integrity:");
    stats_format_label_text(line, HEAP_STATS_BUFFER_SIZE, " status      : ", ok ? "OK" : "FAIL");
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " scanned     : ", report.blocks_scanned);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " bad headers : ", report.corrupted_headers);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " bad align   : ", report.split_alignment_issues);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " unmerged adj: ", report.adjacent_unmerged_free_pairs);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " bad next ptr: ", report.next_pointer_regressions);
    kprintln(line);
}

static void shell_print_heap_triage(void) {
    struct heap_integrity_report report;
    const int ok = heap_check_integrity(&report);

    char heap_line_fallback[HEAP_STATS_BUFFER_SIZE];
    char* line = g_heap_stats_buffer != 0 ? g_heap_stats_buffer : heap_line_fallback;

    sbuf_reset(line);
    uint32_t len = 0u;
    sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, "Heap triage: ");
    sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, ok ? "OK" : "FAIL");
    sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " scanned=");
    sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, report.blocks_scanned);
    sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " bad=");
    sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, report.corrupted_headers);
    sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " align=");
    sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, report.split_alignment_issues);
    sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " unmerged=");
    sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, report.adjacent_unmerged_free_pairs);
    sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " next=");
    sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, report.next_pointer_regressions);
    kprintln(line);
}

static void shell_run_heap_stress(uint32_t rounds) {
    void* slots[HEAP_STRESS_SLOTS];
    for (uint32_t i = 0; i < HEAP_STRESS_SLOTS; ++i) {
        slots[i] = (void*)0;
    }

    uint32_t alloc_attempts = 0u;
    uint32_t alloc_success = 0u;
    uint32_t alloc_fail = 0u;
    uint32_t free_ops = 0u;
    const struct heap_stats before = heap_get_stats();
    uint32_t peak_used_bytes = before.used_bytes;

    for (uint32_t round = 0; round < rounds; ++round) {
        const uint32_t index = (round * 37u + 11u) % HEAP_STRESS_SLOTS;

        if (slots[index] != 0 && ((round & 1u) == 0u || (round % 5u) == 0u)) {
            kfree(slots[index]);
            slots[index] = (void*)0;
            ++free_ops;
        } else if (slots[index] == 0) {
            const uint32_t size = 16u + ((round * 53u) % 1024u);
            ++alloc_attempts;
            void* ptr = kmalloc(size);
            if (ptr == 0) {
                ++alloc_fail;
            } else {
                uint8_t* bytes = (uint8_t*)ptr;
                bytes[0] = (uint8_t)(round & 0xFFu);
                bytes[size - 1u] = (uint8_t)((round >> 1u) & 0xFFu);
                slots[index] = ptr;
                ++alloc_success;
            }
        }

        const struct heap_stats mid = heap_get_stats();
        if (mid.used_bytes > peak_used_bytes) {
            peak_used_bytes = mid.used_bytes;
        }
    }

    for (uint32_t i = 0; i < HEAP_STRESS_SLOTS; ++i) {
        if (slots[i] != 0) {
            kfree(slots[i]);
            slots[i] = (void*)0;
            ++free_ops;
        }
    }

    const struct heap_stats after = heap_get_stats();

    char heap_line_fallback[HEAP_STATS_BUFFER_SIZE];
    char* line = g_heap_stats_buffer != 0 ? g_heap_stats_buffer : heap_line_fallback;

    kprintln("Heap allocator stress:");

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " rounds       : ", rounds);
    kprintln(line);

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " alloc tries  : ", alloc_attempts);
    kprintln(line);

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " alloc ok     : ", alloc_success);
    kprintln(line);

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " alloc failed : ", alloc_fail);
    kprintln(line);

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " frees        : ", free_ops);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " peak used    : ", peak_used_bytes);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " used before  : ", before.used_bytes);
    kprintln(line);

    stats_format_label_hex32(line, HEAP_STATS_BUFFER_SIZE, " used after   : ", after.used_bytes);
    kprintln(line);

    if (after.used_bytes != before.used_bytes) {
        kprintln("Warning: heap stress left different used-byte count.");
    }
}

static void shell_print_heap_histogram(void) {
    struct heap_diag_counters diag;
    heap_get_diag_counters(&diag);

    const uint32_t* limits = heap_hist_bucket_limits();
    const uint32_t bucket_count = heap_hist_bucket_count();

    char heap_line_fallback[HEAP_STATS_BUFFER_SIZE];
    char* line = g_heap_stats_buffer != 0 ? g_heap_stats_buffer : heap_line_fallback;

    kprintln("Heap live allocation histogram:");

    for (uint32_t i = 0; i < bucket_count; ++i) {
        sbuf_reset(line);
        uint32_t len = 0u;
        sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " ");
        if (i == bucket_count - 1u) {
            sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, ">1024");
        } else {
            sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, "<=");
            sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, limits[i]);
        }
        sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " : blocks=");
        sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, diag.hist_live_blocks[i]);
        sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " bytes=");
        sbuf_append_hex_u32(line, HEAP_STATS_BUFFER_SIZE, &len, diag.hist_live_bytes[i]);
        kprintln(line);
    }

    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " live allocs  : ", diag.live_allocations);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " peak live    : ", diag.peak_live_allocations);
    kprintln(line);
}

static void shell_print_heap_leaks(uint32_t max_to_show) {
    struct heap_diag_counters diag;
    heap_get_diag_counters(&diag);

    struct heap_trace_record records[HEAP_LEAKS_MAX_LIMIT];
    const uint32_t snap_limit = max_to_show > HEAP_LEAKS_MAX_LIMIT ? HEAP_LEAKS_MAX_LIMIT : max_to_show;
    const uint32_t count = heap_trace_snapshot(records, snap_limit);

    char heap_line_fallback[HEAP_STATS_BUFFER_SIZE];
    char* line = g_heap_stats_buffer != 0 ? g_heap_stats_buffer : heap_line_fallback;

    kprintln("Heap live allocation trace:");
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " showing      : ", count);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " live total   : ", diag.live_allocations);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " alloc calls  : ", diag.alloc_calls);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " free calls   : ", diag.free_calls);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " alloc failed : ", diag.failed_alloc_calls);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " invalid free : ", diag.invalid_free_calls);
    kprintln(line);
    stats_format_label_dec(line, HEAP_STATS_BUFFER_SIZE, " trace ovf    : ", diag.trace_overflow_events);
    kprintln(line);

    for (uint32_t i = 0; i < count; ++i) {
        sbuf_reset(line);
        uint32_t len = 0u;
        sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " #");
        sbuf_append_dec_u32(line, HEAP_STATS_BUFFER_SIZE, &len, i + 1u);
        sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " addr=");
        sbuf_append_hex_u32(line, HEAP_STATS_BUFFER_SIZE, &len, records[i].addr);
        sbuf_append_str(line, HEAP_STATS_BUFFER_SIZE, &len, " size=");
        sbuf_append_hex_u32(line, HEAP_STATS_BUFFER_SIZE, &len, records[i].size);
        kprintln(line);
    }
}

static void shell_print_pmm(void) {
    const struct pmm_stats stats = pmm_get_stats();
    const uint64_t total_bytes = (uint64_t)stats.total_frames * 4096u;
    const uint64_t used_bytes = (uint64_t)stats.used_frames * 4096u;
    const uint64_t free_bytes = (uint64_t)stats.free_frames * 4096u;

    char pmm_line_fallback[PMM_STATS_BUFFER_SIZE];
    char* line = g_pmm_stats_buffer != 0 ? g_pmm_stats_buffer : pmm_line_fallback;

    kprintln("PMM stats:");

    stats_format_label_dec(line, PMM_STATS_BUFFER_SIZE, " total frames: ", stats.total_frames);
    kprintln(line);

    stats_format_label_dec(line, PMM_STATS_BUFFER_SIZE, " used frames : ", stats.used_frames);
    kprintln(line);

    stats_format_label_dec(line, PMM_STATS_BUFFER_SIZE, " free frames : ", stats.free_frames);
    kprintln(line);

    stats_format_label_hex64(line, PMM_STATS_BUFFER_SIZE, " total bytes : ", total_bytes);
    kprintln(line);

    stats_format_label_hex64(line, PMM_STATS_BUFFER_SIZE, " used bytes  : ", used_bytes);
    kprintln(line);

    stats_format_label_hex64(line, PMM_STATS_BUFFER_SIZE, " free bytes  : ", free_bytes);
    kprintln(line);
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
    char memmap_line_fallback[MEMMAP_LINE_BUFFER_SIZE];
    char* line = g_memmap_line_buffer != 0 ? g_memmap_line_buffer : memmap_line_fallback;

    kprintln("Memory map entries:");
    while (cursor < end) {
        const struct multiboot_mmap_entry* entry = (const struct multiboot_mmap_entry*)(uintptr_t)cursor;

        stats_format_memmap_entry_line(
            line,
            MEMMAP_LINE_BUFFER_SIZE,
            index,
            mem_type_name(entry->type),
            entry->addr,
            entry->len
        );
        kprintln(line);

        if (entry->type == 1u) {
            ++available_count;
            available_bytes += entry->len;
        }

        cursor += entry->size + sizeof(entry->size);
        ++index;
    }

    stats_format_label_dec(line, MEMMAP_LINE_BUFFER_SIZE, "Available regions: ", available_count);
    kprintln(line);

    stats_format_label_hex64(line, MEMMAP_LINE_BUFFER_SIZE, "Available total bytes: ", available_bytes);
    kprintln(line);
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

    if (str_equal(cmd, "heap")) {
        shell_print_heap();
        return;
    }

    if (str_equal(cmd, "heapcheck")) {
        shell_print_heap_integrity();
        return;
    }

    if (str_equal(cmd, "heaptriage")) {
        shell_print_heap_triage();
        return;
    }

    if (str_equal(cmd, "heapfrag")) {
        shell_print_heap_fragmentation();
        return;
    }

    if (str_equal(cmd, "heapstress")) {
        shell_run_heap_stress(HEAP_STRESS_DEFAULT_ROUNDS);
        return;
    }

    if (str_starts_with(cmd, "heapstress ")) {
        uint32_t rounds = 0u;
        if (!parse_u32_strict(cmd + 10, &rounds) || rounds == 0u) {
            kprintln("Usage: heapstress [rounds>0]");
            return;
        }

        if (rounds > HEAP_STRESS_MAX_ROUNDS) {
            rounds = HEAP_STRESS_MAX_ROUNDS;
        }

        shell_run_heap_stress(rounds);
        return;
    }

    if (str_equal(cmd, "heaphist")) {
        shell_print_heap_histogram();
        return;
    }

    if (str_equal(cmd, "heapleaks")) {
        shell_print_heap_leaks(HEAP_LEAKS_DEFAULT_LIMIT);
        return;
    }

    if (str_starts_with(cmd, "heapleaks ")) {
        uint32_t limit = 0u;
        if (!parse_u32_strict(cmd + 10, &limit) || limit == 0u) {
            kprintln("Usage: heapleaks [max>0]");
            return;
        }

        if (limit > HEAP_LEAKS_MAX_LIMIT) {
            limit = HEAP_LEAKS_MAX_LIMIT;
        }

        shell_print_heap_leaks(limit);
        return;
    }

    if (str_equal(cmd, "history")) {
        shell_print_history();
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

    char uptime_fallback[STATUS_UPTIME_BUFFER_SIZE];
    char* uptime = g_status_uptime_buffer != 0 ? g_status_uptime_buffer : uptime_fallback;
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
    heap_initialize();
    const int print_heap_buffer_ok = print_initialize();
    const int keyboard_heap_queue_ok = keyboard_initialize();
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
    if (!print_heap_buffer_ok) {
        kprintln("Warning: print decimal buffer heap allocation failed; using static fallback.");
    }
    if (!keyboard_heap_queue_ok) {
        kprintln("Warning: keyboard queue heap allocation failed; using static fallback queue.");
    }
    kprintln("Type below (help, clear, version, locks, uptime, memmap, pmm, heap, heapcheck, heaptriage, heapfrag, heapstress [rounds], heaphist, heapleaks [max], history):");

    g_status_uptime_buffer = (char*)kmalloc(STATUS_UPTIME_BUFFER_SIZE);
    if (g_status_uptime_buffer == 0) {
        kprintln("Warning: status bar uptime uses stack fallback (heap alloc failed).");
    }

    g_memmap_line_buffer = (char*)kmalloc(MEMMAP_LINE_BUFFER_SIZE);
    if (g_memmap_line_buffer == 0) {
        kprintln("Warning: memmap line buffer uses stack fallback (heap alloc failed).");
    }

    g_pmm_stats_buffer = (char*)kmalloc(PMM_STATS_BUFFER_SIZE);
    if (g_pmm_stats_buffer == 0) {
        kprintln("Warning: PMM stats buffer uses stack fallback (heap alloc failed).");
    }

    g_heap_stats_buffer = (char*)kmalloc(HEAP_STATS_BUFFER_SIZE);
    if (g_heap_stats_buffer == 0) {
        kprintln("Warning: heap stats buffer uses stack fallback (heap alloc failed).");
    }

    kprint("> ");

    char* input = (char*)kmalloc(SHELL_INPUT_INITIAL);
    if (input == 0) {
        kprintln("FATAL: heap unavailable for shell input buffer.");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    uint32_t input_cap = SHELL_INPUT_INITIAL;
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
            if (input[0] != '\0' && !shell_store_history(input)) {
                kprintln("Warning: failed to store command history.");
            }
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

        if (input_len + 1u >= input_cap) {
            if (input_cap >= SHELL_INPUT_MAX) {
                continue;
            }

            uint32_t next_cap = input_cap * 2u;
            if (next_cap > SHELL_INPUT_MAX) {
                next_cap = SHELL_INPUT_MAX;
            }

            char* grown = (char*)kmalloc(next_cap);
            if (grown == 0) {
                kprintln("Warning: shell buffer growth failed.");
                continue;
            }

            for (uint32_t i = 0; i < input_len; ++i) {
                grown[i] = input[i];
            }

            kfree(input);
            input = grown;
            input_cap = next_cap;
        }

        input[input_len++] = key;

        terminal_write_char(key);
    }
}
