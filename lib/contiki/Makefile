
CONTIKI_CVSROOT=":pserver:anonymous@contiki.cvs.sourceforge.net:/cvsroot/contiki"
CONTIKI_CVSREV="HEAD"
CONTIKI_CVSOPTS=-z3 -d $(CONTIKI_CVSROOT) export -r $(CONTIKI_CVSREV)
CONTIKI_CVSMODULE=contiki-2.x

# Contiki OS files
CONTIKI := contiki
CONTIKI_INC := \
	$(BUILDDIR)/$(CONTIKI)/core/ \
	$(BUILDDIR)/$(CONTIKI)/cpu/avr/
CONTIKI_SYSTEM := \
	process.c procinit.c autostart.c \
	timetable.c timetable-aggregate.c compower.c mt.c \
	timer.c etimer.c ctimer.c energest.c rtimer.c stimer.c
CONTIKI_LIBS := \
	memb.c mmem.c list.c \
	print-stats.c ifft.c random.c checkpoint.c ringbuf.c
CONTIKI_AVR := \
	mtarch.c
CONTIKI_DEV := \
	serial-line.c
CONTIKI_NET := \
	netstack.c uip-debug.c packetbuf.c queuebuf.c packetqueue.c

ifeq ($(CONFIG_LIB_CONTIKI_IPV6),y)

CFLAGS += -DUIP_CONF_IPV6=1
CONTIKI_UIP := \
	uip6.c tcpip.c psock.c uip-udp-packet.c uip-split.c \
	tcpdump.c uiplib.c
CONTIKI_NET += \
	$(CONTIKI_UIP) uip-icmp6.c uip-nd6.c uip-packetqueue.c \
	neighbor-attr.c neighbor-info.c uip-ds6.c
#sicslowpan.c 

else # CONFIG_LIB_CONTIKI_IPV6

CONTIKI_UIP := \
	uip.c uiplib.c tcpip.c psock.c hc.c uip-split.c uip-fw.c \
	uip-fw-drv.c uip_arp.c tcpdump.c uip-neighbor.c uip-udp-packet.c \
	uip-over-mesh.c dhcpc.c #rawpacket-udp.c
CONTIKI_NET += \
	$(CONTIKI_UIP) uaodv.c uaodv-rt.c

endif # CONFIG_LIB_CONTIKI_IPV6

CONTIKI_SRC := \
	${addprefix $(BUILDDIR)/$(CONTIKI)/core/sys/,$(CONTIKI_SYSTEM)} \
	${addprefix $(BUILDDIR)/$(CONTIKI)/core/lib/,$(CONTIKI_LIBS)} \
	${addprefix $(BUILDDIR)/$(CONTIKI)/core/net/,$(CONTIKI_NET)} \
	${addprefix $(BUILDDIR)/$(CONTIKI)/cpu/avr/,$(CONTIKI_AVR)} \
	${addprefix $(BUILDDIR)/$(CONTIKI)/core/dev/,$(CONTIKI_DEV)}


LIB_CONTIKI_DIR := $(curdir)
EXTRAINCDIRS += $(CONTIKI_INC)
SRC += $(CONTIKI_SRC)

# make all source depend on contiki
$(SRC): $(BUILDDIR)/$(CONTIKI)

$(BUILDDIR)/$(CONTIKI):
	$(REMOVEDIR) "$(BUILDDIR)/$(CONTIKI_CVSMODULE)"
	cd $(BUILDDIR) && cvs $(CONTIKI_CVSOPTS) $(CONTIKI_CVSMODULE)
	for F in `cat $(LIB_CONTIKI_DIR)/patches/series`; do \
		cat "$(LIB_CONTIKI_DIR)/patches/$$F" | (cd "$(BUILDDIR)/$(CONTIKI_CVSMODULE)" && patch -p0 || exit 1); \
	done
	mv "$(BUILDDIR)/$(CONTIKI_CVSMODULE)" "$(BUILDDIR)/$(CONTIKI)"

$(eval $(call subdir,$(curdir)))

