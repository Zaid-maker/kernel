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
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static void model_append_char(char* buffer, uint32_t cap, uint32_t* len, char c) {
    if (*len + 1u >= cap) {
        return;
    }

    buffer[*len] = c;
    ++(*len);
    buffer[*len] = '\0';
}

static void model_append_str(char* buffer, uint32_t cap, uint32_t* len, const char* text) {
    uint32_t i = 0;
    while (text[i] != '\0') {
        model_append_char(buffer, cap, len, text[i]);
        ++i;
    }
}

int main(void) {
    int ok = 1;

    {
        char line[160];
        uint32_t len = 0;
        sbuf_reset(line);
        sbuf_append_str(line, sizeof(line), &len, " total frames: ");
        sbuf_append_dec_u32(line, sizeof(line), &len, 1024u);
        ok &= assert_eq(line, " total frames: 1024", "pmm line compose");
    }

    {
        char line[160];
        uint32_t len = 0;
        sbuf_reset(line);
        sbuf_append_str(line, sizeof(line), &len, " #");
        sbuf_append_dec_u32(line, sizeof(line), &len, 2u);
        sbuf_append_str(line, sizeof(line), &len, " available base=");
        sbuf_append_hex64(line, sizeof(line), &len, 0x0000000000100000ull);
        sbuf_append_str(line, sizeof(line), &len, " len=");
        sbuf_append_hex64(line, sizeof(line), &len, 0x0000000003F00000ull);
        ok &= assert_eq(line, " #2 available base=0x0000000000100000 len=0x0000000003F00000", "memmap line compose");
    }

    {
        char line[48];
        uint32_t len = 0;
        sbuf_reset(line);
        sbuf_append_str(line, sizeof(line), &len, "Error code: ");
        sbuf_append_hex_u32(line, sizeof(line), &len, 13u);
        ok &= assert_eq(line, "Error code: 0x0000000D", "exception line compose");
    }

    {
        enum { CAP = 64, OPS = 400 };
        const char* words[] = {"A", "bc", "DEF", "_", "-", "42", "xyz"};

        char line[CAP];
        char model[CAP];
        uint32_t len = 0;
        uint32_t model_len = 0;
        uint32_t rng = 0xC0FFEEu;

        sbuf_reset(line);
        model[0] = '\0';

        for (uint32_t i = 0; i < OPS; ++i) {
            const uint32_t r = next_rand(&rng);
            const uint32_t op = r % 3u;

            if (op == 0u) {
                const char c = (char)('a' + ((r >> 8) % 26u));
                sbuf_append_char(line, CAP, &len, c);
                model_append_char(model, CAP, &model_len, c);
                continue;
            }

            if (op == 1u) {
                const char* w = words[(r >> 12) % (sizeof(words) / sizeof(words[0]))];
                sbuf_append_str(line, CAP, &len, w);
                model_append_str(model, CAP, &model_len, w);
                continue;
            }

            const uint32_t value = r ^ (r >> 7);
            char temp[32];
            uint32_t temp_len = 0;
            sbuf_reset(temp);
            sbuf_append_hex_u32(temp, sizeof(temp), &temp_len, value);

            sbuf_append_hex_u32(line, CAP, &len, value);
            model_append_str(model, CAP, &model_len, temp);
        }

        ok &= assert_eq(line, model, "deterministic mixed-append model match");
        ok &= assert_true(len == model_len, "len matches model", "length diverged from reference model");
        ok &= assert_true(len <= (CAP - 1u), "len stays in bounds", "len exceeded capacity-1");
        ok &= assert_true(line[len] == '\0', "line null terminated", "missing null terminator at len");
    }

    return ok ? 0 : 1;
}
