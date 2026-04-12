#ifndef USER_MODE_H
#define USER_MODE_H

#include <stdint.h>

void user_mode_run_demo(void);
int user_mode_is_active(void);
void user_mode_exit_to_kernel(void) __attribute__((noreturn));

extern uint32_t g_user_return_esp;
extern uint32_t g_user_return_eip;

#endif
