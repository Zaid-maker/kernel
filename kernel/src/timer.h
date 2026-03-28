#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_initialize(uint32_t hz);
void timer_handle_irq(void);
uint32_t timer_ticks(void);
uint32_t timer_hz(void);
uint32_t timer_seconds(void);
uint32_t timer_centiseconds(void);

#endif
