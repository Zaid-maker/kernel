#include "syscall.h"

#include "terminal.h"
#include "timer.h"
#include "user_mode.h"

enum {
    SYSCALL_WRITE_MAX_CHUNK = 4096u
};

static const char* syscall_decode_text_ptr(uint32_t raw_ptr) {
#ifdef SYSCALL_ENABLE_TEST_HOOKS
    extern const char* syscall_test_resolve_ptr(uint32_t token);
    return syscall_test_resolve_ptr(raw_ptr);
#else
    return (const char*)(uintptr_t)raw_ptr;
#endif
}

static uint32_t syscall_write_text(const char* text, uint32_t len) {
    if (text == 0) {
        return 0u;
    }

    uint32_t written = 0u;
    while (written < len) {
        uint32_t chunk_len = len - written;
        if (chunk_len > SYSCALL_WRITE_MAX_CHUNK) {
            chunk_len = SYSCALL_WRITE_MAX_CHUNK;
        }

        for (uint32_t i = 0; i < chunk_len; ++i) {
            terminal_write_char(text[written + i]);
        }

        written += chunk_len;

        /* Let hardware IRQs run between large writes to avoid starvation. */
        if (written < len) {
#ifndef SYSCALL_ENABLE_TEST_HOOKS
            __asm__ volatile("sti\n\tpause\n\tcli" : : : "memory");
#endif
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
