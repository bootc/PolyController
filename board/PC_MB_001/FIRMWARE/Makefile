
bin: $(TARGET).bin
pfs: tools/polyfs $(TARGET).pfs

%.bin: %.elf
	@echo
	@echo $(MSG_FLASH) $@
	$(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock $< $@

# FIXME: more deps...
%.pfs: %.bin $(BUILDDIR)/fsroot
	@echo
	@echo Creating firmware image: $@
	$(MKPOLYFS) -v -E -i $(TARGET).bin -n $(BOARD) -q -l \
		$(BUILDDIR)/fsroot $@
	$(POLYFSCK) -v $@

$(BUILDDIR)/fsroot: $(IMAGE_DIR)/fsroot 
	$(REMOVEDIR) "$(BUILDDIR)/fsroot"
	rsync -a --exclude .svn "$(IMAGE_DIR)/fsroot/" "$(BUILDDIR)/fsroot/"
	touch "$(BUILDDIR)/fsroot"

FSFILES := $(shell find $(IMAGE_DIR)/fsroot -mindepth 1 -name .svn -prune -o -print)
$(IMAGE_DIR)/fsroot: $(FSFILES)
	touch $(IMAGE_DIR)/fsroot

$(curdir)-y += main.c

EXTRA_BUILD_RULES += pfs
EXTRA_CLEAN_FILES += $(TARGET).bin $(TARGET).pfs
EXTRA_CLEAN_DIRS += $(BUILDDIR)/fsroot

.PHONY: bin

$(eval $(call subdir,$(curdir)))

