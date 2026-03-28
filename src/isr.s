.section .text

.extern interrupts_handle_keyboard_irq

.global isr_keyboard
.type isr_keyboard, @function
isr_keyboard:
    pusha
    cld
    call interrupts_handle_keyboard_irq
    popa
    iretd

.size isr_keyboard, . - isr_keyboard
