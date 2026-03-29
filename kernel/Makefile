CROSS_COMPILE ?= i686-elf-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
HOST_CC ?= gcc

CFLAGS := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -m32
ASFLAGS := --32
LDFLAGS := -T linker.ld -m elf_i386
HOST_CFLAGS := -std=c11 -O2 -Wall -Wextra
HOST_COVERAGE_CFLAGS := $(HOST_CFLAGS) --coverage

VERSION_FILE := VERSION
KERNEL_VERSION ?= $(shell cat $(VERSION_FILE) 2>/dev/null || echo v0.0.0-dev)
CFLAGS += -DKERNEL_VERSION=\"$(KERNEL_VERSION)\"

BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
ISO_IMAGE := $(BUILD_DIR)/kernel.iso

SRC_ASM := src/boot.s src/isr.s
SRC_C := src/kernel.c src/terminal.c src/print.c src/keyboard.c src/interrupts.c src/timer.c src/pmm.c src/heap.c src/sbuf.c

OBJ_ASM := $(patsubst src/%.s,$(BUILD_DIR)/%.o,$(SRC_ASM))
OBJ_C := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_C))

.PHONY: all clean run iso check-tools test coverage

all: $(KERNEL_BIN)

check-tools:
	@command -v $(CC) >/dev/null 2>&1 || { echo "Missing $(CC)"; exit 1; }
	@command -v $(LD) >/dev/null 2>&1 || { echo "Missing $(LD)"; exit 1; }
	@command -v grub-mkrescue >/dev/null 2>&1 || { echo "Missing grub-mkrescue"; exit 1; }
	@command -v xorriso >/dev/null 2>&1 || { echo "Missing xorriso"; exit 1; }
	@command -v qemu-system-i386 >/dev/null 2>&1 || { echo "Missing qemu-system-i386"; exit 1; }

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.s | $(BUILD_DIR)
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

test: $(BUILD_DIR)
	$(HOST_CC) $(HOST_CFLAGS) tests/sbuf_test.c src/sbuf.c -o $(BUILD_DIR)/sbuf_test
	$(HOST_CC) $(HOST_CFLAGS) tests/sbuf_integration_test.c src/sbuf.c -o $(BUILD_DIR)/sbuf_integration_test
	$(HOST_CC) $(HOST_CFLAGS) tests/sbuf_boundary_test.c src/sbuf.c -o $(BUILD_DIR)/sbuf_boundary_test
	$(BUILD_DIR)/sbuf_test
	$(BUILD_DIR)/sbuf_integration_test
	$(BUILD_DIR)/sbuf_boundary_test

coverage: $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/coverage
	$(HOST_CC) $(HOST_COVERAGE_CFLAGS) -c src/sbuf.c -o $(BUILD_DIR)/sbuf_cov.o
	$(HOST_CC) $(HOST_COVERAGE_CFLAGS) -c tests/sbuf_test.c -o $(BUILD_DIR)/sbuf_test_cov.o
	$(HOST_CC) $(HOST_COVERAGE_CFLAGS) -c tests/sbuf_integration_test.c -o $(BUILD_DIR)/sbuf_integration_test_cov.o
	$(HOST_CC) $(HOST_COVERAGE_CFLAGS) -c tests/sbuf_boundary_test.c -o $(BUILD_DIR)/sbuf_boundary_test_cov.o
	$(HOST_CC) $(HOST_COVERAGE_CFLAGS) $(BUILD_DIR)/sbuf_test_cov.o $(BUILD_DIR)/sbuf_cov.o -o $(BUILD_DIR)/sbuf_test_cov
	$(HOST_CC) $(HOST_COVERAGE_CFLAGS) $(BUILD_DIR)/sbuf_integration_test_cov.o $(BUILD_DIR)/sbuf_cov.o -o $(BUILD_DIR)/sbuf_integration_test_cov
	$(HOST_CC) $(HOST_COVERAGE_CFLAGS) $(BUILD_DIR)/sbuf_boundary_test_cov.o $(BUILD_DIR)/sbuf_cov.o -o $(BUILD_DIR)/sbuf_boundary_test_cov
	lcov --zerocounters --directory $(BUILD_DIR)
	$(BUILD_DIR)/sbuf_test_cov
	$(BUILD_DIR)/sbuf_integration_test_cov
	$(BUILD_DIR)/sbuf_boundary_test_cov
	lcov --capture --directory $(BUILD_DIR) --output-file $(BUILD_DIR)/coverage/lcov.raw.info
	lcov --remove $(BUILD_DIR)/coverage/lcov.raw.info '/usr/*' '*/tests/*' --output-file $(BUILD_DIR)/coverage/lcov.info
	lcov --summary $(BUILD_DIR)/coverage/lcov.info

clean:
	rm -rf $(BUILD_DIR)
