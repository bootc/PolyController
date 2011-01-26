
# Make sure we have BOARD set
ifeq ($(origin BOARD), undefined)
	$(error BOARD is not defined)
endif

# Make sure we have IMAGE set
ifeq ($(origin IMAGE), undefined)
	$(error IMAGE is not defined)
endif


BOARD_DIR := $(curdir)/$(BOARD)
IMAGE_DIR := $(curdir)/$(BOARD)/$(IMAGE)

EXTRAINCDIRS += $(BOARD_DIR)/ $(IMAGE_DIR)/

$(curdir)-y += $(BOARD)/
$(eval $(call subdir,$(curdir)))

