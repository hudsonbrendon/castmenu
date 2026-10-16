# Cast Menu — KallistiOS build. Run inside the dreamcast-sdk container: `make`.
TARGET   = castmenu.elf
VERSION ?= 0.1.0
SRCS     = $(wildcard src/*.c src/ui/*.c src/loader/*.c src/net/*.c)
OBJS     = $(SRCS:.c=.o) romdisk.o
KOS_ROMDISK_DIR = assets/romdisk
KOS_CFLAGS += -std=gnu11 -O2 -Wall -Wextra -Isrc -MMD -MP   # .d files: objects rebuild when headers (lang_ids.h!) change

all: rm-elf assets/romdisk/cast_ldr.bin $(TARGET) 1ST_READ.BIN

include $(KOS_BASE)/Makefile.rules
-include $(SRCS:.c=.d)

clean: rm-elf
	-rm -f $(OBJS) $(SRCS:.c=.d) romdisk.img 1ST_READ.BIN castmenu.bin assets/romdisk/cast_ldr.bin
	$(MAKE) -C ldr clean

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lppp -lm

assets/romdisk/cast_ldr.bin: ldr/ldr.c ldr/start.S ldr/ldr.ld
	$(MAKE) -C ldr
	cp ldr/cast_ldr.bin assets/romdisk/cast_ldr.bin

romdisk.img: assets/romdisk/cast_ldr.bin $(shell find assets/romdisk -type f 2>/dev/null)

# GDI images are read unscrambled; keep the plain binary for the card tool.
1ST_READ.BIN: $(TARGET)
	$(KOS_OBJCOPY) -O binary $(TARGET) 1ST_READ.BIN

.PHONY: all clean rm-elf
