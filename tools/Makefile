

MKPOLYFS := $(curdir)/polyfs/mkpolyfs
POLYFSCK := $(curdir)/polyfs/polyfsck
EXTRA_CLEAN_RULES += $(curdir)/polyfs_clean

$(MKPOLYFS): $(curdir)/polyfs
$(POLYFSCK): $(curdir)/polyfsck

$(curdir)/polyfs:
	@$(MAKE) -C $(curdir)/polyfs/

$(curdir)/polyfs_clean:
	$(MAKE) -C $(curdir)/polyfs/ clean

.PHONY: $(curdir)/polyfs $(curdir)/polyfs_clean

$(eval $(call subdir,$(curdir)))

