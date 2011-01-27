

MKPOLYFS := $(curdir)/polyfs/mkpolyfs
POLYFSCK := $(curdir)/polyfs/polyfsck
EXTRA_CLEAN_FILES += $(MKPOLYFS) $(POLYFSCK)

$(MKPOLYFS): $(curdir)/polyfs

$(curdir)/polyfs:
	$(MAKE) -C $(curdir)/polyfs/

.PHONY: $(curdir)/polyfs

$(eval $(call subdir,$(curdir)))

