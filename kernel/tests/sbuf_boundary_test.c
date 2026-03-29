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

static int assert_true(int cond, const char* name, const char* detail) {
    if (!cond) {
        printf("FAIL: %s\n  detail: %s\n", name, detail);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static uint32_t next_rand(uint32_t* state) {
    *state = (*state * 1103515245u) + 12345u;
    return *state;
}

int main(void) {
    int ok = 1;

    {
        char buf[12];
        memset(buf, 'Z', sizeof(buf));
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_str(buf, 8u, &len, "1234567");
        sbuf_append_str(buf, 8u, &len, "ABC");
        ok &= assert_eq(buf, "1234567", "no growth when full (append_str)");
        ok &= assert_true(len == 7u, "len stable when full", "len changed after full append_str");
        ok &= assert_true(buf[8] == 'Z', "sentinel preserved append_str", "write escaped cap boundary");
    }

    {
        char buf[16];
        memset(buf, 'Q', sizeof(buf));
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_str(buf, 8u, &len, "ABCD");
        sbuf_append_hex_u32(buf, 8u, &len, 0x12u);
        ok &= assert_eq(buf, "ABCD0x0", "hex append truncation behavior");
        ok &= assert_true(len == 7u, "len capped for hex append", "len exceeded expected cap-1");
        ok &= assert_true(buf[8] == 'Q', "sentinel preserved hex", "write escaped cap boundary");
    }

    {
        char buf[16];
        memset(buf, 'R', sizeof(buf));
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_str(buf, 8u, &len, "ABCD");
        sbuf_append_dec_u32(buf, 8u, &len, 98765u);
        ok &= assert_eq(buf, "ABCD987", "dec append truncation behavior");
        ok &= assert_true(len == 7u, "len capped for dec append", "len exceeded expected cap-1");
        ok &= assert_true(buf[8] == 'R', "sentinel preserved dec", "write escaped cap boundary");
    }

    {
        char buf[6];
        uint32_t len = 0;
        sbuf_reset(buf);
        sbuf_append_char(buf, sizeof(buf), &len, 'A');
        sbuf_append_char(buf, sizeof(buf), &len, 'B');
        sbuf_append_char(buf, sizeof(buf), &len, 'C');
        sbuf_append_char(buf, sizeof(buf), &len, 'D');
        sbuf_append_char(buf, sizeof(buf), &len, 'E');
        sbuf_append_char(buf, sizeof(buf), &len, 'F');
        ok &= assert_eq(buf, "ABCDE", "char append exact boundary");
        ok &= assert_true(len == 5u, "len exact boundary", "len should stop at cap-1");
        ok &= assert_true(buf[len] == '\0', "null at len boundary", "missing null terminator");
    }

    {
        enum { CAP = 9, OPS = 300 };
        const char* words[] = {"A", "BC", "xyz", "_", "12"};

        char buf[CAP + 3];
        uint32_t len = 0;
        uint32_t seed = 0x5EED1234u;

        memset(buf, '#', sizeof(buf));
        sbuf_reset(buf);

        for (uint32_t i = 0; i < OPS; ++i) {
            const uint32_t r = next_rand(&seed);
            const uint32_t op = r % 4u;

            if (op == 0u) {
                sbuf_append_char(buf, CAP, &len, (char)('a' + ((r >> 8) % 26u)));
            } else if (op == 1u) {
                sbuf_append_str(buf, CAP, &len, words[(r >> 12) % (sizeof(words) / sizeof(words[0]))]);
            } else if (op == 2u) {
                sbuf_append_dec_u32(buf, CAP, &len, (r >> 16) % 1000u);
            } else {
                sbuf_append_hex_u32(buf, CAP, &len, r);
            }

            ok &= assert_true(len <= (CAP - 1u), "random len in bounds", "len exceeded cap-1 during random ops");
            ok &= assert_true(buf[len] == '\0', "random null at len", "null terminator missing during random ops");
        }

        ok &= assert_true(buf[CAP] == '#', "random sentinel+0 preserved", "write escaped first sentinel");
        ok &= assert_true(buf[CAP + 1] == '#', "random sentinel+1 preserved", "write escaped second sentinel");
        ok &= assert_true(buf[CAP + 2] == '#', "random sentinel+2 preserved", "write escaped third sentinel");
    }

    return ok ? 0 : 1;
}
