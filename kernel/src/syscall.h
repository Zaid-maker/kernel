#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>
#include <stdint.h>

/*
 * Layout contract: this must match the register save order produced by
 * isr_syscall in src/isr.s, which does "pusha" then passes "pushl %esp"
 * to C. Reordering fields will break syscall register decoding.
 */
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

_Static_assert(sizeof(struct syscall_frame) == 32u, "syscall_frame size must match pusha frame");
_Static_assert(offsetof(struct syscall_frame, edi) == 0u, "syscall_frame edi offset mismatch");
_Static_assert(offsetof(struct syscall_frame, ebx) == 16u, "syscall_frame ebx offset mismatch");
_Static_assert(offsetof(struct syscall_frame, ecx) == 24u, "syscall_frame ecx offset mismatch");
_Static_assert(offsetof(struct syscall_frame, eax) == 28u, "syscall_frame eax offset mismatch");

enum {
    SYSCALL_WRITE = 0u,
    SYSCALL_UPTIME_SECONDS = 1u,
    SYSCALL_EXIT = 2u
};

void syscall_handle(struct syscall_frame* frame);

#endif
