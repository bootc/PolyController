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
#include <stdarg.h>
#include <board.h>
#include <polyfs.h>
#include <flashmgt.h>
#include "shell.h"

#include <string.h>
#include <avr/pgmspace.h>

PROCESS(shell_info_process, "info");
SHELL_COMMAND(info_command,
	"info", "info: software and hardware information",
	&shell_info_process);
INIT_SHELL_COMMAND(info_command);

PROCESS_THREAD(shell_info_process, ev, data) {
	struct board_info bi;

	PROCESS_BEGIN();

	shell_output_P(&info_command,
		PSTR("Software Information:\n"));
	shell_output_P(&info_command,
		PSTR("  Version: " CONFIG_VERSION "\n"));
	shell_output_P(&info_command,
		PSTR("  VCS Rev: %d\n"), VCS_REV);

	if (flashmgt_pfs) {
		shell_output_P(&info_command,
			PSTR("  FW CRC:  %08lx\n"),
			flashmgt_pfs->sb.crc);
	}
	shell_output_P(&info_command,
		PSTR("\n"));

	// Read hardware info
	board_info_read(&bi);

	// Validate the info block
	int ret = board_info_validate(&bi);

	if (ret) {
		shell_output_P(&info_command,
			PSTR("Hardware information block is not valid!\n"));
	}
	else {
		shell_output_P(&info_command,
			PSTR("Hardware Information:\n"));
		shell_output_P(&info_command,
			PSTR("  Model:     %s\n"), bi.model);
		shell_output_P(&info_command,
			PSTR("  Revision:  %s\n"), bi.hw_rev);
		shell_output_P(&info_command,
			PSTR("  Serial:    %s\n"), bi.serial);
		shell_output_P(&info_command,
			PSTR("  Mfr. Date: %04d-%02d-%02d\n"),
			bi.mfr_year, bi.mfr_month, bi.mfr_day);
	}

	shell_output_P(&info_command,
		PSTR("\n"));

	PROCESS_END();
}

