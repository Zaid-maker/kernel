/*
 * Ring-3 demo program (user_demo.s)
 *
 * Assembled to a raw binary (build/user_demo.bin), embedded in the kernel,
 * and copied into a freshly mapped user page when the `usermode` command
 * runs. The program is position-independent: all data is reached PC-relative
 * through call/pop pairs, and all output goes through the int 0x80 ABI.
 *
 * Syscall ABI: eax = number, ebx/ecx = arguments, int $0x80
 *   SYSCALL_WRITE          = 1   (ebx = text, ecx = length)
 *   SYSCALL_UPTIME_SECONDS = 2
 *   SYSCALL_EXIT           = 3
 */
.section .text
.global user_demo_start
user_demo_start:
    /* iret does not reload DS/ES/FS/GS; set the user data segments first. */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* write("[user] hello from ring3 via int 0x80\n") */
    call 9f
    .ascii "[user] hello from ring3 via int 0x80\n"
9:  popl %ebx
    movl $1, %eax            /* SYSCALL_WRITE */
    movl $37, %ecx
    int $0x80

    /* uptime = syscall(SYSCALL_UPTIME_SECONDS) */
    movl $2, %eax
    xorl %ebx, %ebx
    xorl %ecx, %ecx
    int $0x80
    movl %eax, %esi

    /* write("[user] uptime seconds: ") */
    call 8f
    .ascii "[user] uptime seconds: "
8:  popl %ebx
    movl $1, %eax
    movl $23, %ecx
    int $0x80

    /* Convert the uptime to decimal digits on the stack, then write them. */
    movl %esp, %ebp          /* bottom of the digit buffer */
    movl $10, %ecx
    movl %esi, %eax
1:  xorl %edx, %edx
    divl %ecx                /* eax = quotient, edx = digit 0..9 */
    addl $48, %edx           /* '0' */
    pushl %edx
    testl %eax, %eax
    jnz 1b

    movl %esp, %ebx          /* first digit */
    movl %ebp, %ecx
    subl %ebx, %ecx          /* digit count */
    movl $1, %eax
    int $0x80
    movl %ebp, %esp

    /* write("\n") */
    call 7f
    .byte 0x0a
7:  popl %ebx
    movl $1, %eax
    movl $1, %ecx
    int $0x80

    /* exit() — the kernel never returns from this syscall. */
    movl $3, %eax
    int $0x80

    /* Safety net in case exit somehow returns. */
1:  jmp 1b
.global user_demo_end
user_demo_end:
