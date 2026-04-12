#include "syscall.h"

#include "terminal.h"
#include "timer.h"
#include "user_mode.h"

enum {
    SYSCALL_WRITE_MAX_CHUNK = 4096u,
    SYSCALL_USER_SPACE_TOP = 0xC0000000u
};

static const char* syscall_decode_text_ptr(uint32_t raw_ptr) {
#ifdef SYSCALL_ENABLE_TEST_HOOKS
    return syscall_test_resolve_ptr(raw_ptr);
#else
    return (const char*)(uintptr_t)raw_ptr;
#endif
}

static int syscall_validate_user_range(const void* text, uint32_t len) {
#ifdef SYSCALL_ENABLE_TEST_HOOKS
    return text != 0;
#else
    const uintptr_t start = (uintptr_t)text;
    const uintptr_t end = start + (uintptr_t)len;

    if (text == 0) {
        return 0;
    }

    if (len == 0u) {
        return 1;
    }

    if (start >= SYSCALL_USER_SPACE_TOP) {
        return 0;
    }

    if (end < start) {
        return 0;
    }

    return end <= SYSCALL_USER_SPACE_TOP;
#endif
}

static int syscall_copy_user_chunk(char* dest, const char* src, uint32_t len) {
    if (dest == 0 || src == 0) {
        return 0;
    }

    for (uint32_t i = 0u; i < len; ++i) {
        dest[i] = src[i];
    }

    dest[len] = '\0';
    return 1;
}

static void syscall_irq_yield(void) {
#ifdef SYSCALL_ENABLE_TEST_HOOKS
    extern void syscall_test_record_irq_yield(void);
    syscall_test_record_irq_yield();
#else
    __asm__ volatile("sti\n\tpause\n\tcli" : : : "memory");
#endif
}

static uint32_t syscall_write_text(const char* text, uint32_t len) {
    if (text == 0 || len == 0u) {
        return 0u;
    }

    if (!syscall_validate_user_range(text, len)) {
        return 0u;
    }

    uint32_t written = 0u;
    char kernel_chunk[SYSCALL_WRITE_MAX_CHUNK + 1u];
    while (written < len) {
        uint32_t chunk_len = len - written;
        if (chunk_len > SYSCALL_WRITE_MAX_CHUNK) {
            chunk_len = SYSCALL_WRITE_MAX_CHUNK;
        }

        if (!syscall_copy_user_chunk(kernel_chunk, text + written, chunk_len)) {
            break;
        }

        for (uint32_t i = 0u; i < chunk_len; ++i) {
            terminal_write_char(kernel_chunk[i]);
        }

        written += chunk_len;

        /* Let hardware IRQs run between large writes to avoid starvation. */
        if (written < len) {
            syscall_irq_yield();
        }
    }

    return written;
}

void syscall_handle(struct syscall_frame* frame) {
    switch (frame->eax) {
        case SYSCALL_WRITE:
            frame->eax = syscall_write_text(syscall_decode_text_ptr(frame->ebx), frame->ecx);
            return;

        case SYSCALL_UPTIME_SECONDS:
            frame->eax = timer_seconds();
            return;

        case SYSCALL_EXIT:
            user_mode_exit_to_kernel();
            __builtin_unreachable();

        default:
            frame->eax = 0xFFFFFFFFu;
            return;
    }
}
