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

#ifdef MOUSE_ENABLE_TEST_HOOKS
void mouse_test_reset_io(void);
void mouse_test_push_status(uint8_t status);
void mouse_test_push_data(uint8_t data);
uint32_t mouse_test_out_count(void);
uint16_t mouse_test_out_port(uint32_t index);
uint8_t mouse_test_out_value(uint32_t index);
#endif

#endif
