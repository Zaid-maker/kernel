#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/stats_util.h"

static int assert_eq(const char* actual, const char* expected, const char* name) {
    if (strcmp(actual, expected) != 0) {
        printf("FAIL: %s\n  actual:   %s\n  expected: %s\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

int main(void) {
    int ok = 1;

    {
        char line[64];
        stats_format_label_text(line, sizeof(line), "Error code: ", "N/A");
        ok &= assert_eq(line, "Error code: N/A", "label text");
    }

    {
        char line[64];
        stats_format_label_dec(line, sizeof(line), " blocks      : ", 42u);
        ok &= assert_eq(line, " blocks      : 42", "label dec");
    }

    {
        char line[64];
        stats_format_label_hex32(line, sizeof(line), "EIP: ", 0x1Fu);
        ok &= assert_eq(line, "EIP: 0x0000001F", "label hex32");
    }

    {
        char line[80];
        stats_format_label_hex64(line, sizeof(line), " total bytes : ", 0x0000000000002000ull);
        ok &= assert_eq(line, " total bytes : 0x0000000000002000", "label hex64");
    }

    {
        char line[96];
        stats_format_vector_line(line, sizeof(line), 14u, "Page Fault");
        ok &= assert_eq(line, "Vector: 14 - Page Fault", "vector line");
    }

    {
        char line[160];
        stats_format_memmap_entry_line(line, sizeof(line), 2u, "available", 0x0000000000100000ull, 0x0000000003F00000ull);
        ok &= assert_eq(line, " #2 available base=0x0000000000100000 len=0x0000000003F00000", "memmap entry line");
    }

    return ok ? 0 : 1;
}
