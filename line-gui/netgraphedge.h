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

#ifndef NETGRAPHEDGE_H
#define NETGRAPHEDGE_H

#include <QtCore>

#define ETH_DATA_LEN	    1500
#define ETH_FRAME_LEN	1514 // max. bytes in frame without FCS

#ifdef LINE_EMULATOR
#include "../line-router/bitarray.h"
#endif

#ifdef LINE_EMULATOR

class Packet;

struct packetEvent {
	quint64      timestamp;
	int          type;
};

struct edgeTimelineItem {
	quint64      timestamp;
	quint64      arrivals_p;    // number of packet arrivals
	quint64      arrivals_B;    // ingress bytes
	quint64      qdrops_p;      // number of packets dropped by queueing
	quint64      qdrops_B;      // bytes dropped by queueing
	quint64      rdrops_p;      // number of packets dropped randomly
	quint64      rdrops_B;      // bytes dropped randomly
	quint64      queue_sampled; // queue utilization sampled at timestamp
	quint64      queue_avg;     // divide this by arrivals_p (if that is zero, this is zero) to get the average queue size, sampled at packet arrivals
	quint64      queue_max;     // the maximum queue size over this time interval
};
#endif

class NetGraphEdge
{
public:
	NetGraphEdge();

	qint32 index;            // edge index in graph (0..e-1)
	qint32 source;           // index of the source node
	qint32 dest;             // index of the destination node

	qint32 delay_ms;         // propagation delay in ms
	qreal lossBernoulli;     // bernoulli loss rate (before queueing)
	qint32 queueLength;      // queue length in #Ethernet frames
	qreal bandwidth;         // bandwidth in B/s

	bool used;

	bool recordSampledTimeline;
	quint64 timelineSamplingPeriod; // nanoseconds
	// TODO not supported by GUI
	bool recordFullTimeline;

#ifdef LINE_EMULATOR
	quint64 rate_Bps;        // link rate in bytes/s
	qint32 lossRate_int;     // packet loss rate (2^31-1 means 100% loss)

	// Queue
	quint64 qcapacity;     // queue size in bytes
	quint64 qload;         // how many bytes are used at time == qts_head
	quint64 qts_head;      // the timestamp at which the first byte begins transmitting

	// Statistics
	quint64 packets_in;   // Total number of packets that arrived on this link
	quint64 bytes;        // Total number of bytes that arrived on this link
	quint64 qdrops;       // Total number of packets dropped because of queuing
	quint64 rdrops;       // Total number of packets dropped randomly
	quint64 total_qdelay; // Total queueing delay

	quint64 drops_mininterval;  // Minimum time interval between 2 consecutive drops
	quint64 drops_lastts;       // Last timestamp of a drop
	quint64 drops_history;      // History of the last 64 incoming packets (0 = transmitted, 1 = qdropped
	quint64 drops_history_dcnt; // Number of 1 bits in drops_history

	// Timeline
	QList<packetEvent> timelineFull;
	QList<edgeTimelineItem> timelineSampled;

	// Packet events
	BitArray packetEvents;      // 0 = successful forwarding; 1 = drop
#endif

	QString tooltip();    // shows bw, delay etc
	double metric();

#ifdef LINE_EMULATOR
	void prepareEmulation();
	bool enqueue(Packet *p, quint64 ts_now, quint64 &ts_exit);
#endif

	bool operator==(const NetGraphEdge &other) const {
		return this->index == other.index;
	}
};

inline uint qHash(const NetGraphEdge &e) {return qHash(e.index); }

QDataStream& operator>>(QDataStream& s, NetGraphEdge& e);

QDataStream& operator<<(QDataStream& s, const NetGraphEdge& e);

#endif // NETGRAPHEDGE_H
