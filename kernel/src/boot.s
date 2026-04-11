/* Multiboot header and kernel entry glue for 32-bit x86 */

.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
_start:
    cli
    mov $stack_top, %esp
    push %ebx
    push %eax
    call kernel_main
    add $8, %esp
.hang:
    hlt
    jmp .hang
