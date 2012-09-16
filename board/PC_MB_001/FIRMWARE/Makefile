
MSG_BINARY = " [BIN]  "
MSG_PFS = " [PFS]  "

# Heap ends and stack starts 2K from RAMEND
CFLAGS += -Wl,--defsym=__heap_end=0x8038ff

pfs: tools/polyfs $(TARGET).pfs

$(TARGET).bin: $(TARGET).elf
	@echo $(MSG_BINARY) $@
	@$(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock $< $@

# FIXME: more deps...
%.pfs: $(BUILDDIR)/fsroot $(BUILDDIR)/fsroot/www/version.shtml $(TARGET).bin
	@echo $(MSG_PFS) $@
	@$(MKPOLYFS) -E -n $(BOARD) -q -l \
		-i $(TARGET).bin \
		$(BUILDDIR)/fsroot $@
	@$(POLYFSCK) $@

$(BUILDDIR)/fsroot: $(IMAGE_DIR)/fsroot $(TARGET).bin
	@$(REMOVEDIR) "$(BUILDDIR)/fsroot"
	@rsync -a \
		--exclude ".*.swp" \
		--exclude .DS_Store --exclude "._*" \
		"$(IMAGE_DIR)/fsroot/" "$(BUILDDIR)/fsroot/"
	@touch "$(BUILDDIR)/fsroot"

$(BUILDDIR)/fsroot/www/version.shtml: $(TARGET).bin $(BUILDDIR)/config.h
	@echo "%!:/head.html" > $@
	@echo "<h1>Build &amp; Version Information</h1>" >> $@
	@echo "<pre>" >> $@
	@echo "Version Number: $(CONFIG_VERSION)" >> $@
	@echo -n "Build Date: " >> $@
	@date -r $(TARGET).bin >> $@
	@echo "GIT Revision: $(VCS_REVISION)" >> $@
	@echo >> $@
	@echo "Changed files:" >> $@
	@git status --porcelain >> $@
	@echo "</pre>" >> $@
	@echo "%!:/foot.html" >> $@

FSFILES := $(shell find $(IMAGE_DIR)/fsroot -mindepth 1 -print)
$(IMAGE_DIR)/fsroot: $(FSFILES)
	@touch $(IMAGE_DIR)/fsroot

EXTRA_BUILD_RULES += pfs
EXTRA_CLEAN_FILES += $(TARGET).bin $(TARGET).pfs
EXTRA_CLEAN_DIRS += $(BUILDDIR)/fsroot

PRINTF_LIB = $(PRINTF_LIB_FLOAT)

.PHONY: pfs

$(eval $(call subdir,$(curdir)))

