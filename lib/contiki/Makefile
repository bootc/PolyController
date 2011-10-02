
# Contiki OS files
CONTIKI := contiki-os
CONTIKI_INC := \
	$(curdir)/$(CONTIKI)/core/ \
	$(curdir)/$(CONTIKI)/cpu/avr/
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
	${addprefix $(curdir)/$(CONTIKI)/core/sys/,$(CONTIKI_SYSTEM)} \
	${addprefix $(curdir)/$(CONTIKI)/core/lib/,$(CONTIKI_LIBS)} \
	${addprefix $(curdir)/$(CONTIKI)/core/net/,$(CONTIKI_NET)} \
	${addprefix $(curdir)/$(CONTIKI)/cpu/avr/,$(CONTIKI_AVR)} \
	${addprefix $(curdir)/$(CONTIKI)/core/dev/,$(CONTIKI_DEV)}

LIB_CONTIKI_DIR := $(curdir)
EXTRAINCDIRS += $(CONTIKI_INC)
SRC += $(CONTIKI_SRC)

$(eval $(call subdir,$(curdir)))

