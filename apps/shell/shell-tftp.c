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
#include <tftp.h>
#include <resolv_helper.h>
#include <polyfs.h>
#include <flashmgt.h>

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"

static const char progress[] PROGMEM = {
	'-', '\\', '|', '/'
};

PROCESS(shell_tftpupdate_process, "tftpupdate");
SHELL_COMMAND(tftpupdate_command,
	"tftpupdate", "tftpupdate: update firmware over TFTP",
	&shell_tftpupdate_process);
INIT_SHELL_COMMAND(tftpupdate_command);

struct tftpupdate_params {
	struct resolv_helper_status res;
	char filename[32];
	struct tftp_state s;
};

struct tftpupdate_params *tftpupdate = NULL;

static void tftpupdate_usage(void) {
	shell_output_P(&tftpupdate_command,
		PSTR("Usage: tftpupdate <server> <filename>\n"));
}

static void tftpupdate_cleanup(void) {
	if (tftpupdate) {
		if (tftpupdate->s.conn) {
			uip_udp_remove(tftpupdate->s.conn);
			tftpupdate->s.conn = NULL;
		}

		free(tftpupdate);
		tftpupdate = NULL;
	}
}

static int tftpupdate_iofunc(struct tftp_state *s, uint32_t offset,
		uint16_t size, void *buf)
{
	int err;

	// Write the flash block
	err = flashmgt_sec_write_block(buf, offset, size);
	if (err) {
		shell_output_P(&tftpupdate_command,
			PSTR("\rWrite error %d at block %d\n"),
			err, s->block);
	}
	else {
		shell_output_P(&tftpupdate_command,
			PSTR("\r%c"),
			pgm_read_byte(&progress[s->block % sizeof(progress)]));
	}

	return err;
}

PROCESS_THREAD(shell_tftpupdate_process, ev, data) {
	char *s;
	int err;

	PROCESS_BEGIN();

	// Make sure we got some arguments
	if ((data == NULL) || (strlen(data) == 0)) {
		tftpupdate_usage();
		PROCESS_EXIT();
	}

	// Find the space between the server and the filename
	s = strchr(data, ' ');

	// Check we got <server><SPACE><filename>
	if (s == NULL) {
		tftpupdate_usage();
		PROCESS_EXIT();
	}

	// Clean up after a previous run
	if (tftpupdate) {
		shell_output_P(&tftpupdate_command,
			PSTR("Previous run failed to clean up after itself! Clobbering.\n"));
		tftpupdate_cleanup();
	}

	// Allocate our memory block if necessary
	tftpupdate = malloc(sizeof(*tftpupdate));
	if (tftpupdate == NULL) {
		shell_output_P(&tftpupdate_command,
			PSTR("Out of memory!\n"));
		PROCESS_EXIT();
	}
	tftpupdate->s.conn = NULL;

	// Copy out the server name
	int len = s - (char *)data;
	if (len >= sizeof(tftpupdate->res.name)) {
		len = sizeof(tftpupdate->res.name) - 1;
	}
	strncpy(tftpupdate->res.name, data, len);
	tftpupdate->res.name[len] = '\0';

	// Copy out the filename
	strncpy(tftpupdate->filename, s + 1, sizeof(tftpupdate->filename) - 1);
	tftpupdate->filename[sizeof(tftpupdate->filename) - 1] = '\0';

	// Check there is something in the filename
	if (strlen(tftpupdate->filename) == 0) {
		tftpupdate_usage();
		tftpupdate_cleanup();
		PROCESS_EXIT();
	}

	// Tell the user what's going on
	shell_output_P(&tftpupdate_command,
		PSTR("Looking up '%s'...\n"), tftpupdate->res.name);

	// Start the lookup
	resolv_helper_lookup(&tftpupdate->res);
	while (1) {
		PROCESS_WAIT_EVENT();
		resolv_helper_appcall(&tftpupdate->res, ev, data);

		if (tftpupdate->res.state == RESOLV_HELPER_STATE_ASKING) {
			continue;
		}
		else if (tftpupdate->res.state == RESOLV_HELPER_STATE_DONE) {
			break;
		}
		else if (tftpupdate->res.state == RESOLV_HELPER_STATE_ERROR) {
			shell_output_P(&tftpupdate_command,
				PSTR("Error during DNS lookup.\n"));
			tftpupdate_cleanup();
			PROCESS_EXIT();
		}
		else {
			shell_output_P(&tftpupdate_command,
				PSTR("Error during DNS lookup. (unknown state)\n"));
			tftpupdate_cleanup();
			PROCESS_EXIT();
		}
	}

	// Tell the user what's going on
	shell_output_P(&tftpupdate_command,
		PSTR("Preparing to write to flash...\n"));

	// Start the flash write
	err = flashmgt_sec_write_start();
	if (err) {
		shell_output_P(&tftpupdate_command,
			PSTR("Could not set up flash write (%d).\n"), err);
		tftpupdate_cleanup();
		PROCESS_EXIT();
	}

	// More user info
	shell_output_P(&tftpupdate_command,
		PSTR("Requesting '%s' from %u.%u.%u.%u...\n"),
		tftpupdate->filename,
		uip_ipaddr_to_quad(&tftpupdate->res.ipaddr));

	// Start the TFTP transfer
	tftpupdate->s.addr = tftpupdate->res.ipaddr;
	tftpupdate->s.iofunc = tftpupdate_iofunc;
	tftp_init(&tftpupdate->s);
	tftp_get(&tftpupdate->s, tftpupdate->filename);
	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == tcpip_event) {
			tftp_appcall(&tftpupdate->s);

			if (tftpupdate->s.state == TFTP_STATE_CLOSE) {
				shell_output_P(&tftpupdate_command,
					PSTR("\rTransfer complete (%lu bytes read).\n"),
					tftpupdate->s.size);

				// Send final ACK
				PROCESS_WAIT_EVENT();
				break;
			}
			else if (tftpupdate->s.state == TFTP_STATE_ERR) {
				shell_output_P(&tftpupdate_command,
					PSTR("\rAborting due to error.\n"));

				// Abort the flash write
				flashmgt_sec_write_abort();

				// Make sure error message is sent
				PROCESS_WAIT_EVENT();

				tftpupdate_cleanup();
				PROCESS_EXIT();
			}
			else if (tftpupdate->s.state == TFTP_STATE_TIMEOUT) {
				shell_output_P(&tftpupdate_command,
					PSTR("\rTransfer timed out.\n"));

				// Abort the flash write
				flashmgt_sec_write_abort();

				// Make sure error message is sent
				PROCESS_WAIT_EVENT();

				tftpupdate_cleanup();
				PROCESS_EXIT();
			}
		}
	}

	// Tell the user what's going on
	shell_output_P(&tftpupdate_command,
		PSTR("Completing update process...\n"));

	// Finish the flash write process
	err = flashmgt_sec_write_finish();
	if (err) {
		shell_output_P(&tftpupdate_command,
			PSTR("Could not apply firmware update (%d)\n"),
			err);
	}
	else {
		shell_output_P(&tftpupdate_command,
			PSTR("New firmware image is in flash. "
				"Please reboot to apply the upgrade.\n"),
			err);
	}

	tftpupdate_cleanup();
	PROCESS_END();
}

