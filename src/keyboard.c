#include "keyboard.h"

#include <stdint.h>

static inline uint8_t port_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void port_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static int keyboard_wait_input_clear(void) {
    for (uint32_t i = 0; i < 100000u; ++i) {
        if ((port_inb(0x64) & 0x02u) == 0) {
            return 1;
        }
    }

    return 0;
}

static int keyboard_wait_output_full(void) {
    for (uint32_t i = 0; i < 100000u; ++i) {
        if ((port_inb(0x64) & 0x01u) != 0) {
            return 1;
        }
    }

    return 0;
}

static int keyboard_write_with_ack(uint8_t value) {
    for (uint32_t attempt = 0; attempt < 3u; ++attempt) {
        if (!keyboard_wait_input_clear()) {
            return 0;
        }

        port_outb(0x60, value);

        if (!keyboard_wait_output_full()) {
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

static void keyboard_update_leds(uint8_t caps_lock, uint8_t num_lock, uint8_t scroll_lock) {
    const uint8_t led_mask = (uint8_t)((scroll_lock & 1u) | ((num_lock & 1u) << 1) | ((caps_lock & 1u) << 2));

    if (!keyboard_write_with_ack(0xEDu)) {
        return;
    }

    (void)keyboard_write_with_ack(led_mask);
}

static int is_letter(char c) {
    return c >= 'a' && c <= 'z';
}

static int is_upper_letter(char c) {
    return c >= 'A' && c <= 'Z';
}

static char to_upper(char c) {
    return (char)(c - ('a' - 'A'));
}

static char to_lower(char c) {
    return (char)(c + ('a' - 'A'));
}

static uint8_t shift_pressed = 0;
static uint8_t caps_lock = 0;
static uint8_t num_lock = 0;
static uint8_t scroll_lock = 0;

enum {
    KEYBOARD_QUEUE_SIZE = 128
};

static volatile char keyboard_queue[KEYBOARD_QUEUE_SIZE];
static volatile uint8_t keyboard_queue_head = 0;
static volatile uint8_t keyboard_queue_tail = 0;

static void keyboard_queue_push(char c) {
    const uint8_t next = (uint8_t)((keyboard_queue_head + 1u) % KEYBOARD_QUEUE_SIZE);
    if (next == keyboard_queue_tail) {
        return;
    }

    keyboard_queue[keyboard_queue_head] = c;
    keyboard_queue_head = next;
}

static const char scancode_map[128] = {
    0,
    27,
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b',
    '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n',
    0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,
    '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,
    '*',
    0,
    ' ',
    0,
};

static const char scancode_map_shift[128] = {
    0,
    27,
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b',
    '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n',
    0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,
    '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,
    '*',
    0,
    ' ',
    0,
};

void keyboard_handle_irq(void) {
    const uint8_t scancode = port_inb(0x60);

    if (scancode == 0xE0u) {
        return;
    }

    if (scancode == 0x2Au || scancode == 0x36u) {
        shift_pressed = 1;
        return;
    }

    if (scancode == 0xAAu || scancode == 0xB6u) {
        shift_pressed = 0;
        return;
    }

    if (scancode == 0x3Au) {
        caps_lock ^= 1u;
        keyboard_update_leds(caps_lock, num_lock, scroll_lock);
        return;
    }

    if (scancode == 0x45u) {
        num_lock ^= 1u;
        keyboard_update_leds(caps_lock, num_lock, scroll_lock);
        return;
    }

    if (scancode == 0x46u) {
        scroll_lock ^= 1u;
        keyboard_update_leds(caps_lock, num_lock, scroll_lock);
        return;
    }

    if ((scancode & 0x80u) != 0) {
        return;
    }

    if (scancode >= 128u) {
        return;
    }

    if (scancode >= 0x47u && scancode <= 0x53u) {
        if (scancode == 0x4Au) {
            keyboard_queue_push('-');
            return;
        }

        if (scancode == 0x4Eu) {
            keyboard_queue_push('+');
            return;
        }

        if (num_lock != 0u && shift_pressed == 0u) {
            char keypad = 0;
            switch (scancode) {
                case 0x47u: keypad = '7'; break;
                case 0x48u: keypad = '8'; break;
                case 0x49u: keypad = '9'; break;
                case 0x4Bu: keypad = '4'; break;
                case 0x4Cu: keypad = '5'; break;
                case 0x4Du: keypad = '6'; break;
                case 0x4Fu: keypad = '1'; break;
                case 0x50u: keypad = '2'; break;
                case 0x51u: keypad = '3'; break;
                case 0x52u: keypad = '0'; break;
                case 0x53u: keypad = '.'; break;
                default: keypad = 0; break;
            }

            if (keypad != 0) {
                keyboard_queue_push(keypad);
            }
        }

        return;
    }

    char out = shift_pressed ? scancode_map_shift[scancode] : scancode_map[scancode];
    if (out == 0) {
        return;
    }

    if (caps_lock != 0u) {
        if (is_letter(out)) {
            out = to_upper(out);
            keyboard_queue_push(out);
            return;
        }

        if (is_upper_letter(out)) {
            out = to_lower(out);
            keyboard_queue_push(out);
            return;
        }
    }

    keyboard_queue_push(out);
}

char keyboard_read_char(void) {
    if (keyboard_queue_head == keyboard_queue_tail) {
        return 0;
    }

    const char out = keyboard_queue[keyboard_queue_tail];
    keyboard_queue_tail = (uint8_t)((keyboard_queue_tail + 1u) % KEYBOARD_QUEUE_SIZE);
    return out;
}

uint8_t keyboard_is_caps_lock_on(void) {
    return caps_lock;
}

uint8_t keyboard_is_num_lock_on(void) {
    return num_lock;
}

uint8_t keyboard_is_scroll_lock_on(void) {
    return scroll_lock;
}
