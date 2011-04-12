
# Extra CFLAGS
CFLAGS += -fno-jump-tables

# Set .text section address for bootloader
LDFLAGS += -Wl,--section-start=.text=$(CONFIG_STUBBOOT_START_ADDR)

# We don't want any standard init/libraries
LDFLAGS += -nostartfiles
LDFLAGS += -nodefaultlibs

# Custom linker script
LDFLAGS += -T$(IMAGE_DIR)/link.x

$(curdir)-y += stubboot.c

%.bin: %.elf
	@echo
	@echo $(MSG_FLASH) $@
	$(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock $< $@

EXTRA_BUILD_RULES += $(TARGET).bin
EXTRA_CLEAN_FILES += $(TARGET).bin

$(eval $(call subdir,$(curdir)))

