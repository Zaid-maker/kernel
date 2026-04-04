#include <stdint.h>
#include <stdio.h>

#define MOUSE_ENABLE_TEST_HOOKS
#include "../src/drivers/mouse.h"

static int expect_true(const char* name, int value) {
    if (!value) {
        printf("FAIL: %s\n", name);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static int expect_u32(const char* name, uint32_t actual, uint32_t expected) {
    if (actual != expected) {
        printf("FAIL: %s (actual=%u expected=%u)\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static int expect_i32(const char* name, int32_t actual, int32_t expected) {
    if (actual != expected) {
        printf("FAIL: %s (actual=%d expected=%d)\n", name, actual, expected);
        return 0;
    }

    printf("PASS: %s\n", name);
    return 1;
}

static void push_init_success_sequence(void) {
    /* enable aux: wait input clear */
    mouse_test_push_status(0x00u);

    /* read command byte: wait input clear, send 0x20, then output full + data */
    mouse_test_push_status(0x00u);
    mouse_test_push_status(0x01u);
    mouse_test_push_data(0x00u);

    /* write command byte: two input-clear waits */
    mouse_test_push_status(0x00u);
    mouse_test_push_status(0x00u);

    /* set defaults (0xF6): input-clear, write 0xD4, input-clear, write 0xF6, output-full + ACK */
    mouse_test_push_status(0x00u);
    mouse_test_push_status(0x00u);
    mouse_test_push_status(0x01u);
    mouse_test_push_data(0xFAu);

    /* enable streaming (0xF4): input-clear, write 0xD4, input-clear, write 0xF4, output-full + ACK */
    mouse_test_push_status(0x00u);
    mouse_test_push_status(0x00u);
    mouse_test_push_status(0x01u);
    mouse_test_push_data(0xFAu);
}

int main(void) {
    int ok = 1;

    {
        mouse_test_reset_io();
        struct mouse_state state;
        mouse_get_state(&state);
        ok &= expect_i32("initial x", state.x, 0);
        ok &= expect_i32("initial y", state.y, 0);
        ok &= expect_u32("initial buttons", state.buttons, 0u);
        ok &= expect_u32("initial packets", state.packets, 0u);

        /* Null out_state guard path */
        mouse_get_state(0);
        ok &= expect_true("null state guard", 1);
    }

    {
        mouse_test_reset_io();

        /* IRQ before init should be ignored. */
        mouse_test_push_data(0x08u);
        mouse_handle_irq();

        struct mouse_state state;
        mouse_get_state(&state);
        ok &= expect_u32("pre-init packets unchanged", state.packets, 0u);
    }

    {
        mouse_test_reset_io();
        push_init_success_sequence();

        ok &= expect_true("initialize success", mouse_initialize());
        ok &= expect_true("out writes emitted", mouse_test_out_count() >= 6u);
        ok &= expect_u32("first out port", mouse_test_out_port(0), 0x64u);
        ok &= expect_u32("first out value", mouse_test_out_value(0), 0xA8u);
    }

    {
        struct mouse_state state;

        /* Packet resync: first byte must contain always-set bit 3. */
        mouse_test_push_data(0x00u);
        mouse_handle_irq();

        /* Valid packet: left button, dx=+5, dy=-3 => y += 3 because driver inverts Y axis. */
        mouse_test_push_data(0x09u);
        mouse_handle_irq();
        mouse_test_push_data(0x05u);
        mouse_handle_irq();
        mouse_test_push_data(0xFDu);
        mouse_handle_irq();

        mouse_get_state(&state);
        ok &= expect_i32("x after packet", state.x, 5);
        ok &= expect_i32("y after packet", state.y, 3);
        ok &= expect_u32("buttons after packet", state.buttons, 1u);
        ok &= expect_u32("packets after packet", state.packets, 1u);

        /* Overflow bits set in first packet byte should discard completed packet. */
        mouse_test_push_data(0xC8u);
        mouse_handle_irq();
        mouse_test_push_data(0x7Fu);
        mouse_handle_irq();
        mouse_test_push_data(0x7Fu);
        mouse_handle_irq();

        mouse_get_state(&state);
        ok &= expect_u32("overflow packet ignored", state.packets, 1u);
    }

    {
        mouse_test_reset_io();

        /* Initialize failure via ACK error path (not ACK and not RESEND). */
        mouse_test_push_status(0x00u);
        mouse_test_push_status(0x00u);
        mouse_test_push_status(0x01u);
        mouse_test_push_data(0x00u);
        mouse_test_push_status(0x00u);
        mouse_test_push_status(0x00u);
        mouse_test_push_status(0x00u);
        mouse_test_push_status(0x00u);
        mouse_test_push_status(0x01u);
        mouse_test_push_data(0xFCu);

        ok &= expect_true("initialize failure bad ack", !mouse_initialize());
    }

    return ok ? 0 : 1;
}
