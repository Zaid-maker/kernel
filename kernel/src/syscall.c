#include "syscall.h"

#include "terminal.h"
#include "timer.h"
#include "user_mode.h"

static void syscall_write_text(const char* text, uint32_t len) {
    if (text == 0) {
        return;
    }

    for (uint32_t i = 0; i < len; ++i) {
        terminal_write_char(text[i]);
    }
}

void syscall_handle(struct syscall_frame* frame) {
    switch (frame->eax) {
        case SYSCALL_WRITE:
            syscall_write_text((const char*)(uintptr_t)frame->ebx, frame->ecx);
            frame->eax = frame->ecx;
            return;

        case SYSCALL_UPTIME_SECONDS:
            frame->eax = timer_seconds();
            return;

        case SYSCALL_EXIT:
            user_mode_exit_to_kernel();
            return;

        default:
            frame->eax = 0xFFFFFFFFu;
            return;
    }
}
