#include "timer.h"

#include <stdint.h>

static volatile uint32_t g_timer_ticks = 0;
static volatile uint32_t g_timer_hz = 100;

static inline void port_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void timer_initialize(uint32_t hz) {
    uint32_t target_hz = hz;
    if (target_hz == 0u) {
        target_hz = 100u;
    }

    if (target_hz > 1193182u) {
        target_hz = 1193182u;
    }

    uint32_t divisor = 1193182u / target_hz;
    if (divisor == 0u) {
        divisor = 1u;
    }

    g_timer_hz = 1193182u / divisor;

    port_outb(0x43, 0x36);
    port_outb(0x40, (uint8_t)(divisor & 0xFFu));
    port_outb(0x40, (uint8_t)((divisor >> 8) & 0xFFu));
}

void timer_handle_irq(void) {
    ++g_timer_ticks;
}

uint32_t timer_ticks(void) {
    return g_timer_ticks;
}

uint32_t timer_hz(void) {
    return g_timer_hz;
}

uint32_t timer_seconds(void) {
    const uint32_t hz = g_timer_hz;
    return hz == 0u ? 0u : (g_timer_ticks / hz);
}

uint32_t timer_centiseconds(void) {
    const uint32_t hz = g_timer_hz;
    if (hz == 0u) {
        return 0u;
    }

    return (g_timer_ticks * 100u / hz) % 100u;
}
