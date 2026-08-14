/*
 * Embedded ring-3 demo blob.
 *
 * user_demo.s is assembled to a raw binary (build/user_demo.bin) and included
 * here so the kernel can copy it into a user page at runtime. This file must
 * be assembled after the binary exists; the Makefile wires that dependency.
 */
.section .rodata
.global user_demo_blob_start
.global user_demo_blob_end
user_demo_blob_start:
.incbin "build/user_demo.bin"
user_demo_blob_end:
