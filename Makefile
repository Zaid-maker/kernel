CROSS_COMPILE ?= i686-elf-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld

CFLAGS := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -m32
ASFLAGS := --32
LDFLAGS := -T linker.ld -m elf_i386

BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
ISO_IMAGE := $(BUILD_DIR)/kernel.iso

SRC_ASM := src/boot.s
SRC_C := src/kernel.c src/terminal.c src/print.c src/keyboard.c

OBJ_ASM := $(BUILD_DIR)/boot.o
OBJ_C := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_C))

.PHONY: all clean run iso check-tools

all: $(KERNEL_BIN)

check-tools:
	@command -v $(CC) >/dev/null 2>&1 || { echo "Missing $(CC)"; exit 1; }
	@command -v $(LD) >/dev/null 2>&1 || { echo "Missing $(LD)"; exit 1; }
	@command -v grub-mkrescue >/dev/null 2>&1 || { echo "Missing grub-mkrescue"; exit 1; }
	@command -v xorriso >/dev/null 2>&1 || { echo "Missing xorriso"; exit 1; }
	@command -v qemu-system-i386 >/dev/null 2>&1 || { echo "Missing qemu-system-i386"; exit 1; }

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJ_ASM): $(SRC_ASM) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(OBJ_ASM) $(OBJ_C) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ_ASM) $(OBJ_C)

$(KERNEL_BIN): $(KERNEL_ELF)
	cp $(KERNEL_ELF) $@

iso: $(KERNEL_BIN)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)

run: iso
	qemu-system-i386 -cdrom $(ISO_IMAGE)

clean:
	rm -rf $(BUILD_DIR)
