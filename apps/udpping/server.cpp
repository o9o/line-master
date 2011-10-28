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

#include "server.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>

#include "messages.h"
#include "util.h"

#define BUFSIZE 65536

static int packetsReceived = 0;

// client -> server delay
static long long avgDelay_ms = 0;

void handleSigInt(int )
{
	fprintf(stdout, "Packets received: %d\n", packetsReceived); fflush(stdout);
	fprintf(stdout, "Average client->server delay: %lld\n", packetsReceived ? avgDelay_ms/packetsReceived : 0LL); fflush(stdout);
	exit(0);
}

void server(int port, int verbosity, int sink_only)
{
	int sock;
	struct sockaddr_in echoserver;
	struct sockaddr_in echoclient;
	char buffer[BUFSIZE];
	unsigned int clientlen, serverlen;
	int received = 0;
	fd_set readfds;

	/* Create the UDP socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		die("Failed to create socket");
	}

	/* Construct the server sockaddr_in structure */
	memset(&echoserver, 0, sizeof(echoserver));       /* Clear struct */
	echoserver.sin_family = AF_INET;                  /* Internet/IP */
	echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   /* Any IP address */
	echoserver.sin_port = htons(port);                /* server port */

	/* Bind the socket */
	serverlen = sizeof(echoserver);
	if (bind(sock, (struct sockaddr *) &echoserver, serverlen) < 0) {
		die("Failed to bind server socket");
	}

	(void) signal(SIGINT, handleSigInt);

	/* Run until cancelled */
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
#if EXIT_ON_STDIN
		FD_SET(STDIN_FILENO, &readfds);
#endif

		select(FD_SETSIZE, &readfds, NULL, NULL, NULL);

#if EXIT_ON_STDIN
		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			handleSigInt(0);
		}
#endif
		if (FD_ISSET(sock, &readfds)) {
			/* Receive a message from the client */
			clientlen = sizeof(echoclient);
			if ((received = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *) &echoclient, &clientlen)) < 0) {
				die("Failed to receive message");
			}
			packetsReceived++;
		} else continue;

		/* Get ts from client */
		long long tsclient = 0;
		memcpy(&tsclient, buffer + OFFSET_C2S_TSCLIENT, SIZE_C2S_TSCLIENT);

		/* Get current time */
		long long tssend = gettimestamp_us();
		/* Send the timestamp to the client */
		memcpy(buffer + OFFSET_S2C_TSSERVER, &tssend, SIZE_S2C_TSSERVER);
		memcpy(buffer + OFFSET_S2C_CLIENTIP, &echoclient.sin_addr.s_addr, SIZE_S2C_CLIENTIP);

		long long delay_us = tssend - tsclient;
		avgDelay_ms += delay_us / 1000;

		if (!sink_only) {
			//if (rand() % 5 > 0) {
				/* Send the message back to client */
				if (sendto(sock, buffer, received, 0, (struct sockaddr *) &echoclient, sizeof(echoclient)) != received) {
					die("Mismatch in number of echo'd bytes");
				}
				if (verbosity) {
					fprintf(stderr, "Client connected: %s; send timestamp %lld; bytes sent %d\n", inet_ntoa(echoclient.sin_addr), tssend, received);
				}
			/*} else {
				if (verbosity) {
					fprintf(stderr, "Client connected: %s; not replying\n", inet_ntoa(echoclient.sin_addr));
				}
			}*/
		} else if (verbosity) {
			fprintf(stdout, "Client connected: %s; send timestamp %lld; bytes sent %d; delay %lld\n", inet_ntoa(echoclient.sin_addr), tssend, received, delay_us);
			fflush(stdout);
		}
	}
}

