/*
 * (C) Copyright 2017
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * This is a simple example how to send a command to
 * a SWUpdate's subprocess. It sends a "feedback"
 * to the suricatta module and waits for the answer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "network_ipc.h"

static void usage(char *program) {
	printf("%s <polling interval 0=from server> ..\n", program);
}

/*
 * Simple example, it does nothing but calling the library
 */
int main(int argc, char *argv[]) {
	int rc;
	ipc_message msg;
	size_t size;
	char *buf;

	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}

	memset(&msg, 0, sizeof(msg));
	msg.data.instmsg.source = SOURCE_SURICATTA;
	msg.data.instmsg.cmd = CMD_CONFIG;

	size = sizeof(msg.data.instmsg.buf);
	buf = msg.data.instmsg.buf;

	/*
	 * Build a json string with the command line parameters
	 * do not check anything, let SWUpdate
	 * doing the checks
	 * An error or a NACK is returned in
	 * case of failure
	 */

	snprintf(buf, size, "{ \"polling\" : \"%lu\"}", strtoul(argv[1], NULL, 10));

	fprintf(stdout, "Sending: '%s'", msg.data.instmsg.buf);

	rc = ipc_send_cmd(&msg);

	fprintf(stdout, " returned %d\n", rc);
	if (!rc)
		fprintf(stdout, "Server returns %s\n",
				(msg.type == ACK) ? "ACK" : "NACK");

	exit(0);
}
