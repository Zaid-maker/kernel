#include "user_mode.h"

#include <stdint.h>

#include "paging.h"
#include "pmm.h"
#include "print.h"

/* Raw ring-3 program produced from user_demo.s (see user_demo_blob.S). */
extern const uint8_t user_demo_blob_start[];
extern const uint8_t user_demo_blob_end[];

uint32_t g_user_return_esp = 0u;
uint32_t g_user_return_eip = 0u;
uint32_t g_user_transition_stack_top = 0u;

static uint8_t g_user_mode_active = 0u;

/* Stack used by isr.s to build the ring-3 iret frame. It lives in .bss
   (higher half), so it stays mapped while either page directory is active. */
static uint8_t g_user_transition_stack[4096] __attribute__((aligned(16)));

enum {
    USER_CODE_VA = 0x00100000u,
    USER_STACK_VA = 0x00200000u,
    USER_STACK_SIZE = 4096u
};

extern void user_mode_enter(uint32_t entry, uint32_t user_stack_top);
extern void user_mode_return_to_kernel_asm(void) __attribute__((noreturn));

static void copy_bytes(uint8_t* dst, const uint8_t* src, uint32_t len) {
    for (uint32_t i = 0u; i < len; ++i) {
        dst[i] = src[i];
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

    const uint32_t blob_len = (uint32_t)(user_demo_blob_end - user_demo_blob_start);
    if (blob_len == 0u || blob_len > USER_STACK_SIZE) {
        kprintln("User demo blob has an invalid size.");
        return;
    }

    const uint32_t code_frame = pmm_alloc_frame();
    const uint32_t stack_frame = pmm_alloc_frame();
    if (code_frame == 0u || stack_frame == 0u) {
        if (code_frame != 0u) {
            pmm_free_frame(code_frame);
        }
        if (stack_frame != 0u) {
            pmm_free_frame(stack_frame);
        }
        kprintln("Failed to allocate user mode memory.");
        return;
    }

    /* Copy the embedded ring-3 program into its code page (the kernel's
       identity map makes the frame address directly dereferenceable). */
    copy_bytes((uint8_t*)(uintptr_t)code_frame, user_demo_blob_start, blob_len);

    if (!paging_prepare_user_space(USER_CODE_VA, code_frame, USER_STACK_VA, stack_frame)) {
        pmm_free_frame(code_frame);
        pmm_free_frame(stack_frame);
        kprintln("Failed to set up user page tables.");
        return;
    }

    g_user_transition_stack_top =
        (uint32_t)(uintptr_t)&g_user_transition_stack[sizeof(g_user_transition_stack)];

    g_user_mode_active = 1u;
    kprintln("Switching to user mode...");
    user_mode_enter(USER_CODE_VA, USER_STACK_VA + USER_STACK_SIZE);
    kprintln("Returned from user mode.");

    /* Back on the kernel directory; hand the demo's frames and page tables
       back to the PMM so repeated `usermode` runs do not leak memory. */
    paging_cleanup_user_space();
    pmm_free_frame(code_frame);
    pmm_free_frame(stack_frame);
}
