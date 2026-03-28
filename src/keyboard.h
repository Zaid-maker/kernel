#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

char keyboard_read_char(void);
void keyboard_handle_irq(void);
uint8_t keyboard_is_caps_lock_on(void);
uint8_t keyboard_is_num_lock_on(void);
uint8_t keyboard_is_scroll_lock_on(void);

#endif
