.section .text

.extern interrupts_handle_exception
.extern interrupts_handle_timer_irq
.extern interrupts_handle_keyboard_irq
.extern interrupts_handle_mouse_irq
.extern interrupts_handle_syscall
.extern g_user_return_esp
.extern g_user_return_eip

.macro ISR_NOERR num
.global isr_exception_\num
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

.endm

.macro ISR_ERR num
.global isr_exception_\num
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
isr_timer:
    pusha
    cld
    call interrupts_handle_timer_irq
    popa
    iret

.global isr_keyboard
isr_keyboard:
    pusha
    cld
    call interrupts_handle_keyboard_irq
    popa
    iret

.global isr_mouse
isr_mouse:
    pusha
    cld
    call interrupts_handle_mouse_irq
    popa
    iret

.global isr_syscall
isr_syscall:
    pusha
    cld
    pushl %esp
    call interrupts_handle_syscall
    addl $4, %esp
    popa
    iret

.global gdt_flush
gdt_flush:
    movl 4(%esp), %eax
    lgdt (%eax)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ljmp $0x08, $gdt_flush_done
gdt_flush_done:
    ret

.global tss_flush
tss_flush:
    movw 4(%esp), %ax
    ltr %ax
    ret

.global user_mode_enter
user_mode_enter:
    movl 4(%esp), %eax
    movl 8(%esp), %edx

    movl %esp, g_user_return_esp
    movl $user_mode_return_label, g_user_return_eip

    pushl $0x23
    pushl %edx
    pushfl
    popl %ecx
    orl $0x200, %ecx
    pushl %ecx
    pushl $0x1B
    pushl %eax
    iret

user_mode_return_label:
    ret

.global user_mode_return_to_kernel_asm
user_mode_return_to_kernel_asm:
    movl g_user_return_esp, %esp
    jmp *g_user_return_eip

