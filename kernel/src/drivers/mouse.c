#include "mouse.h"

#include <stdint.h>

static uint8_t g_mouse_initialized = 0u;
static uint8_t g_packet_index = 0u;
static uint8_t g_packet[3];
static int32_t g_mouse_x = 0;
static int32_t g_mouse_y = 0;
static uint8_t g_mouse_buttons = 0u;
static uint32_t g_mouse_packets = 0u;

static inline uint8_t port_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void port_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static int mouse_wait_input_clear(void) {
    for (uint32_t i = 0; i < 100000u; ++i) {
        if ((port_inb(0x64) & 0x02u) == 0u) {
            return 1;
        }
    }

    return 0;
}

static int mouse_wait_output_full(void) {
    for (uint32_t i = 0; i < 100000u; ++i) {
        if ((port_inb(0x64) & 0x01u) != 0u) {
            return 1;
        }
    }

    return 0;
}

static int mouse_write_device(uint8_t value) {
    if (!mouse_wait_input_clear()) {
        return 0;
    }
    port_outb(0x64, 0xD4u);

    if (!mouse_wait_input_clear()) {
        return 0;
    }
    port_outb(0x60, value);
    return 1;
}

static int mouse_write_with_ack(uint8_t value) {
    for (uint32_t attempt = 0; attempt < 3u; ++attempt) {
        if (!mouse_write_device(value)) {
            return 0;
        }

        if (!mouse_wait_output_full()) {
            return 0;
        }

        const uint8_t response = port_inb(0x60);
        if (response == 0xFAu) {
            return 1;
        }

        if (response != 0xFEu) {
            return 0;
        }
    }

    return 0;
}

int mouse_initialize(void) {
    g_mouse_initialized = 0u;
    g_packet_index = 0u;
    g_mouse_x = 0;
    g_mouse_y = 0;
    g_mouse_buttons = 0u;
    g_mouse_packets = 0u;

    if (!mouse_wait_input_clear()) {
        return 0;
    }
    port_outb(0x64, 0xA8u);

    if (!mouse_wait_input_clear()) {
        return 0;
    }
    port_outb(0x64, 0x20u);

    if (!mouse_wait_output_full()) {
        return 0;
    }

    uint8_t command_byte = port_inb(0x60);
    command_byte = (uint8_t)(command_byte | 0x02u);
    command_byte = (uint8_t)(command_byte & (uint8_t)~0x20u);

    if (!mouse_wait_input_clear()) {
        return 0;
    }
    port_outb(0x64, 0x60u);

    if (!mouse_wait_input_clear()) {
        return 0;
    }
    port_outb(0x60, command_byte);

    if (!mouse_write_with_ack(0xF6u)) {
        return 0;
    }

    if (!mouse_write_with_ack(0xF4u)) {
        return 0;
    }

    g_mouse_initialized = 1u;
    return 1;
}

void mouse_handle_irq(void) {
    const uint8_t data = port_inb(0x60);

    if (g_mouse_initialized == 0u) {
        return;
    }

    if (g_packet_index == 0u && (data & 0x08u) == 0u) {
        return;
    }

    g_packet[g_packet_index++] = data;
    if (g_packet_index < 3u) {
        return;
    }

    g_packet_index = 0u;

    if ((g_packet[0] & 0xC0u) != 0u) {
        return;
    }

    const int32_t dx = (int32_t)(int8_t)g_packet[1];
    const int32_t dy = (int32_t)(int8_t)g_packet[2];

    g_mouse_x += dx;
    g_mouse_y -= dy;
    g_mouse_buttons = (uint8_t)(g_packet[0] & 0x07u);
    ++g_mouse_packets;
}

void mouse_get_state(struct mouse_state* out_state) {
    if (out_state == 0) {
        return;
    }

    out_state->x = g_mouse_x;
    out_state->y = g_mouse_y;
    out_state->buttons = g_mouse_buttons;
    out_state->packets = g_mouse_packets;
}
