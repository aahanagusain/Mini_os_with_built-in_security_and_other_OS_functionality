CC=gcc
AS=as
GCCPARAMS = -m32 -nostdlib -fno-builtin -fno-exceptions -ffreestanding -fno-leading-underscore -Wall -Wextra -Wpedantic
ASPARAMS = --32
LDPARAMS = -melf_i386

SRC_DIR=src
HDR_DIR=include/
OBJ_DIR=obj
ISO_DIR=iso

# generated sources directory
GEN_DIR=$(SRC_DIR)/generated
GEN_OBJ_DIR=$(OBJ_DIR)/generated

# automatic time_build.asm generation (build-time)
TIME_SRC=$(GEN_DIR)/time_build.asm
TIME_OBJ=$(GEN_OBJ_DIR)/time_build.o


# initrd generation (phase 1)
INITRD_DIR=fsroot
INITRD_SRC=$(SRC_DIR)/initrd_data.c
MKINIT=tools/mkinitrd.py

SRC_FILES1=$(shell find $(SRC_DIR) -name '*.c')
OBJ_FILES1=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_FILES1))
SRC_FILES2=$(shell find $(SRC_DIR) -name '*.s')
OBJ_FILES2=$(patsubst $(SRC_DIR)/%.s, $(OBJ_DIR)/%.o, $(SRC_FILES2))
SRC_FILES3=$(shell find $(SRC_DIR) -name '*.asm')
OBJ_FILES3=$(patsubst $(SRC_DIR)/%.asm, $(OBJ_DIR)/%.o, $(SRC_FILES3))

# Combine and deduplicate all object files to avoid passing duplicates to the linker
OBJ_ALL=$(sort $(OBJ_FILES1) $(OBJ_FILES2) $(OBJ_FILES3) $(TIME_OBJ))


# Generate time_build.asm at build time so it contains the current build time.
$(TIME_SRC):
	@echo "Generating $(TIME_SRC) with current time"
	@mkdir -p $(dir $@)
	@echo 'global build_time_str' > $@
	@echo '' >> $@
	@echo 'section .text' >> $@
	@echo 'build_time_str:' >> $@
	@echo -n '    db "' >> $@
	@echo -n "$(shell date '+%Y-%m-%d %H:%M:%S')" >> $@
	@echo -n ' | commit: ' >> $@
	@echo -n "$(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)" >> $@
	@echo -n ' (' >> $@
	@echo -n "$(shell git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)" >> $@
	@echo '", 0x0' >> $@
	@echo '    ret' >> $@



# If an fsroot directory exists, generate $(INITRD_SRC) from it.
ifeq ($(wildcard $(INITRD_DIR)),)
# no fsroot dir: nothing to generate
else
$(INITRD_SRC): $(shell find $(INITRD_DIR) -type f)
	@echo "Generating $(INITRD_SRC) from $(INITRD_DIR)"
	python3 $(MKINIT) $(INITRD_DIR) > $(INITRD_SRC)
endif

check_dir:
	if [ ! -d "$(OBJ_DIR)" ]; then \
		mkdir -p $(OBJ_DIR); \
	fi

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(GCCPARAMS) $^ -I$(HDR_DIR) -c -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	mkdir -p $(dir $@)
	$(AS) $(ASPARAMS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	mkdir -p $(dir $@)
	nasm -f elf32 -o $@ $<

# Rule for compiling generated asm -> obj in the generated obj dir
$(GEN_OBJ_DIR)/%.o: $(GEN_DIR)/%.asm
	@mkdir -p $(dir $@)
	nasm -f elf32 -o $@ $<

pegasus.bin: $(SRC_DIR)/linker.ld $(TIME_SRC) $(OBJ_ALL)
	@mkdir -p $(GEN_OBJ_DIR)
	ld $(LDPARAMS) -T $< -o $@ $(OBJ_ALL)

pegasus.iso: pegasus.bin
	@if [ -x ./update_version ]; then ./update_version; else echo "(skipping update_version)"; fi
	mkdir -p iso/boot/grub
	cp pegasus.bin iso/boot/pegasus.bin
	printf '%s\n' 'set timeout=0' 'set default=0' '' 'menuentry "My-OS" {' '  multiboot /boot/pegasus.bin' '  boot' '}' > iso/boot/grub/grub.cfg
	# Try to provide grub modules dir if present (common locations)
	if [ -d /usr/lib/grub/i386-pc ]; then \
		grub-mkrescue -d /usr/lib/grub/i386-pc --output=pegasus.iso iso; \
	elif [ -d /usr/lib/grub2/i386-pc ]; then \
		grub-mkrescue -d /usr/lib/grub2/i386-pc --output=pegasus.iso iso; \
	else \
		grub-mkrescue --output=pegasus.iso iso || echo "grub-mkrescue failed: ensure grub modules and xorriso are installed"; \
	fi
	rm -rf iso
install: pegasus.bin
	sudo cp $< /boot/pegasus.bin

clean:
	rm -rf $(OBJ_DIR) pegasus.bin pegasus.iso iso
	# remove generated initrd if present
	rm -f $(INITRD_SRC)
