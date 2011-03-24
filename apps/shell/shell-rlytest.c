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

#include <contiki-net.h>
#include <stdio.h>
#include <string.h>

#include "drivers/port_ext.h"
#include "shell.h"

PROCESS(shell_rlytest_process, "rlytest");
SHELL_COMMAND(rlytest_command,
	"rlytest", "rlytest <1234>: test external relay board(s)",
	&shell_rlytest_process);
INIT_SHELL_COMMAND(rlytest_command);

PROCESS_THREAD(shell_rlytest_process, ev, data) {
	//int err;
	char *args = data;

	PROCESS_BEGIN();

	if ((data == NULL) || (strlen(data) != 4)) {
		shell_output_P(&rlytest_command,
			PSTR("Usage: rlytest <1234>"));
		shell_output_P(&rlytest_command,
			PSTR("<1234> is a binary string representing "
				"the state of the relays"));
		shell_output_P(&rlytest_command,
			PSTR("Example: rlytest 1011"));

		PROCESS_EXIT();
	}

	for (int i = 0; i < 4; i++) {
		if (args[i] == '1') {
			port_ext_bit_set(0, i);
		}
		else if (args[i] == '0') {
			port_ext_bit_clear(0, i);
		}
	}

	port_ext_update();

	PROCESS_END();
}

