
pfs: tools/polyfs $(TARGET).pfs

%.bin: %.elf
	@echo
	@echo $(MSG_FLASH) $@
	$(OBJCOPY) -O binary -R .eeprom -R .fuse -R .lock $< $@

# FIXME: more deps...
%.pfs: $(BUILDDIR)/fsroot $(BUILDDIR)/fsroot/www/version.shtml
	@echo
	@echo Creating firmware image: $@
	$(MKPOLYFS) -v -E -n $(BOARD) -q -l \
		$(BUILDDIR)/fsroot $@
	$(POLYFSCK) -v $@

$(BUILDDIR)/fsroot: $(IMAGE_DIR)/fsroot $(TARGET).bin
	$(REMOVEDIR) "$(BUILDDIR)/fsroot"
	rsync -a \
		--exclude .svn \
		--exclude .*.swp \
		--exclude .DS_Store --exclude ._* \
		"$(IMAGE_DIR)/fsroot/" "$(BUILDDIR)/fsroot/"
	cp $(TARGET).bin "$(BUILDDIR)"/fsroot/system.bin
	touch "$(BUILDDIR)/fsroot"

$(BUILDDIR)/fsroot/www/version.shtml: $(TARGET).bin
	@echo "%!:/head.html" > $@
	@echo "<h1>Build &amp; Version Information</h1>" >> $@
	@echo "<pre>" >> $@
	@echo -n "Build Date: " >> $@
	@date -r $(TARGET).bin >> $@
	@svn info . | grep Revision >> $@
	@echo >> $@
	@echo "Changed files:" >> $@
	@svn status -q >> $@
	@echo "</pre>" >> $@
	@echo "%!:/foot.html" >> $@

FSFILES := $(shell find $(IMAGE_DIR)/fsroot -mindepth 1 -name .svn -prune -o -print)
$(IMAGE_DIR)/fsroot: $(FSFILES)
	touch $(IMAGE_DIR)/fsroot

EXTRA_BUILD_RULES += pfs
EXTRA_CLEAN_FILES += $(TARGET).bin $(TARGET).pfs
EXTRA_CLEAN_DIRS += $(BUILDDIR)/fsroot

PRINTF_LIB = $(PRINTF_LIB_FLOAT)

.PHONY: pfs

$(eval $(call subdir,$(curdir)))

