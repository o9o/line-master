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

#include "client.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <signal.h>

#include "util.h"
#include "messages.h"

#define BUFSIZE 40
#define SLEEP_BY_BUSYWAITING_INTERVAL_US 20000

static long long sequence = 0;
static long long packetsSent = 0;
static long long packetsReceived = 0;

static double timeStart = gettimestamp_us();
static double totalBytesSent = 0;
static double averageBitrate = 0;
static double averageSleep = 0;

void printStats()
{
	fprintf(stdout, "Average bitrate: %1.2f kbps\n", averageBitrate); fflush(stdout);
	fprintf(stdout, "Total bytes sent: %1.2f B\n", totalBytesSent); fflush(stdout);
	fprintf(stdout, "Packets sent: %lld\n", packetsSent); fflush(stdout);
	fprintf(stdout, "Packets received: %lld\n", packetsReceived); fflush(stdout);
	fprintf(stdout, "Time passed: %1.2f sec\n", (gettimestamp_us() - timeStart) * 1.0e-6); fflush(stdout);
	fprintf(stdout, "Average sleep %5.2f ms\n", averageSleep / sequence); fflush(stdout);
}

void handleSigIntClient(int )
{
	printStats();
	exit(0);
}

void client(in_addr_t server_address, int port, int probe_size, int probe_count, int interval_us, int dont_fragment,
		  int timeout_us, int burst, int burst_timeout_us, int source_only, int randomize_interval, int verbosity,
		  int busywaiting, in_addr_t local_address, int nobusywaiting)
{
	int sock;							                   /* The socket descriptor */
	struct sockaddr_in sockaddr_server;                    /* sockaddr_in for sending probes */
	struct sockaddr_in sockaddr_client;                    /* sockaddr_in for receiving probes */
	char *buffer;                                          /* Send/receive buffer */
	char buffer_static[BUFSIZE];
	fd_set readfds;

	(void) signal(SIGINT, handleSigIntClient);

	if (probe_size <= BUFSIZE) {
		buffer = buffer_static;
	} else {
		buffer = (char*) malloc(probe_size);
	}

	/* Create the UDP socket */
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Failed to create socket\n");
		exit(1);
	}

	// bind to a local address if specified
	if (local_address != INADDR_ANY) {
		struct sockaddr_in local_saddr;
		memset(&local_saddr, 0, sizeof(local_saddr));    /* Clear struct */
		local_saddr.sin_family = AF_INET;                    /* Internet/IPv4 */
		local_saddr.sin_addr.s_addr = local_address; /* IP address */
		sockaddr_server.sin_port = 0;                  /* any port */
		bind(sock, (struct sockaddr *)&local_saddr, sizeof(local_saddr));
	}

	// don't fragment
	if (dont_fragment) {
		int val = IP_PMTUDISC_DO;
		setsockopt(sock, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	} else {
		int val = IP_PMTUDISC_DONT;
		setsockopt(sock, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	}

//	int OneInt = 1;
//	setsockopt(sock, SOL_SOCKET, SO_NO_CHECK, &OneInt, sizeof(OneInt));

	/* Construct the server sockaddr_in structure */
	memset(&sockaddr_server, 0, sizeof(sockaddr_server));    /* Clear struct */
	sockaddr_server.sin_family = AF_INET;                    /* Internet/IPv4 */
	sockaddr_server.sin_addr.s_addr = server_address; /* IP address */
	sockaddr_server.sin_port = htons(port);                  /* server port */

	sequence = 0;
	packetsSent = 0;
	packetsReceived = 0;

	timeStart = gettimestamp_us();
	totalBytesSent = 0;
	averageBitrate = 0;
	averageSleep = 0;

	for (int i = 0; i < probe_count || probe_count == 0; i++) {
		averageBitrate = totalBytesSent * 8.0 / (gettimestamp_us() - timeStart) * 1.0e3;
		if (verbosity) {
			fprintf(stdout, "Round %d of %d\n", i+1, probe_count); fflush(stdout);
			printStats();
		}

		memcpy(buffer + OFFSET_C2S_SEQNO, &sequence, SIZE_C2S_SEQNO);

		long long tssend = gettimestamp_us();

		/* Send the timestamp to the server */
		memcpy(buffer + OFFSET_C2S_TSCLIENT, &tssend, SIZE_C2S_TSCLIENT);

		/* Send the datagram to the server */
		for (int ib = 0; ib < burst; ib++) {
			ssize_t sendcount;
			if ((sendcount = sendto(sock, buffer, probe_size, 0, (struct sockaddr *) &sockaddr_server, sizeof(sockaddr_server))) != probe_size) {
				fprintf(stderr, "Error: bytes sent: %lu; it should have been %d\n", sendcount, probe_size);
				exit(1);
			}
			totalBytesSent += 42 + probe_size;
			averageBitrate = totalBytesSent * 8.0 / (gettimestamp_us() - timeStart) * 1.0e3;
			packetsSent++;
			if (verbosity) fprintf(stdout, "Probe sent; bytes: %d\n", probe_size); fflush(stdout);
		}

		/* Receive the reply from the server */
		for (int ib = 0; ib < burst; ib++) {
			// wait until the socket has data ready to be read
			struct timeval tv;
			if (source_only) {
				tv.tv_sec = tv.tv_usec = 0;
			} else {
				if (ib == 0) {
					tv.tv_sec = timeout_us / 1000000LL;
					tv.tv_usec = timeout_us % 1000000LL;
				} else {
					tv.tv_sec = burst_timeout_us / 1000000LL;
					tv.tv_usec = burst_timeout_us % 1000000LL;
				}
			}

			FD_ZERO(&readfds);
			if (!source_only) {
				FD_SET(sock, &readfds);
			}
#if EXIT_ON_STDIN
			FD_SET(STDIN_FILENO, &readfds);
#endif

			int ready = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);
			if (ready == -1) {
				fprintf(stderr, "Error: select() %d %s\n", errno, strerror(errno));
				exit(1);
			}
			if (ready == 0) {
				if (!source_only) {
					if (verbosity) {
						fprintf(stdout, "Timeout\n"); fflush(stdout);
					}
				}
			}
#if EXIT_ON_STDIN
			else if (FD_ISSET(STDIN_FILENO, &readfds)) {
				printStats();
				exit(0);
			}
#endif
			else if (FD_ISSET(sock, &readfds)) {
				unsigned int recvlen = sizeof(sockaddr_client);
				int received;
				if ((received = recvfrom(sock, buffer, probe_size, 0, (struct sockaddr *) &sockaddr_client, &recvlen)) != probe_size) {
					fprintf(stderr, "Error: reply has %d bytes; it should have been %d\n", received, probe_size);
					exit(1);
				}

				long long tsrecv = gettimestamp_us();

				/* Check that client and server are using same socket */
				if (sockaddr_server.sin_addr.s_addr != sockaddr_client.sin_addr.s_addr) {
					fprintf(stderr, "Error: reply from incorrect address %s\n", inet_ntoa(sockaddr_client.sin_addr));
					exit(1);
				}

				long long sequence_echoed;
				memcpy(&sequence_echoed, buffer + OFFSET_S2C_SEQNO, SIZE_S2C_SEQNO);
				long long tssend_echoed;
				memcpy(&tssend_echoed, buffer + OFFSET_S2C_TSECHOED, SIZE_S2C_TSECHOED);
				if (sequence_echoed != sequence || tssend_echoed != tssend) {
					fprintf(stderr, "Error: corrupted packet or delayed reply (maybe the timeout is too small?)\n");
					fprintf(stderr, "Expected seqno: %lld received: %lld\n", sequence, sequence_echoed);
					exit(1);
				}

				long long tsserver;
				memcpy(&tsserver, buffer + OFFSET_S2C_TSSERVER, SIZE_S2C_TSSERVER);

				struct in_addr myaddr;
				memcpy(&myaddr.s_addr, buffer + OFFSET_S2C_CLIENTIP, SIZE_S2C_CLIENTIP);

				char addrmy[50];
				char addrother[50];
				strcpy(addrmy, inet_ntoa(myaddr));
				strcpy(addrother, inet_ntoa(sockaddr_client.sin_addr));
				fprintf(stdout, "Recv from %s reports my address %s; bytes received: %d\n", addrother, addrmy, received);

				print_timestamp_us(buffer, tssend);
				fprintf(stdout, "TS Sent: %s %lld\n", buffer, tssend);

				print_timestamp_us(buffer, tsserver);
				fprintf(stdout, "TS Serv: %s %lld\n", buffer, tsserver);

				print_timestamp_us(buffer, tsrecv);
				fprintf(stdout, "TS Recv: %s %lld\n", buffer, tsrecv);
				fprintf(stdout, "RTT: %7.3f ms\n", (tsrecv - tssend) * 1.0e-3);
				fprintf(stdout, "FORW delay: %7.3f ms\n", (tsserver - tssend) * 1.0e-3);
				fprintf(stdout, "BACK delay: %7.3f ms\n", (tsrecv - tsserver) * 1.0e-3);
				packetsReceived++;
			}
		}

		if (i < probe_count - 1 || probe_count == 0) {
			long long current_interval_us = interval_us;
			if (randomize_interval) {
				const float multiplier = 0.5; // add +- 50% of itself
				current_interval_us += (int) (interval_us * (rand() - RAND_MAX*0.5) / (RAND_MAX*0.5) * multiplier);
			}

			double tssleep = gettimestamp_us();
			current_interval_us -= gettimestamp_us() - tssend;

			int sleepcount = 0;

			if (!nobusywaiting && (current_interval_us <= SLEEP_BY_BUSYWAITING_INTERVAL_US || busywaiting)) {
				while (gettimestamp_us() - tssend < current_interval_us) {
					// busy waiting
					sleepcount++;
				}
			} else {
				// normal sleep
				// expect 10% (actually arbitrary) extra error here...
				// usleep(interval_us / 20);
				struct timespec sleepNano;
				sleepNano.tv_sec = (current_interval_us) / 1000000LL;
				sleepNano.tv_nsec = 1000 * ((current_interval_us) % 1000000LL);
				nanosleep(&sleepNano, NULL);
				sleepcount++;
			}

			double sleepduration = gettimestamp_us() - tssleep;
			averageSleep += sleepduration * 1.0e-3;

			if (verbosity) {
				fprintf(stdout, "Sleep lasted %5.2f ms (count %d)\n", sleepduration * 1.0e-3, sleepcount); fflush(stdout);
			}
		}

		if (verbosity) {
			fprintf(stdout, "\n"); fflush(stdout);
		}

		sequence++;
	}
	close(sock);

	if (buffer != buffer_static)
		free(buffer);

	printStats();
}
