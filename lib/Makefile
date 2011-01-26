
EXTRAINCDIRS += $(curdir)/

$(curdir)-$(CONFIG_LIB_CONTIKI) += contiki/
$(curdir)-$(CONFIG_LIB_LZO) += minilzo/minilzo.c
$(curdir)-$(CONFIG_LIB_POLYFS) += polyfs.c
$(curdir)-$(CONFIG_LIB_SNTP) += sntp.c
$(curdir)-$(CONFIG_LIB_STACK) += stack.c

$(eval $(call subdir,$(curdir)))

