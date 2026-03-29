#include "interrupts.h"

#include <stdint.h>

#include "heap.h"
#include "keyboard.h"
#include "print.h"
#include "timer.h"
#include "terminal.h"

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

extern void isr_exception_0(void);
extern void isr_exception_1(void);
extern void isr_exception_2(void);
extern void isr_exception_3(void);
extern void isr_exception_4(void);
extern void isr_exception_5(void);
extern void isr_exception_6(void);
extern void isr_exception_7(void);
extern void isr_exception_8(void);
extern void isr_exception_9(void);
extern void isr_exception_10(void);
extern void isr_exception_11(void);
extern void isr_exception_12(void);
extern void isr_exception_13(void);
extern void isr_exception_14(void);
extern void isr_exception_15(void);
extern void isr_exception_16(void);
extern void isr_exception_17(void);
extern void isr_exception_18(void);
extern void isr_exception_19(void);
extern void isr_exception_20(void);
extern void isr_exception_21(void);
extern void isr_exception_22(void);
extern void isr_exception_23(void);
extern void isr_exception_24(void);
extern void isr_exception_25(void);
extern void isr_exception_26(void);
extern void isr_exception_27(void);
extern void isr_exception_28(void);
extern void isr_exception_29(void);
extern void isr_exception_30(void);
extern void isr_exception_31(void);
extern void isr_timer(void);
extern void isr_keyboard(void);

static const uint32_t exception_handlers[32] = {
    (uint32_t)isr_exception_0,
    (uint32_t)isr_exception_1,
    (uint32_t)isr_exception_2,
    (uint32_t)isr_exception_3,
    (uint32_t)isr_exception_4,
    (uint32_t)isr_exception_5,
    (uint32_t)isr_exception_6,
    (uint32_t)isr_exception_7,
    (uint32_t)isr_exception_8,
    (uint32_t)isr_exception_9,
    (uint32_t)isr_exception_10,
    (uint32_t)isr_exception_11,
    (uint32_t)isr_exception_12,
    (uint32_t)isr_exception_13,
    (uint32_t)isr_exception_14,
    (uint32_t)isr_exception_15,
    (uint32_t)isr_exception_16,
    (uint32_t)isr_exception_17,
    (uint32_t)isr_exception_18,
    (uint32_t)isr_exception_19,
    (uint32_t)isr_exception_20,
    (uint32_t)isr_exception_21,
    (uint32_t)isr_exception_22,
    (uint32_t)isr_exception_23,
    (uint32_t)isr_exception_24,
    (uint32_t)isr_exception_25,
    (uint32_t)isr_exception_26,
    (uint32_t)isr_exception_27,
    (uint32_t)isr_exception_28,
    (uint32_t)isr_exception_29,
    (uint32_t)isr_exception_30,
    (uint32_t)isr_exception_31,
};

static const char* const exception_names[32] = {
    "Divide-by-zero Error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

enum {
    EXCEPTION_DIAG_BUFFER_SIZE = 160
};

static char g_exception_diag_fallback[EXCEPTION_DIAG_BUFFER_SIZE];
static char* g_exception_diag_buffer = g_exception_diag_fallback;

static void diag_reset(char* buffer) {
    buffer[0] = '\0';
}

static void diag_append_char(char* buffer, uint32_t cap, uint32_t* len, char c) {
    if (*len + 1u >= cap) {
        return;
    }

    buffer[*len] = c;
    ++(*len);
    buffer[*len] = '\0';
}

static void diag_append_str(char* buffer, uint32_t cap, uint32_t* len, const char* text) {
    uint32_t i = 0;
    while (text[i] != '\0') {
        if (*len + 1u >= cap) {
            return;
        }

        buffer[*len] = text[i];
        ++(*len);
        ++i;
    }

    buffer[*len] = '\0';
}

static void diag_append_dec_u32(char* buffer, uint32_t cap, uint32_t* len, uint32_t value) {
    if (value == 0u) {
        diag_append_char(buffer, cap, len, '0');
        return;
    }

    char digits[10];
    uint32_t pos = 0;
    while (value != 0u && pos < 10u) {
        digits[pos++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (pos > 0u) {
        diag_append_char(buffer, cap, len, digits[--pos]);
    }
}

static void diag_append_hex_u32(char* buffer, uint32_t cap, uint32_t* len, uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";

    diag_append_str(buffer, cap, len, "0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        const uint32_t nibble = (value >> (uint32_t)shift) & 0xFu;
        diag_append_char(buffer, cap, len, hex[nibble]);
    }
}

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

static int exception_has_error_code(uint32_t vector) {
    switch (vector) {
        case 8u:
        case 10u:
        case 11u:
        case 12u:
        case 13u:
        case 14u:
        case 17u:
        case 21u:
        case 29u:
        case 30u:
            return 1;
        default:
            return 0;
    }
}

void interrupts_initialize(void) {
    char* heap_diag_buffer = (char*)kmalloc(EXCEPTION_DIAG_BUFFER_SIZE);
    if (heap_diag_buffer != 0) {
        g_exception_diag_buffer = heap_diag_buffer;
    } else {
        g_exception_diag_buffer = g_exception_diag_fallback;
    }

    for (uint32_t i = 0; i < 256u; ++i) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
    }

    for (uint32_t i = 0; i < 32u; ++i) {
        idt_set_gate((uint8_t)i, exception_handlers[i], 0x08u, 0x8Eu);
    }

    idt_set_gate(0x20u, (uint32_t)isr_timer, 0x08u, 0x8Eu);
    idt_set_gate(0x21u, (uint32_t)isr_keyboard, 0x08u, 0x8Eu);

    pic_remap(0x20u, 0x28u);
    pic_set_irq_mask(0xFCu, 0xFFu);

    idt_load();
}

void interrupts_enable(void) {
    __asm__ volatile("sti");
}

void interrupts_handle_timer_irq(void) {
    timer_handle_irq();
    pic_send_eoi(0);
}

void interrupts_handle_keyboard_irq(void) {
    keyboard_handle_irq();
    pic_send_eoi(1);
}

void interrupts_handle_exception(uint32_t vector, uint32_t error_code, uint32_t eip, uint32_t cs, uint32_t eflags) {
    __asm__ volatile("cli");

    terminal_initialize();
    terminal_fill_row(0, ' ', VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal_write_at(" KERNEL EXCEPTION ", 0, 1, VGA_COLOR_WHITE, VGA_COLOR_RED);

    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintln("A CPU exception occurred. System halted.");
    terminal_write_char('\n');

    char diag_fallback[EXCEPTION_DIAG_BUFFER_SIZE];
    char* line = g_exception_diag_buffer != 0 ? g_exception_diag_buffer : diag_fallback;
    uint32_t len = 0;

    diag_reset(line);
    diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, "Vector: ");
    diag_append_dec_u32(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, vector);
    diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, " - ");
    if (vector < 32u) {
        diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, exception_names[vector]);
    } else {
        diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, "Unknown Exception");
    }
    kprintln(line);

    len = 0;
    diag_reset(line);
    diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, "Error code: ");
    if (exception_has_error_code(vector)) {
        diag_append_hex_u32(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, error_code);
    } else {
        diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, "N/A");
    }
    kprintln(line);

    len = 0;
    diag_reset(line);
    diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, "EIP: ");
    diag_append_hex_u32(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, eip);
    kprintln(line);

    len = 0;
    diag_reset(line);
    diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, "CS: ");
    diag_append_hex_u32(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, cs);
    kprintln(line);

    len = 0;
    diag_reset(line);
    diag_append_str(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, "EFLAGS: ");
    diag_append_hex_u32(line, EXCEPTION_DIAG_BUFFER_SIZE, &len, eflags);
    kprintln(line);

    terminal_write_char('\n');
    kprintln("Reset or power cycle to recover.");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
