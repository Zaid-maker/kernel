#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void interrupts_initialize(void);
void interrupts_enable(void);
void interrupts_handle_keyboard_irq(void);
void interrupts_handle_exception(uint32_t vector, uint32_t error_code, uint32_t eip, uint32_t cs, uint32_t eflags);

#endif
