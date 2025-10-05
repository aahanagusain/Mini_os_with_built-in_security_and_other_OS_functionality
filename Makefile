## Prefer cross-toolchain; fall back to system toolchain if missing
ifneq (,$(shell command -v x86_64-elf-gcc 2>/dev/null))
CC := x86_64-elf-gcc
else
CC := gcc
endif

ifneq (,$(shell command -v x86_64-elf-ld 2>/dev/null))
LD := x86_64-elf-ld
else
# prefer ld if available, otherwise use gcc as the linker driver
ifneq (,$(shell command -v ld 2>/dev/null))
LD := ld
else
LD := $(CC)
endif
endif

# Common flags
CFLAGS := -I src/intf -ffreestanding -m64 -O2 -Wall -Wextra
LDFLAGS := -T targets/x86_64/linker.ld

# Source discovery (silence errors if directories don't exist)
kernel_source_files := $(shell find src/impl/kernel -type f -name '*.c' 2>/dev/null)
kernel_object_files := $(patsubst src/impl/kernel/%.c, build/kernel/%.o, $(kernel_source_files))

x86_64_c_source_files := $(shell find src/impl/x86_64 -type f -name '*.c' 2>/dev/null)
x86_64_c_object_files := $(patsubst src/impl/x86_64/%.c, build/x86_64/%.o, $(x86_64_c_source_files))

x86_64_asm_source_files := $(shell find src/impl/x86_64 -type f -name '*.asm' 2>/dev/null)
x86_64_asm_object_files := $(patsubst src/impl/x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(x86_64_c_object_files) $(x86_64_asm_object_files)

build/kernel/%.o: src/impl/kernel/%.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(patsubst build/kernel/%.o, src/impl/kernel/%.c, $@) -o $@

build/x86_64/%.o: src/impl/x86_64/%.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(patsubst build/x86_64/%.o, src/impl/x86_64/%.c, $@) -o $@

build/x86_64/%.o: src/impl/x86_64/%.asm
	mkdir -p $(dir $@)
	nasm -f elf64 $(patsubst build/x86_64/%.o, src/impl/x86_64/%.asm, $@) -o $@

.PHONY: build-x86_64

build-x86_64: $(kernel_object_files) $(x86_64_object_files)
	mkdir -p dist/x86_64
	# Link using detected linker and flags. Use a shell conditional at recipe time.
	if [ "$(LD)" = "ld" ]; then \
		$(LD) -n $(LDFLAGS) -o dist/x86_64/kernel.bin $(kernel_object_files) $(x86_64_object_files); \
	else \
		$(LD) $(LDFLAGS) -o dist/x86_64/kernel.bin $(kernel_object_files) $(x86_64_object_files); \
	fi
	cp dist/x86_64/kernel.bin targets/x86_64/iso/boot/kernel.bin
	# Call grub-mkrescue with a module directory if present, otherwise fallback to the default
	if [ -d /usr/lib/grub/i386-pc ]; then \
		grub-mkrescue -d /usr/lib/grub/i386-pc -o dist/x86_64/kernel.iso targets/x86_64/iso; \
	elif [ -d /usr/lib/grub2/i386-pc ]; then \
		grub-mkrescue -d /usr/lib/grub2/i386-pc -o dist/x86_64/kernel.iso targets/x86_64/iso; \
	else \
		grub-mkrescue -o dist/x86_64/kernel.iso targets/x86_64/iso; \
	fi

.PHONY: clean
clean:
	rm -rf build dist
