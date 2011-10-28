/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "client.h"
#include "server.h"
#include "messages.h"
#include "util.h"

int main(int argc, char *argv[])
{
	argc--, argv++;

	int iamserver = 0;
	int port = 3333;
	in_addr_t address = INADDR_NONE;
	in_addr_t local_address = INADDR_NONE;
	int interval_us = 1000 * 1000;
	int probe_count = 5;
	int probe_size = MIN_PAYLOAD; // UDP payload, max 65535
	int dont_fragment = 1; // 0/1
	int timeout_us = 1000 * 1000; // can be zero
	int burst = 1; // how many packets to send at once
	int burst_timeout_us = 100 * 1000; // how long to wait after the rest of the burst probe replies
	int verbosity = -1;
	int source_only = 0; // client: 1 = don't read replies
	int sink_only = 0; // server: 1 = don't send replies
	int randomize_interval = 0;
	int busywaiting = 0; // client: high precision intervals between packets
	int nobusywaiting = 0; // client: disable busy waiting

	{
		srand(gettimestamp_us() / 1000LL);
	}

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-l") == 0) {
			iamserver = 1;

		} else if (strcmp(argv[i], "-p") == 0) {
			if (i == argc - 1)
				die("Argument -p must be followed by a port number");
			if (sscanf(argv[i+1], "%d", &port) != 1)
				die("Argument -p must be followed by a valid port number");
			if (port <= 0 || port > 65535)
				die("Argument -p must be followed by a valid port number");

		} else if (strcmp(argv[i], "-a") == 0) {
			if (i == argc - 1)
				die("Argument -a must be followed by an IPv4 address in decimal dot representation");
			address = inet_addr(argv[i+1]);
			if (address == INADDR_NONE)
				die("Argument -a must be followed by an IPv4 address in decimal dot representation");

		} else if (strcmp(argv[i], "-la") == 0) {
			if (i == argc - 1)
				die("Argument -la must be followed by an IPv4 address in decimal dot representation");
			local_address = inet_addr(argv[i+1]);
			if (local_address == INADDR_NONE)
				die("Argument -la must be followed by an IPv4 address in decimal dot representation");

		} else if (strcmp(argv[i], "-i") == 0) {
			if (i == argc - 1)
				die("Argument -i must be followed by a delay in us");
			if (sscanf(argv[i+1], "%d", &interval_us) != 1)
				die("Argument -i must be followed by a valid delay in us");
			if (interval_us < 0)
				die("Argument -i must be followed by a valid delay in us");

		} else if (strcmp(argv[i], "-v") == 0) {
			if (i == argc - 1)
				die("Argument -v must be followed by a number");
			if (sscanf(argv[i+1], "%d", &verbosity) != 1)
				die("Argument -v must be followed by a number");

		} else if (strcmp(argv[i], "-c") == 0) {
			if (i == argc - 1)
				die("Argument -c must be followed by a positive number");
			if (sscanf(argv[i+1], "%d", &probe_count) != 1)
				die("Argument -c must be followed by a positive number");
			if (probe_count < 0)
				die("Argument -c must be followed by a positive number");

		} else if (strcmp(argv[i], "-t") == 0) {
			if (i == argc - 1)
				die("Argument -t must be followed by a positive number");
			if (sscanf(argv[i+1], "%d", &timeout_us) != 1)
				die("Argument -t must be followed by a positive number");
			if (timeout_us < 0)
				die("Argument -t must be followed by a positive number");

		} else if (strcmp(argv[i], "-b") == 0) {
			if (i == argc - 1)
				die("Argument -b must be followed by a positive number");
			if (sscanf(argv[i+1], "%d", &burst) != 1)
				die("Argument -b must be followed by a positive number");
			if (burst <= 0)
				die("Argument -b must be followed by a positive number");

		} else if (strcmp(argv[i], "-bt") == 0) {
			if (i == argc - 1)
				die("Argument -bt must be followed by a positive number");
			if (sscanf(argv[i+1], "%d", &burst_timeout_us) != 1)
				die("Argument -bt must be followed by a positive number");
			if (burst_timeout_us <= 0)
				die("Argument -bt must be followed by a positive number");

		} else if (strcmp(argv[i], "-s") == 0) {
			char message[256];
			sprintf(message, "Argument -s must be followed by a positive number >= %lu and <= 65535", MIN_PAYLOAD);
			if (i == argc - 1)
				die(message);
			if (sscanf(argv[i+1], "%d", &probe_size) != 1)
				die(message);
			if (probe_size < (int)MIN_PAYLOAD || probe_size > 65535)
				die(message);

		} else if (strcmp(argv[i], "--allow-frags") == 0) {
			dont_fragment = 0;

		} else if (strcmp(argv[i], "--source-only") == 0) {
			source_only = 1;

		} else if (strcmp(argv[i], "--sink-only") == 0) {
			sink_only = 1;

		} else if (strcmp(argv[i], "--randomize-interval") == 0) {
			randomize_interval = 1;

		} else if (strcmp(argv[i], "--busywaiting") == 0) {
			busywaiting = 1;

		} else if (strcmp(argv[i], "--nobusywaiting") == 0) {
			nobusywaiting = 1;

		} else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			fprintf(stderr, "Usage:\n");

			fprintf(stderr, "Server: udpping -l -p <port> [--sink-only]\n");
			fprintf(stderr, "Server default: -p 3333\n");

			fprintf(stderr, "Client: udpping -a <serveraddress> -p <port> -i <interval_us> -t <timeout_us> -c <count> -s <size> -b <burst_size> -bt <burst_timeout_us> -v <verbosity> [--allow-frags --source-only --randomize-interval --busywaiting --nobusywaiting]\n");
			fprintf(stderr, "Client default: -a 127.0.0.1 -p 3333 -i 1000000 -t 1000000 -c 5 -s 20 -b 1 -bt 100000 -v 1\n");
			exit(0);
		}
	}

	if (address == INADDR_NONE) {
		if (iamserver) {
			address = htonl(INADDR_ANY);
		} else {
			address = htonl(INADDR_LOOPBACK);
		}
	}

	if (busywaiting && nobusywaiting) {
		fprintf(stderr, "Client: cannot use both busywaiting and nobusywaiting\n");
		exit(-1);
	}

	if (iamserver) {
		if (verbosity < 0)
			verbosity = 0;
		server(port, verbosity, sink_only);
	} else {
		if (verbosity < 0)
			verbosity = 1;
		client(address, port, probe_size, probe_count, interval_us, dont_fragment,
			  timeout_us, burst, burst_timeout_us, source_only, randomize_interval, verbosity,
			  busywaiting, local_address, nobusywaiting);
	}

	return 0;
}
