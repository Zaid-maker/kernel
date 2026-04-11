#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

struct syscall_frame {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
};

enum {
    SYSCALL_WRITE = 0u,
    SYSCALL_UPTIME_SECONDS = 1u,
    SYSCALL_EXIT = 2u
};

void syscall_handle(struct syscall_frame* frame);

#endif
