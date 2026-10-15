# Builds cast_ldr.bin (raw, linked at 0x8CE00000). Needs sh-elf-gcc from KOS.
CC = sh-elf-gcc
CFLAGS = -ml -m4-single-only -O2 -ffreestanding -fno-builtin -nostdlib -fno-stack-protector -Wall -Wextra -fno-pic -fno-asynchronous-unwind-tables
all: cast_ldr.bin
cast_ldr.elf: start.S ldr.c ldr.ld
	$(CC) $(CFLAGS) -T ldr.ld -o $@ start.S ldr.c -lgcc
cast_ldr.bin: cast_ldr.elf
	sh-elf-objcopy -O binary $< $@
	@ls -la $@
clean:
	rm -f cast_ldr.elf cast_ldr.bin
