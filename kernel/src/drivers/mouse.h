#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

struct mouse_state {
    int32_t x;
    int32_t y;
    uint8_t buttons;
    uint32_t packets;
};

int mouse_initialize(void);
void mouse_handle_irq(void);
void mouse_get_state(struct mouse_state* out_state);

#endif
