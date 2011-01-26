
bin: $(TARGET).bin
pfs: tools/polyfs $(TARGET).pfs

%.bin: %.elf
	@echo
	@echo $(MSG_FLASH) $@
	$(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock $< $@

# FIXME: more deps...
%.pfs: %.bin $(IMAGE_DIR)/fsroot
	@echo
	@echo Creating firmware image: $@
	$(MKPOLYFS) -v -E -i $(TARGET).bin -n $(BOARD) -q -l \
		$(IMAGE_DIR)/fsroot $@
	$(POLYFSCK) -v $@


$(curdir)-y += main.c

EXTRA_BUILD_RULES += pfs
EXTRA_CLEAN_FILES += $(TARGET).bin $(TARGET).pfs

.PHONY: bin

$(eval $(call subdir,$(curdir)))

