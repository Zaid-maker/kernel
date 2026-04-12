#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#include "../src/syscall.h"

static char g_terminal_out[8192];
static uint32_t g_terminal_len = 0u;
static uint32_t g_fake_uptime = 0u;
static volatile uint32_t g_user_mode_exit_invoked = 0u;
static jmp_buf* g_user_mode_exit_jmp = 0;

enum {
    SYSCALL_TEST_PTR_NULL = 0u,
    SYSCALL_TEST_PTR_SMALL = 1u,
    SYSCALL_TEST_PTR_LARGE = 2u
};

static const char* g_small_text_ptr = 0;
static const char* g_large_text_ptr = 0;

const char* syscall_test_resolve_ptr(uint32_t token) {
    switch (token) {
        case SYSCALL_TEST_PTR_SMALL:
            return g_small_text_ptr;
        case SYSCALL_TEST_PTR_LARGE:
            return g_large_text_ptr;
        case SYSCALL_TEST_PTR_NULL:
        default:
            return 0;
    }
}

void terminal_write_char(char c) {
    if (g_terminal_len < (uint32_t)sizeof(g_terminal_out)) {
        g_terminal_out[g_terminal_len] = c;
    }
    ++g_terminal_len;
}

uint32_t timer_seconds(void) {
    return g_fake_uptime;
}

void user_mode_exit_to_kernel(void) {
    g_user_mode_exit_invoked = 1u;
    if (g_user_mode_exit_jmp != 0) {
        longjmp(*g_user_mode_exit_jmp, 1);
    }

    for (;;) {
    }
}

static void reset_terminal_capture(void) {
    for (uint32_t i = 0u; i < (uint32_t)sizeof(g_terminal_out); ++i) {
        g_terminal_out[i] = '\0';
    }
    g_terminal_len = 0u;
}

static int expect_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual != expected) {
        printf("FAIL: %s (actual=%u expected=%u)\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static int expect_bytes(const char* name, const char* expected, uint32_t len) {
    if (len > g_terminal_len) {
        printf("FAIL: %s (len=%u exceeds captured=%u)\n", name, len, g_terminal_len);
        return 0;
    }

    for (uint32_t i = 0u; i < len; ++i) {
        if (g_terminal_out[i] != expected[i]) {
            printf("FAIL: %s (mismatch at %u)\n", name, i);
            return 0;
        }
    }

    printf("PASS: %s\n", name);
    return 1;
}

int main(void) {
    int ok = 1;

    {
        reset_terminal_capture();

        struct syscall_frame frame = {0};
        frame.eax = SYSCALL_WRITE;
        frame.ebx = SYSCALL_TEST_PTR_NULL;
        frame.ecx = 7u;

        syscall_handle(&frame);

        ok &= expect_u32("write null returns zero", frame.eax, 0u);
        ok &= expect_u32("write null emits zero chars", g_terminal_len, 0u);
    }

    {
        reset_terminal_capture();
        static const char msg[] = "hello";

        struct syscall_frame frame = {0};
        frame.eax = SYSCALL_WRITE;
        g_small_text_ptr = msg;
        frame.ebx = SYSCALL_TEST_PTR_SMALL;
        frame.ecx = (uint32_t)(sizeof(msg) - 1u);

        syscall_handle(&frame);

        ok &= expect_u32("write small returns len", frame.eax, (uint32_t)(sizeof(msg) - 1u));
        ok &= expect_u32("write small emits len", g_terminal_len, (uint32_t)(sizeof(msg) - 1u));
        ok &= expect_bytes("write small output", msg, (uint32_t)(sizeof(msg) - 1u));
    }

    {
        reset_terminal_capture();

        static char large[5000];
        for (uint32_t i = 0u; i < (uint32_t)sizeof(large); ++i) {
            large[i] = (char)('A' + (i % 26u));
        }

        struct syscall_frame frame = {0};
        frame.eax = SYSCALL_WRITE;
        g_large_text_ptr = large;
        frame.ebx = SYSCALL_TEST_PTR_LARGE;
        frame.ecx = (uint32_t)sizeof(large);

        syscall_handle(&frame);

        ok &= expect_u32("write large returns full len", frame.eax, (uint32_t)sizeof(large));
        ok &= expect_u32("write large emits full len", g_terminal_len, (uint32_t)sizeof(large));
        ok &= expect_u32("write large first char", (uint32_t)(uint8_t)g_terminal_out[0], (uint32_t)(uint8_t)'A');
        ok &= expect_u32("write large last char", (uint32_t)(uint8_t)g_terminal_out[sizeof(large) - 1u], (uint32_t)(uint8_t)large[sizeof(large) - 1u]);
    }

    {
        struct syscall_frame frame = {0};
        g_fake_uptime = 1234u;

        frame.eax = SYSCALL_UPTIME_SECONDS;
        syscall_handle(&frame);
        ok &= expect_u32("uptime syscall", frame.eax, 1234u);
    }

    {
        struct syscall_frame frame = {0};
        frame.eax = 99u;
        syscall_handle(&frame);
        ok &= expect_u32("unknown syscall", frame.eax, 0xFFFFFFFFu);
    }

    {
        struct syscall_frame frame = {0};
        jmp_buf exit_env;
        g_user_mode_exit_invoked = 0u;
        g_user_mode_exit_jmp = &exit_env;

        if (setjmp(exit_env) == 0) {
            frame.eax = SYSCALL_EXIT;
            syscall_handle(&frame);
            ok &= expect_u32("syscall exit must not return", 0u, 1u);
        } else {
            ok &= expect_u32("syscall exit invoked", g_user_mode_exit_invoked, 1u);
        }

        g_user_mode_exit_jmp = 0;
    }

    return ok ? 0 : 1;
}
