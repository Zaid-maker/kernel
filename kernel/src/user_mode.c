#include "user_mode.h"

#include <stdint.h>

#include "print.h"
#include "syscall.h"

uint32_t g_user_return_esp = 0u;
uint32_t g_user_return_eip = 0u;

static uint8_t g_user_mode_active = 0u;
static uint8_t g_user_stack[4096] __attribute__((aligned(16)));

extern void user_mode_enter(uint32_t entry, uint32_t user_stack_top);
extern void user_mode_return_to_kernel_asm(void) __attribute__((noreturn));

static inline uint32_t user_syscall2(uint32_t number, uint32_t arg0, uint32_t arg1) {
    uint32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg0), "c"(arg1)
        : "edx", "memory"
    );
    return result;
}

static inline void user_syscall_exit(void) __attribute__((noreturn));

static inline void user_syscall_exit(void) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_EXIT)
        : "ebx", "ecx", "edx", "memory"
    );

    __builtin_unreachable();
}

__attribute__((noreturn)) static void user_demo_entry(void) {
    __asm__ volatile(
        "movw $0x23, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        :
        :
        : "ax", "memory"
    );

    static const char user_msg[] = "[user] hello from ring3 via int 0x80\n";
    (void)user_syscall2(SYSCALL_WRITE, (uint32_t)(uintptr_t)user_msg, (uint32_t)(sizeof(user_msg) - 1u));

    const uint32_t uptime = user_syscall2(SYSCALL_UPTIME_SECONDS, 0u, 0u);
    static const char prefix[] = "[user] uptime seconds: ";
    (void)user_syscall2(SYSCALL_WRITE, (uint32_t)(uintptr_t)prefix, (uint32_t)(sizeof(prefix) - 1u));

    char digits[10];
    uint32_t n = uptime;
    uint32_t count = 0u;
    do {
        digits[count++] = (char)('0' + (n % 10u));
        n /= 10u;
    } while (n != 0u && count < 10u);

    for (uint32_t i = 0u; i < count / 2u; ++i) {
        const char tmp = digits[i];
        digits[i] = digits[count - 1u - i];
        digits[count - 1u - i] = tmp;
    }

    (void)user_syscall2(SYSCALL_WRITE, (uint32_t)(uintptr_t)digits, count);
    static const char newline[] = "\n";
    (void)user_syscall2(SYSCALL_WRITE, (uint32_t)(uintptr_t)newline, 1u);

    user_syscall_exit();

    for (;;) {
    }
}

int user_mode_is_active(void) {
    return g_user_mode_active != 0u;
}

void user_mode_exit_to_kernel(void) {
    g_user_mode_active = 0u;
    user_mode_return_to_kernel_asm();
}

void user_mode_run_demo(void) {
    if (g_user_mode_active != 0u) {
        kprintln("User mode demo is already running.");
        return;
    }

    g_user_mode_active = 1u;
    kprintln("Switching to user mode...");
    user_mode_enter((uint32_t)(uintptr_t)user_demo_entry, (uint32_t)(uintptr_t)&g_user_stack[sizeof(g_user_stack)]);
    kprintln("Returned from user mode.");
}
