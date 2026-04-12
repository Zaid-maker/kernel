#include "gdt.h"

#include <stddef.h>
#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry g_gdt[6];
static struct gdt_ptr g_gdt_ptr;
static struct tss_entry g_tss;

static uint8_t g_tss_kernel_stack[4096] __attribute__((aligned(16)));

extern void gdt_flush(const struct gdt_ptr* ptr);
extern void tss_flush(uint16_t tss_selector);

static void gdt_set_gate(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    g_gdt[index].base_low = (uint16_t)(base & 0xFFFFu);
    g_gdt[index].base_middle = (uint8_t)((base >> 16) & 0xFFu);
    g_gdt[index].base_high = (uint8_t)((base >> 24) & 0xFFu);

    g_gdt[index].limit_low = (uint16_t)(limit & 0xFFFFu);
    g_gdt[index].granularity = (uint8_t)((limit >> 16) & 0x0Fu);
    g_gdt[index].granularity |= (uint8_t)(granularity & 0xF0u);

    g_gdt[index].access = access;
}

void gdt_set_kernel_stack(uint32_t esp0) {
    g_tss.esp0 = esp0;
}

static void gdt_write_tss(uint32_t index, uint16_t kernel_ss, uint32_t kernel_esp) {
    const uint32_t base = (uint32_t)(uintptr_t)&g_tss;
    const uint32_t limit = (uint32_t)sizeof(g_tss) - 1u;

    gdt_set_gate(index, base, limit, 0x89u, 0x00u);

    for (uint32_t i = 0; i < (uint32_t)sizeof(g_tss); ++i) {
        ((uint8_t*)&g_tss)[i] = 0u;
    }

    g_tss.ss0 = kernel_ss;
    g_tss.esp0 = kernel_esp;
    g_tss.cs = 0x0Bu;
    g_tss.ss = 0x13u;
    g_tss.ds = 0x13u;
    g_tss.es = 0x13u;
    g_tss.fs = 0x13u;
    g_tss.gs = 0x13u;
    g_tss.iomap_base = (uint16_t)sizeof(g_tss);
}

void gdt_initialize(void) {
    g_gdt_ptr.limit = (uint16_t)(sizeof(g_gdt) - 1u);
    g_gdt_ptr.base = (uint32_t)(uintptr_t)&g_gdt[0];

    gdt_set_gate(0u, 0u, 0u, 0u, 0u);
    gdt_set_gate(1u, 0u, 0xFFFFFu, 0x9Au, 0xCFu);
    gdt_set_gate(2u, 0u, 0xFFFFFu, 0x92u, 0xCFu);
    gdt_set_gate(3u, 0u, 0xFFFFFu, 0xFAu, 0xCFu);
    gdt_set_gate(4u, 0u, 0xFFFFFu, 0xF2u, 0xCFu);

    gdt_write_tss(5u, 0x10u, (uint32_t)(uintptr_t)&g_tss_kernel_stack[sizeof(g_tss_kernel_stack)]);

    gdt_flush(&g_gdt_ptr);
    tss_flush(0x28u);
}
