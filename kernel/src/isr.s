.section .text

.extern interrupts_handle_exception
.extern interrupts_handle_timer_irq
.extern interrupts_handle_keyboard_irq
.extern interrupts_handle_mouse_irq

.macro ISR_NOERR num
.global isr_exception_\num
.type isr_exception_\num, @function
isr_exception_\num:
    pusha
    cld
    movl 40(%esp), %eax
    movl 36(%esp), %ebx
    movl 32(%esp), %ecx
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl $0
    pushl $\num
    call interrupts_handle_exception
    addl $20, %esp
    popa
    iret

.size isr_exception_\num, . - isr_exception_\num
.endm

.macro ISR_ERR num
.global isr_exception_\num
.type isr_exception_\num, @function
isr_exception_\num:
    pusha
    cld
    movl 44(%esp), %eax
    movl 40(%esp), %ebx
    movl 36(%esp), %ecx
    movl 32(%esp), %edx
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl $\num
    call interrupts_handle_exception
    addl $20, %esp
    popa
    addl $4, %esp
    iret

.size isr_exception_\num, . - isr_exception_\num
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

.global isr_timer
.type isr_timer, @function
isr_timer:
    pusha
    cld
    call interrupts_handle_timer_irq
    popa
    iret

.size isr_timer, . - isr_timer

.global isr_keyboard
.type isr_keyboard, @function
isr_keyboard:
    pusha
    cld
    call interrupts_handle_keyboard_irq
    popa
    iret

.size isr_keyboard, . - isr_keyboard

.global isr_mouse
.type isr_mouse, @function
isr_mouse:
    pusha
    cld
    call interrupts_handle_mouse_irq
    popa
    iret

.size isr_mouse, . - isr_mouse
