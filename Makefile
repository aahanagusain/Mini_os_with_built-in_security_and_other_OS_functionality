CC=gcc
AS=as
GCCPARAMS = -m32 -nostdlib -fno-builtin -fno-exceptions -ffreestanding -fno-leading-underscore -Wall -Wextra -Wpedantic
ASPARAMS = --32
LDPARAMS = -melf_i386

SRC_DIR=src
HDR_DIR=include/
OBJ_DIR=obj
ISO_DIR=iso

SRC_FILES1=$(wildcard $(SRC_DIR)/*.c)
OBJ_FILES1=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_FILES1))
SRC_FILES2=$(wildcard $(SRC_DIR)/*.s)
OBJ_FILES2=$(patsubst $(SRC_DIR)/%.s, $(OBJ_DIR)/%.o, $(SRC_FILES2))
SRC_FILES3=$(wildcard $(SRC_DIR)/*.asm)
OBJ_FILES3=$(patsubst $(SRC_DIR)/%.asm, $(OBJ_DIR)/%.o, $(SRC_FILES3))

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

my-os.bin: $(SRC_DIR)/linker.ld $(OBJ_FILES1) $(OBJ_FILES2) $(OBJ_FILES3)
	ld $(LDPARAMS) -T $< -o $@ $(OBJ_DIR)/*.o

my-os.iso: my-os.bin
	@if [ -x ./update_version ]; then ./update_version; else echo "(skipping update_version)"; fi
	mkdir -p iso/boot/grub
	cp my-os.bin iso/boot/my-os.bin
	printf '%s\n' 'set timeout=0' 'set default=0' '' 'menuentry "My-OS" {' '  multiboot /boot/my-os.bin' '  boot' '}' > iso/boot/grub/grub.cfg
	# Try to provide grub modules dir if present (common locations)
	if [ -d /usr/lib/grub/i386-pc ]; then \
		grub-mkrescue -d /usr/lib/grub/i386-pc --output=my-os.iso iso; \
	elif [ -d /usr/lib/grub2/i386-pc ]; then \
		grub-mkrescue -d /usr/lib/grub2/i386-pc --output=my-os.iso iso; \
	else \
		grub-mkrescue --output=my-os.iso iso || echo "grub-mkrescue failed: ensure grub modules and xorriso are installed"; \
	fi
	rm -rf iso
install: my-os.bin
	sudo cp $< /boot/my-os.bin

clean:
	rm -rf $(OBJ_DIR) my-os.bin my-os.iso iso
