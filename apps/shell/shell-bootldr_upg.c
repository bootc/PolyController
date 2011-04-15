/*
 * This file is part of the PolyController firmware source code.
 * Copyright (C) 2011 Chris Boot.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <contiki.h>
#include <polyfs.h>
#include <flashmgt.h>
#include <stubboot.h>
#include <board.h>
#include <util/crc16.h>

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"

// This was hacked up very quickly and needs some serious review. I just wanted
// to get the basics tested so this could be done in the field if necessary!

PROCESS(shell_bootldr_upg_process, "bootldr_upg");
SHELL_COMMAND(bootldr_upg_command,
	"bootldr_upg", "bootldr_upg: update firmware over TFTP",
	&shell_bootldr_upg_process);
INIT_SHELL_COMMAND(bootldr_upg_command);

PROCESS_THREAD(shell_bootldr_upg_process, ev, data) {
	struct polyfs_inode inode;
	int ret;
	uint16_t filesz;
	uint8_t pages;
	uint16_t bufsz;
	uint8_t *buf;
	uint16_t offset;
	struct version_info *ver;
	uint16_t crc = 0xffff;

	PROCESS_BEGIN();

	// Find the update file
	ret = polyfs_lookup(flashmgt_pfs, "/bootloader.bin", &inode);
	if (ret) {
		shell_output_P(&bootldr_upg_command,
			PSTR("Could not locate bootloader update file. Aborting.\n"));
		PROCESS_EXIT();
	}

	filesz = POLYFS_24(inode.size);
	pages = (filesz + (SPM_PAGESIZE - 1)) / SPM_PAGESIZE;
	bufsz = pages * SPM_PAGESIZE;

	// Allocate a buffer large enough for the file
	buf = malloc(bufsz);
	if (!buf) {
		shell_output_P(&bootldr_upg_command,
			PSTR("Could not allocate a buffer large enough. Aborting.\n"));
		PROCESS_EXIT();
	}

	// Copy the file into the buffer
	memset(buf, 0xff, bufsz);
	for (offset = 0; offset < filesz;) {
		int32_t ret = polyfs_fread(flashmgt_pfs, &inode, buf + offset,
			offset, filesz - offset);
		if (ret == 0) {
			break;
		}
		else if (ret > 0) {
			offset += ret;
		}
		else {
			free(buf);
			shell_output_P(&bootldr_upg_command,
				PSTR("Could not read bootloader update file: %d.\n"), ret);
			PROCESS_EXIT();
		}
	}

	ver = (struct version_info *)(&buf[VERSION_INFO_ADDR]);

	shell_output_P(&bootldr_upg_command,
		PSTR("New bootloader version: %s\n"),
		ver->str);


	// Work out the CRC
	for (offset = 0; offset < bufsz; offset++) {
		crc = _crc16_update(crc, buf[offset]);
	}

	ret = stubboot_update_loader(pages, crc, buf);
	if (ret < 0) {
		shell_output_P(&bootldr_upg_command,
			PSTR("Upgrade failed: %d\n"),
			ret);
		free(buf);
		PROCESS_EXIT();
	}
	else if (ret > 0) {
		shell_output_P(&bootldr_upg_command,
			PSTR("Upgrade successful after %d retried writes.\n"),
			ret);
	}

	// Check the CRC
	uint16_t crc1 = 0xffff;
	for (offset = 0; offset < bufsz; offset++) {
		uint_farptr_t addr = CONFIG_BOOTLDR_START_ADDR + offset;
		crc1 = _crc16_update(crc1, pgm_read_byte_far(addr));
	}

	if (crc1 == crc) {
		shell_output_P(&bootldr_upg_command,
			PSTR("Upgrade successful!\n"));
	}
	else {
		shell_output_P(&bootldr_upg_command,
			PSTR("Upgrade failed. CRC mismatch!\n"));
	}

	PROCESS_END();
}

