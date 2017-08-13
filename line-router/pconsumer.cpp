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

#include "pconsumer.h"

#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/poll.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <net/ethernet.h>     /* the L2 protocols */
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pfring.h>

#include <QtCore>

#include "../line-gui/netgraphnode.h"

#define PROFILE_PCONSUMER 0

SpinlockedQueue<Packet*> packetsIn;

quint64 get_current_time()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ((quint64)ts.tv_sec) * 1000ULL * 1000ULL * 1000ULL + ((quint64)ts.tv_nsec);
}

void* packet_consumer_thread(void* ) {
	u_int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
	Packet *p;

	u_long core_id = CORE_CONSUMER % numCPU;
	struct pfring_pkthdr hdr;

	if (numCPU > 1) {
		if (bind2core(core_id) == 0) {
			printf("Set thread consumer affinity to core %lu/%u\n", core_id, numCPU);
		} else {
			printf("Failed to set thread consumer affinity to core %lu/%u\n", core_id, numCPU);
		}
	}

	quint64 packetsReceived = 0;

	memset(&hdr, 0, sizeof(hdr));
	p = new Packet();

#if PROFILE_PCONSUMER
	quint64 ts_prev = 0;
#endif

	while (1) {
		if (do_shutdown)
			break;

        quint8 *buffer = p->buffer;
        quint8 **buffer_ptr = &buffer;

        if (pfring_recv(pd, buffer_ptr, sizeof(p->buffer), &hdr, 0) > 0) {
			if (do_shutdown)
				break;
			if (hdr.caplen != hdr.len) {
				qDebug() << "hdr.caplen != hdr.len:" << hdr.caplen << hdr.len;
				continue;
			}
			if (hdr.len > 1514) {
				qDebug() << "hdr.len =" << hdr.len;
				continue;
			}
			if (hdr.extended_hdr.parsed_pkt.ip_version == 4) {
				if (((htonl(hdr.extended_hdr.parsed_pkt.ip_src.v4) & MODEL_MASK) == MODEL_SUBNET) &&
				    ((htonl(hdr.extended_hdr.parsed_pkt.ip_dst.v4) & MODEL_MASK) == MODEL_SUBNET) &&
				    (htonl(hdr.extended_hdr.parsed_pkt.ip_dst.v4) & MODEL_FORCEBIT) &&
				    !(htonl(hdr.extended_hdr.parsed_pkt.ip_src.v4) & MODEL_FORCEBIT)) {
					if (DEBUG_PACKETS) printf("Accepted packet %d.%d.%d.%d -> %d.%d.%d.%d\n", HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_src.v4), HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_dst.v4));
					packetsReceived++;
					quint64 ts_now = get_current_time();
#if PROFILE_PCONSUMER
					printf("sw ts delta = +"TS_FORMAT"\n", TS_FORMAT_PARAM(ts_now-ts_prev));
					ts_prev = ts_now;
					//printf("hw ts = "TS_FORMAT"\n", TS_FORMAT_PARAM((quint64)hdr.extended_hdr.timestamp_ns));
					//printf("sw ts = "TS_FORMAT"\n", TS_FORMAT_PARAM(ts_now));
#endif
					p->ts_driver_rx = hdr.extended_hdr.timestamp_ns ? hdr.extended_hdr.timestamp_ns : ts_now;
					p->ts_userspace_rx = ts_now;
					p->src_ip = htonl(hdr.extended_hdr.parsed_pkt.ip_src.v4);
					p->dst_ip = htonl(hdr.extended_hdr.parsed_pkt.ip_dst.v4);
					p->src_id = (ntohl(p->src_ip) & MODEL_HOSTMASK) - IP_OFFSET;
					p->dst_id = (ntohl(p->dst_ip) & MODEL_HOSTMASK) - IP_OFFSET;
					p->l4_protocol = hdr.extended_hdr.parsed_pkt.l3_proto; // they named it worng
					p->offsets = hdr.extended_hdr.parsed_pkt.offset;
					p->length = hdr.len;
					packetsIn.enqueue(p);
					p = new Packet();
				} else {
					if (DEBUG_PACKETS) printf("Dropped packet %d.%d.%d.%d -> %d.%d.%d.%d\n", HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_src.v4), HIPQUAD(hdr.extended_hdr.parsed_pkt.ip_dst.v4));
				}
			}
		} else {
//			if (wait_for_packet == 0)
//				sched_yield();
		}
	}

	printf("Total packets received: %llu\n", packetsReceived);

	return(NULL);
}
