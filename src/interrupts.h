#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void interrupts_initialize(void);
void interrupts_enable(void);
void interrupts_handle_keyboard_irq(void);

#endif
