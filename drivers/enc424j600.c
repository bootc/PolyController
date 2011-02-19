/******************************************
 * Title        : Microchip ENCX24J600 Ethernet Interface Driver
 * Author       : Jiri Melnikov
 * Created      : 28.12.2009
 * Version      : 0.1
 * Target MCU   : Atmel AVR series
 *
 * Description  : This driver provides initialization and transmit/receive
 *                functions for the Microchip ENCX24J600 100Mb Ethernet
 *                Controller and PHY. Only supported interface is SPI, no
 *                PSP interface available by now. No security functions are
 *                are supported by now.
 *
 *                This driver is inspired by ENC28J60 driver from Pascal
 *                Stang (2005).
 *
 *                Many lines of code are taken from Microchip's TCP/IP stack.
 *
 * ****************************************/

#include <stdint.h>

#include "enc424j600.h"

#include <avr/io.h>
#include <util/delay.h>
#include "spi.h"

// Binary constant identifiers for ReadMemoryWindow() and WriteMemoryWindow()
// functions
#define UDA_WINDOW		(0x1)
#define GP_WINDOW		(0x2)
#define RX_WINDOW		(0x4)

// Constants from config.h
#define ENC424J600_CONTROL_PORT	CONFIG_DRIVERS_ENC424J600_CTL_PORT
#define ENC424J600_CONTROL_DDR	CONFIG_DRIVERS_ENC424J600_CTL_DDR
#define ENC424J600_CONTROL_CS	CONFIG_DRIVERS_ENC424J600_CTL_PIN

// Promiscuous mode, uncomment if you want to receive all packets, even those which are not for you
// #define PROMISCUOUS_MODE

// Internal MAC level variables and flags.
static uint8_t currentBank;
static uint16_t nextPacketPointer;


void enc424j600Init(void);
uint16_t enc424j600PacketReceive(uint16_t maxlen, uint8_t* packet);
void enc424j600PacketSend(uint16_t len, uint8_t* packet);
void enc424j600GetMACAddr(uint8_t addr[6]);

void enc424j600MACFlush(void);
static void enc424j600SendSystemReset(void);
uint16_t enc424j600ReadReg(uint16_t address);
void enc424j600WriteReg(uint16_t address, uint16_t data);
uint16_t enc424j600ReadPHYReg(uint8_t address);
void enc424j600WritePHYReg(uint8_t address, uint16_t Data);
static void enc424j600ExecuteOp0(uint8_t op);
uint8_t enc424j600ExecuteOp8(uint8_t op, uint8_t data);
uint16_t enc424j600ExecuteOp16(uint8_t op, uint16_t data);
uint32_t enc424j600ExecuteOp32(uint8_t op, uint32_t data);
static void enc424j600BFSReg(uint16_t address, uint16_t bitMask);
static void enc424j600BFCReg(uint16_t address, uint16_t bitMask);
void enc424j600ReadMemoryWindow(uint8_t window, uint8_t *data, uint16_t length);
void enc424j600WriteMemoryWindow(uint8_t window, uint8_t *data, uint16_t length);
static void enc424j600WriteN(uint8_t op, uint8_t* data, uint16_t dataLen);
static void enc424j600ReadN(uint8_t op, uint8_t* data, uint16_t dataLen);

/********************************************************************
 * INITIALIZATION
 * ******************************************************************/
void enc424j600Init(void) {
	//Set default bank
	currentBank = 0;

	// Make sure CS is pulled high (release device)
	ENC424J600_CONTROL_DDR |= _BV(ENC424J600_CONTROL_CS);
	ENC424J600_CONTROL_PORT |= _BV(ENC424J600_CONTROL_CS);

	// Perform a reliable reset
	enc424j600SendSystemReset();

	// Initialize RX tracking variables and other control state flags
	nextPacketPointer = ENC424J600_RXSTART;

	// Set up TX/RX/UDA buffer addresses
	enc424j600WriteReg(ETXST, ENC424J600_TXSTART);
	enc424j600WriteReg(ERXST, ENC424J600_RXSTART);
	enc424j600WriteReg(ERXTAIL, ENC424J600_RAMSIZE - 2);
	enc424j600WriteReg(EUDAST, ENC424J600_RAMSIZE);
	enc424j600WriteReg(EUDAND, ENC424J600_RAMSIZE + 1);

	// If promiscuous mode is set, than allow accept all packets
#ifdef PROMISCUOUS_MODE
	enc424j600WriteReg(ERXFCON,(ERXFCON_CRCEN | ERXFCON_RUNTEN | ERXFCON_UCEN | ERXFCON_NOTMEEN | ERXFCON_MCEN));
#endif

	// Set PHY Auto-negotiation to support 10BaseT Half duplex,
	// 10BaseT Full duplex, 100BaseTX Half Duplex, 100BaseTX Full Duplex,
	// and symmetric PAUSE capability
	enc424j600WritePHYReg(PHANA, PHANA_ADPAUS0 | PHANA_AD10FD | PHANA_AD10 | PHANA_AD100FD | PHANA_AD100 | PHANA_ADIEEE0);

	// Enable RX packet reception
	enc424j600BFSReg(ECON1, ECON1_RXEN);
}

/********************************************************************
 * UTILS
 * ******************************************************************/

static void enc424j600SendSystemReset(void) {
	// Perform a reset via the SPI/PSP interface
	do {
		// Set and clear a few bits that clears themselves upon reset.
		// If EUDAST cannot be written to and your code gets stuck in this
		// loop, you have a hardware problem of some sort (SPI or PMP not
		// initialized correctly, I/O pins aren't connected or are
		// shorted to something, power isn't available, etc.)
		do {
			enc424j600WriteReg(EUDAST, 0x1234);
		} while (enc424j600ReadReg(EUDAST) != 0x1234);
		// Issue a reset and wait for it to complete
		enc424j600BFSReg(ECON2, ECON2_ETHRST);
		currentBank = 0; while ((enc424j600ReadReg(ESTAT) & (ESTAT_CLKRDY | ESTAT_RSTDONE | ESTAT_PHYRDY)) != (ESTAT_CLKRDY | ESTAT_RSTDONE | ESTAT_PHYRDY));
		_delay_us(300);
		// Check to see if the reset operation was successful by
		// checking if EUDAST went back to its reset default.  This test
		// should always pass, but certain special conditions might make
		// this test fail, such as a PSP pin shorted to logic high.
	} while (enc424j600ReadReg(EUDAST) != 0x0000u);

	// Really ensure reset is done and give some time for power to be stable
	_delay_us(1000);
}

/********************************************************************
 * PACKET TRANSMISSION
 * ******************************************************************/

uint16_t enc424j600PacketReceive(uint16_t len, uint8_t* packet) {
	uint16_t newRXTail;
	RXSTATUS statusVector;

	if (!(enc424j600ReadReg(EIR) & EIR_PKTIF)) {
		return 0;
	}


	// Set the RX Read Pointer to the beginning of the next unprocessed packet
	enc424j600WriteReg(ERXRDPT, nextPacketPointer);


	enc424j600ReadMemoryWindow(RX_WINDOW, (uint8_t*) & nextPacketPointer, sizeof (nextPacketPointer));
	enc424j600ReadMemoryWindow(RX_WINDOW, (uint8_t*) & statusVector, sizeof (statusVector));
	//    if (statusVector.bits.ByteCount <= len) len = statusVector.bits.ByteCount;
	len = (statusVector.bits.ByteCount <= len + 4) ? statusVector.bits.ByteCount - 4 : 0;
	enc424j600ReadMemoryWindow(RX_WINDOW, packet, len);

	newRXTail = nextPacketPointer - 2;
	//Special situation if nextPacketPointer is exactly RXSTART
	if (nextPacketPointer == ENC424J600_RXSTART)
		newRXTail = ENC424J600_RAMSIZE - 2;

	//Packet decrement
	enc424j600BFSReg(ECON1, ECON1_PKTDEC);

	//Write new RX tail
	enc424j600WriteReg(ERXTAIL, newRXTail);

	return len;
}

void enc424j600PacketSend(uint16_t len, uint8_t* packet) {
	// Set the Window Write Pointer to the beginning of the transmit buffer
	enc424j600WriteMemoryWindow(GP_WINDOW, packet, len);

	enc424j600WriteReg(EGPWRPT, ENC424J600_TXSTART);
	enc424j600WriteReg(ETXLEN, len);

	enc424j600MACFlush();


}

void enc424j600GetMACAddr(uint8_t mac_addr[6]) {
	// Get MAC adress
	uint16_t regValue;
	regValue = enc424j600ReadReg(MAADR1);
	mac_addr[0] = ((uint8_t*) & regValue)[0];
	mac_addr[1] = ((uint8_t*) & regValue)[1];
	regValue = enc424j600ReadReg(MAADR2);
	mac_addr[2] = ((uint8_t*) & regValue)[0];
	mac_addr[3] = ((uint8_t*) & regValue)[1];
	regValue = enc424j600ReadReg(MAADR3);
	mac_addr[4] = ((uint8_t*) & regValue)[0];
	mac_addr[5] = ((uint8_t*) & regValue)[1];
}

void enc424j600MACFlush(void) {
	uint16_t w;

	// Check to see if the duplex status has changed.  This can
	// change if the user unplugs the cable and plugs it into a
	// different node.  Auto-negotiation will automatically set
	// the duplex in the PHY, but we must also update the MAC
	// inter-packet gap timing and duplex state to match.
	if (enc424j600ReadReg(EIR) & EIR_LINKIF) {
		enc424j600BFCReg(EIR, EIR_LINKIF);

		// Update MAC duplex settings to match PHY duplex setting
		w = enc424j600ReadReg(MACON2);
		if (enc424j600ReadReg(ESTAT) & ESTAT_PHYDPX) {
			// Switching to full duplex
			enc424j600WriteReg(MABBIPG, 0x15);
			w |= MACON2_FULDPX;
		} else {
			// Switching to half duplex
			enc424j600WriteReg(MABBIPG, 0x12);
			w &= ~MACON2_FULDPX;
		}
		enc424j600WriteReg(MACON2, w);
	}


	// Start the transmission, but only if we are linked.  Supressing
	// transmissing when unlinked is necessary to avoid stalling the TX engine
	// if we are in PHY energy detect power down mode and no link is present.
	// A stalled TX engine won't do any harm in itself, but will cause the
	// MACIsTXReady() function to continuously return FALSE, which will
	// ultimately stall the Microchip TCP/IP stack since there is blocking code
	// elsewhere in other files that expect the TX engine to always self-free
	// itself very quickly.
	if (enc424j600ReadReg(ESTAT) & ESTAT_PHYLNK)
		enc424j600BFSReg(ECON1, ECON1_TXRTS);
}

/********************************************************************
 * READERS AND WRITERS
 * ******************************************************************/

void enc424j600WriteMemoryWindow(uint8_t window, uint8_t *data, uint16_t length) {
	uint8_t op = RBMUDA;

	if (window & GP_WINDOW)
		op = WBMGP;
	if (window & RX_WINDOW)
		op = WBMRX;

	enc424j600WriteN(op, data, length);
}

	void enc424j600ReadMemoryWindow(uint8_t window, uint8_t *data, uint16_t length) {
		if (length == 0u)
			return;

		uint8_t op = RBMUDA;

		if (window & GP_WINDOW)
			op = RBMGP;
		if (window & RX_WINDOW)
			op = RBMRX;

		enc424j600ReadN(op, data, length);
	}

/**
 * Reads from address
 * @variable <uint16_t> address - register address
 * @return <uint16_t> data - data in register
 */
uint16_t enc424j600ReadReg(uint16_t address) {
	uint16_t returnValue;
	uint8_t bank;

	// See if we need to change register banks
	bank = ((uint8_t) address) & 0xE0;
	if (bank <= (0x3u << 5)) {
		if (bank != currentBank) {
			if (bank == (0x0u << 5))
				enc424j600ExecuteOp0(B0SEL);
			else if (bank == (0x1u << 5))
				enc424j600ExecuteOp0(B1SEL);
			else if (bank == (0x2u << 5))
				enc424j600ExecuteOp0(B2SEL);
			else if (bank == (0x3u << 5))
				enc424j600ExecuteOp0(B3SEL);

			currentBank = bank;
		}
		returnValue = enc424j600ExecuteOp16(RCR | (address & 0x1F), 0x0000);
	} else {
		uint32_t returnValue32 = enc424j600ExecuteOp32(RCRU, (uint32_t) address);
		((uint8_t*) & returnValue)[0] = ((uint8_t*) & returnValue32)[1];
		((uint8_t*) & returnValue)[1] = ((uint8_t*) & returnValue32)[2];
	}

	return returnValue;
}

/**
 * Writes to register
 * @variable <uint16_t> address - register address
 * @variable <uint16_t> data - data to register
 */
void enc424j600WriteReg(uint16_t address, uint16_t data) {
	uint8_t bank;

	// See if we need to change register banks
	bank = ((uint8_t) address) & 0xE0;
	if (bank <= (0x3u << 5)) {
		if (bank != currentBank) {
			if (bank == (0x0u << 5))
				enc424j600ExecuteOp0(B0SEL);
			else if (bank == (0x1u << 5))
				enc424j600ExecuteOp0(B1SEL);
			else if (bank == (0x2u << 5))
				enc424j600ExecuteOp0(B2SEL);
			else if (bank == (0x3u << 5))
				enc424j600ExecuteOp0(B3SEL);

			currentBank = bank;
		}
		enc424j600ExecuteOp16(WCR | (address & 0x1F), data);
	} else {
		uint32_t data32;
		((uint8_t*) & data32)[0] = (uint8_t) address;
		((uint8_t*) & data32)[1] = ((uint8_t*) & data)[0];
		((uint8_t*) & data32)[2] = ((uint8_t*) & data)[1];
		enc424j600ExecuteOp32(WCRU, data32);
	}

}

uint16_t enc424j600ReadPHYReg(uint8_t address) {
	uint16_t returnValue;

	// Set the right address and start the register read operation
	enc424j600WriteReg(MIREGADR, 0x0100 | address);
	enc424j600WriteReg(MICMD, MICMD_MIIRD);

	// Loop to wait until the PHY register has been read through the MII
	// This requires 25.6us
	while (enc424j600ReadReg(MISTAT) & MISTAT_BUSY);

	// Stop reading
	enc424j600WriteReg(MICMD, 0x0000);

	// Obtain results and return
	returnValue = enc424j600ReadReg(MIRD);

	return returnValue;
}

void enc424j600WritePHYReg(uint8_t address, uint16_t Data) {
	// Write the register address
	enc424j600WriteReg(MIREGADR, 0x0100 | address);

	// Write the data
	enc424j600WriteReg(MIWR, Data);

	// Wait until the PHY register has been written
	while (enc424j600ReadReg(MISTAT) & MISTAT_BUSY);
}

static void enc424j600ReadN(uint8_t op, uint8_t* data, uint16_t dataLen) {
	// Start SPI
	spi_init();
	ENC424J600_CONTROL_PORT &= ~_BV(ENC424J600_CONTROL_CS);

	// Issue read command
	spi_rw(op);

	// Fill the data buffer
	while (dataLen--) {
		*data++ = spi_rw(0x00);
	}

	// Release SPI
	ENC424J600_CONTROL_PORT |= _BV(ENC424J600_CONTROL_CS);
	spi_release();
}

static void enc424j600WriteN(uint8_t op, uint8_t* data, uint16_t dataLen) {
	// Start SPI
	spi_init();
	ENC424J600_CONTROL_PORT &= ~_BV(ENC424J600_CONTROL_CS);

	// Issue write command
	spi_rw(op);

	// Write data
	while (dataLen--) {
		spi_rw(*data++);
	}

	// Release SPI
	ENC424J600_CONTROL_PORT |= _BV(ENC424J600_CONTROL_CS);
	spi_release();
}

static void enc424j600BFSReg(uint16_t address, uint16_t bitMask) {
	uint8_t bank;

	// See if we need to change register banks
	bank = ((uint8_t) address) & 0xE0;
	if (bank != currentBank) {
		if (bank == (0x0u << 5))
			enc424j600ExecuteOp0(B0SEL);
		else if (bank == (0x1u << 5))
			enc424j600ExecuteOp0(B1SEL);
		else if (bank == (0x2u << 5))
			enc424j600ExecuteOp0(B2SEL);
		else if (bank == (0x3u << 5))
			enc424j600ExecuteOp0(B3SEL);

		currentBank = bank;
	}

	enc424j600ExecuteOp16(BFS | (address & 0x1F), bitMask);
}

static void enc424j600BFCReg(uint16_t address, uint16_t bitMask) {
	uint8_t bank;

	// See if we need to change register banks
	bank = ((uint8_t) address) & 0xE0;
	if (bank != currentBank) {
		if (bank == (0x0u << 5))
			enc424j600ExecuteOp0(B0SEL);
		else if (bank == (0x1u << 5))
			enc424j600ExecuteOp0(B1SEL);
		else if (bank == (0x2u << 5))
			enc424j600ExecuteOp0(B2SEL);
		else if (bank == (0x3u << 5))
			enc424j600ExecuteOp0(B3SEL);

		currentBank = bank;
	}

	enc424j600ExecuteOp16(BFC | (address & 0x1F), bitMask);
}
/********************************************************************
 * EXECUTES
 * ******************************************************************/

/**
 * Execute SPI operation
 * @variable <uint8_t> op - operation
 */
static void enc424j600ExecuteOp0(uint8_t op) {
	// Start SPI
	spi_init();
	ENC424J600_CONTROL_PORT &= ~_BV(ENC424J600_CONTROL_CS);

	// Issue command
	spi_rw(op);

	// Release SPI
	ENC424J600_CONTROL_PORT |= _BV(ENC424J600_CONTROL_CS);
	spi_release();
}

/**
 * Write data to SPI with operation
 * @variable <uint8_t> op - SPI operation
 * @variable <uint8_t> data - data
 */
uint8_t enc424j600ExecuteOp8(uint8_t op, uint8_t data) {
	uint8_t returnValue;

	// Start SPI
	spi_init();
	ENC424J600_CONTROL_PORT &= ~_BV(ENC424J600_CONTROL_CS);

	// Issue command
	spi_rw(op);

	// Send data byte
	returnValue = spi_rw(data);

	// release CS
	ENC424J600_CONTROL_PORT |= _BV(ENC424J600_CONTROL_CS);
	spi_release();

	return returnValue;
}

/**
 * Write data to SPI with operation
 * @variable <uint8_t> op - SPI operation
 * @variable <uint16_t> data - data
 */
uint16_t enc424j600ExecuteOp16(uint8_t op, uint16_t data) {
	uint16_t returnValue;

	// Start SPI
	spi_init();
	ENC424J600_CONTROL_PORT &= ~_BV(ENC424J600_CONTROL_CS);

	// Issue command
	spi_rw(op);

	// Read/write data
	for (int x = 0; x < 2; x++) {
		((uint8_t*) & returnValue)[x] = spi_rw(((uint8_t*) & data)[x]);
	}

	// release CS
	ENC424J600_CONTROL_PORT |= _BV(ENC424J600_CONTROL_CS);
	spi_release();

	return returnValue;
}

/**
 * Write data to SPI with operation
 * @variable <uint8_t> op - SPI operation
 * @variable <uint32_t> data - data
 */
uint32_t enc424j600ExecuteOp32(uint8_t op, uint32_t data) {
	uint16_t returnValue;

	// Start SPI
	spi_init();
	ENC424J600_CONTROL_PORT &= ~_BV(ENC424J600_CONTROL_CS);

	// Issue command
	spi_rw(op);

	// Read/write data
	for (int x = 0; x < 3; x++) {
		((uint8_t*) & returnValue)[x] = spi_rw(((uint8_t*) & data)[x]);
	}

	// release CS
	ENC424J600_CONTROL_PORT |= _BV(ENC424J600_CONTROL_CS);
	spi_release();

	return returnValue;
}
