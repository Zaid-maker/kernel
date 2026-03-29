#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/sbuf.h"

static int assert_eq(const char* actual, const char* expected, const char* name) {
    if (strcmp(actual, expected) != 0) {
        printf("FAIL: %s\n  actual:   %s\n  expected: %s\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static int assert_len(uint32_t actual, uint32_t expected, const char* name) {
    if (actual != expected) {
        printf("FAIL: %s\n  actual len:   %u\n  expected len: %u\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

int main(void) {
    int ok = 1;

    {
        char buf[64];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_str(buf, sizeof(buf), &len, "abc");
        sbuf_append_char(buf, sizeof(buf), &len, '-');
        sbuf_append_dec_u32(buf, sizeof(buf), &len, 42u);
        ok &= assert_eq(buf, "abc-42", "append str/char/dec");
        ok &= assert_len(len, 6u, "len tracking basic");
    }

    {
        char buf[64];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_hex_u32(buf, sizeof(buf), &len, 0xDEADBEEFu);
        ok &= assert_eq(buf, "0xDEADBEEF", "append hex32");
    }

    {
        char buf[64];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_hex64(buf, sizeof(buf), &len, 0x00000000ABCDEF12ull);
        ok &= assert_eq(buf, "0x00000000ABCDEF12", "append hex64");
    }

    {
        char buf[8];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_str(buf, sizeof(buf), &len, "1234567");
        sbuf_append_char(buf, sizeof(buf), &len, 'X');
        ok &= assert_eq(buf, "1234567", "capacity guard");
        ok &= assert_len(len, 7u, "len unchanged when full");
    }

    {
        char buf[16];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_dec_u32(buf, sizeof(buf), &len, 0u);
        ok &= assert_eq(buf, "0", "decimal zero");
        ok &= assert_len(len, 1u, "len decimal zero");
    }

    {
        char buf[1];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_char(buf, sizeof(buf), &len, 'A');
        sbuf_append_str(buf, sizeof(buf), &len, "B");
        ok &= assert_eq(buf, "", "cap one remains empty");
        ok &= assert_len(len, 0u, "len cap one");
    }

    {
        char buf[10];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_str(buf, sizeof(buf), &len, "ABCDEFGHIJK");
        ok &= assert_eq(buf, "ABCDEFGHI", "string truncation");
        ok &= assert_len(len, 9u, "len truncation");
    }

    {
        char buf[32];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_str(buf, sizeof(buf), &len, "x=");
        sbuf_append_hex_u32(buf, sizeof(buf), &len, 0x12ABu);
        ok &= assert_eq(buf, "x=0x000012AB", "hex append after prefix");
        ok &= assert_len(len, 12u, "len hex with prefix");
    }

    return ok ? 0 : 1;
}
