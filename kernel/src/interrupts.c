#include "interrupts.h"

#include <stdint.h>

#include "keyboard.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];

extern void isr_keyboard(void);

static inline uint8_t port_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void port_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void) {
    port_outb(0x80, 0);
}

static void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFFu);
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].flags = flags;
    idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xFFFFu);
}

static void idt_load(void) {
    const struct idt_ptr idtr = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uint32_t)&idt[0],
    };

    __asm__ volatile("lidt %0" : : "m"(idtr));
}

static void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    const uint8_t master_mask = port_inb(0x21);
    const uint8_t slave_mask = port_inb(0xA1);

    port_outb(0x20, 0x11);
    io_wait();
    port_outb(0xA0, 0x11);
    io_wait();

    port_outb(0x21, master_offset);
    io_wait();
    port_outb(0xA1, slave_offset);
    io_wait();

    port_outb(0x21, 0x04);
    io_wait();
    port_outb(0xA1, 0x02);
    io_wait();

    port_outb(0x21, 0x01);
    io_wait();
    port_outb(0xA1, 0x01);
    io_wait();

    port_outb(0x21, master_mask);
    port_outb(0xA1, slave_mask);
}

static void pic_set_irq_mask(uint8_t master_mask, uint8_t slave_mask) {
    port_outb(0x21, master_mask);
    port_outb(0xA1, slave_mask);
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8u) {
        port_outb(0xA0, 0x20);
    }

    port_outb(0x20, 0x20);
}

void interrupts_initialize(void) {
    for (uint32_t i = 0; i < 256u; ++i) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
    }

    idt_set_gate(0x21u, (uint32_t)isr_keyboard, 0x08u, 0x8Eu);

    pic_remap(0x20u, 0x28u);
    pic_set_irq_mask(0xFDu, 0xFFu);

    idt_load();
}

void interrupts_enable(void) {
    __asm__ volatile("sti");
}

void interrupts_handle_keyboard_irq(void) {
    keyboard_handle_irq();
    pic_send_eoi(1);
}
