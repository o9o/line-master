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

#ifndef PFILTER_H
#define PFILTER_H

#include <pfring.h>
#include <QtCore>
#include "spinlockedqueue.h"

// masks from libipaddr.so
#define MODEL_SUBNET   htonl(0x0a000000)  /* 10.0.0.0/8 */
#define MODEL_MASK     htonl(0xff000000)  /* 10.0.0.0/8 */
#define MODEL_FORCEBIT htonl(0x00800000)  /* 0000 0000 . 1000 0000 . 0000 0000 . 0000 0000 which gives 10.128.0.0/9 */
#define MODEL_HOSTMASK 0x7FFFFF           /* 0000 0000 . 0111 1111 . 1111 1111 . 1111 1111 */

class Packet {
public:
	Packet() {
		theoretical_delay = 0;
		edgecount = 0;
	}

	quint8 buffer[2048];
	// all timestamps are in nanosec
	quint64 ts_driver_rx;
	quint64 ts_userspace_rx;
	quint64 ts_start_proc;
	quint64 ts_end_proc;
	quint64 ts_start_send;
	quint64 ts_send;
	quint64 theoretical_delay; // ideally = ts_end_proc - ts_start_proc

	int length; // frame length
	struct pkt_offset offsets;
	in_addr_t src_ip;
	in_addr_t dst_ip;
	quint8 l4_protocol;
	QList<qint32> trace; // list of edge IDs
	int edgecount;
	qint32 src_id; // ID of source NetGraphNode
	qint32 dst_id; // ID of destination NetGraphNode
};

extern pfring *pd;
extern quint8 wait_for_packet; // 1 = blocking read, 0 = busy waiting
extern quint8 dna_mode;
extern quint8 do_shutdown;

extern QString simulationId;

// CPU affinity
#define CORE_CONSUMER 0

#define SEC_TO_NSEC  1000000000
#define MSEC_TO_NSEC 1000000
#define USEC_TO_NSEC 1000

#define TS_FORMAT "%llu s %llu ms %llu us %llu ns"
#define TS_FORMAT_PARAM(X) ((X)/SEC_TO_NSEC), ((X)/MSEC_TO_NSEC)%1000, ((X)/USEC_TO_NSEC)%1000, (X)%1000

#define DEBUG_PACKETS 0

int runPacketFilter(int argc, char **argv);

void* packet_consumer_thread(void* );
void* packet_scheduler_thread(void* );

int bind2core(u_int core_id);

quint64 get_current_time();

void loadTopology(QString graphFileName);

extern SpinlockedQueue<Packet*> packetsIn;

// Display an IP address in readable format.
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]

#if defined(__LITTLE_ENDIAN)
#define HIPQUAD(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]
#elif defined(__BIG_ENDIAN)
#define HIPQUAD NIPQUAD
#else
#error "Please fix endian.h"
#endif /* __LITTLE_ENDIAN */

#endif // PFILTER_H
