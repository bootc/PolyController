
# Extra CFLAGS
CFLAGS += -fno-jump-tables -mcall-prologues

# Set .text section address for bootloader
LDFLAGS += -Wl,--section-start=.text=$(CONFIG_BOOTLDR_START_ADDR)

# Fuse values
EFUSE =  0xFD	# default FF (BOD 2.7V)
HFUSE =  0xD0	# default 99 (bootrst + eesave)
LFUSE =  0xF7	# default 62 (no clock div, crystal osc, slow power)
UNLOCK = 0x3F	# everything unlocked
LOCK =   0x2F	# BLB11 programmed (no SPM in bootloader section)

# Special programming rule for the bootloader
program_bl: $(TARGET).hex $(TARGET).eep
	# Unlock device & program fuses
	$(AVRDUDE) $(AVRDUDE_FLAGS) -e -u \
		-U lock:w:$(strip $(UNLOCK)):m -U efuse:w:$(strip $(EFUSE)):m \
		-U hfuse:w:$(strip $(HFUSE)):m -U lfuse:w:$(strip $(LFUSE)):m
	# Program flash and lock bootloader section
	$(AVRDUDE) $(AVRDUDE_FLAGS) \
		$(AVRDUDE_WRITE_FLASH) $(AVRDUDE_WRITE_EEPROM) \
		-U lock:w:$(strip $(LOCK)):m

$(curdir)-y += main.c

.PHONY: program_bl

$(eval $(call subdir,$(curdir)))

