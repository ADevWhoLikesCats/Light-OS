# ============================================================
#  mykernel — Makefile
#  Target: x86_64 long mode, BIOS 2-stage bootloader
#  Boot medium: raw floppy image (1.44 MB)
# ============================================================

# ---- Toolchain ----
CC      := gcc
LD      := ld
AS      := nasm
OBJCOPY := objcopy

# ---- Tools ----
QEMU    := qemu-system-x86_64

# ---- Paths ----
BUILD_DIR  := build
BOOT_DIR   := boot
KERN_DIR   := kernel

STAGE1_BIN := $(BUILD_DIR)/stage1.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
IMAGE      := $(BUILD_DIR)/mykernel.img

# ---- Flags ----
CFLAGS := -m64 -g -O2 -pipe \
          -Wall -Wextra \
          -std=gnu11 \
          -ffreestanding \
          -fno-stack-protector \
          -fno-stack-check \
          -fno-lto \
          -fno-PIC \
          -mno-red-zone \
          -mno-80387 \
          -mno-mmx \
          -mno-sse \
          -mno-sse2 \
          -mcmodel=kernel \
          -nostdlib \
          -I$(KERN_DIR)

LDFLAGS := -m elf_x86_64 -T $(KERN_DIR)/linker.ld -nostdlib \
           -z max-page-size=0x1000

ASFLAGS_BIN := -f bin
ASFLAGS_ELF := -f elf64

# ---- Disk layout constants ----
STAGE2_START_SECTOR := 1
STAGE2_SECTORS      := 16
KERNEL_START_SECTOR := 17
KERNEL_SECTORS      := 64

# ---- Phony targets ----
.PHONY: all clean run boot debug kernel dirs

all: $(IMAGE)

dirs:
	@mkdir -p $(BUILD_DIR)

# ---- Stage 1 (raw 512-byte MBR) ----
$(STAGE1_BIN): $(BOOT_DIR)/stage1.asm | dirs
	$(AS) $(ASFLAGS_BIN) $< -o $@
	@size=$$(stat -c%s $@); \
	if [ "$$size" -ne 512 ]; then \
		echo "ERROR: stage1 is $$size bytes (expected exactly 512)"; exit 1; \
	fi; \
	echo "stage1: $$size bytes"

# ---- Stage 2 (raw binary, loaded at 0x8000 in real mode) ----
$(STAGE2_BIN): $(BOOT_DIR)/stage2.asm | dirs
	$(AS) $(ASFLAGS_BIN) $< -o $@
	@size=$$(stat -c%s $@); \
	max=$$(( $(STAGE2_SECTORS) * 512 )); \
	if [ $$size -gt $$max ]; then \
		echo "ERROR: stage2 is $$size bytes (max $$max)"; exit 1; \
	fi; \
	echo "stage2: $$size bytes (max $$max)"

# ---- Kernel object files ----
$(BUILD_DIR)/main.o: $(KERN_DIR)/main.c $(KERN_DIR)/idt.h | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: $(KERN_DIR)/idt.c $(KERN_DIR)/idt.h | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pic.o: $(KERN_DIR)/pic.c $(KERN_DIR)/pic.h | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pit.o: $(KERN_DIR)/pit.c $(KERN_DIR)/pit.h | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/irq.o: $(KERN_DIR)/irq.asm | dirs
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(BUILD_DIR)/isr.o: $(KERN_DIR)/isr.asm | dirs
	$(AS) $(ASFLAGS_ELF) $< -o $@

# ---- Kernel ELF link ----
$(KERNEL_ELF): $(BUILD_DIR)/main.o $(BUILD_DIR)/idt.o $(BUILD_DIR)/isr.o $(BUILD_DIR)/pic.o $(BUILD_DIR)/pit.o $(BUILD_DIR)/irq.o $(KERN_DIR)/linker.ld
	$(LD) $(LDFLAGS) \
		$(BUILD_DIR)/main.o \
		$(BUILD_DIR)/idt.o \
		$(BUILD_DIR)/isr.o \
		$(BUILD_DIR)/pic.o \
		$(BUILD_DIR)/pit.o \
		$(BUILD_DIR)/irq.o \
		-o $@

# ---- Kernel raw binary (objcopy + pad to KERNEL_SECTORS * 512) ----
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@size=$$(stat -c%s $@); \
	max=$$(( $(KERNEL_SECTORS) * 512 )); \
	if [ $$size -gt $$max ]; then \
		echo "ERROR: kernel is $$size bytes (max $$max)"; exit 1; \
	fi; \
	pad=$$(( $$max - $$size )); \
	if [ $$pad -gt 0 ]; then \
		dd if=/dev/zero bs=1 count=$$pad >> $@ 2>/dev/null; \
	fi; \
	echo "kernel: $$size bytes content, padded to $$max"

kernel: $(KERNEL_BIN)

# ---- Full image (stage1 + stage2 + kernel) ----
$(IMAGE): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	@echo "--- building image ---"
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0                     conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=$(STAGE2_START_SECTOR) conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=$(KERNEL_START_SECTOR) conv=notrunc 2>/dev/null
	@echo "image:  $@ ($$(stat -c%s $@) bytes)"

# ---- Boot stage1 + stage2 only (no kernel) ----
boot: $(STAGE1_BIN) $(STAGE2_BIN)
	dd if=/dev/zero of=$(IMAGE) bs=512 count=2880 2>/dev/null
	dd if=$(STAGE1_BIN) of=$(IMAGE) bs=512 seek=0 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$(IMAGE) bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "boot image: $(IMAGE)"
	stdbuf -o0 $(QEMU) -fda $(IMAGE) -m 256M -serial stdio -no-reboot -no-shutdown

# ---- Boot full image ----
run: $(IMAGE)
	stdbuf -o0 $(QEMU) -fda $(IMAGE) -m 256M -serial stdio -no-reboot -no-shutdown

# ---- Debug (QEMU waits for GDB on :1234) ----
debug: $(IMAGE)
	stdbuf -o0 $(QEMU) -fda $(IMAGE) -m 256M -serial stdio -no-reboot -no-shutdown -s -S

# ---- Clean ----
clean:
	rm -rf $(BUILD_DIR)
