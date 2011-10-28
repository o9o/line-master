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
#include <net/ethernet.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pfring.h>

#include <QtCore>
#include <QtXml>

#include "pscheduler.h"
#include "pconsumer.h"
#include "psender.h"
#include "qpairingheap.h"
#include "bitarray.h"
#include "../line-gui/netgraph.h"
#include "../util/util.h"
#include "../tomo/tomodata.h"

/// topology stuff

#define DEBUG_CALLS 0
#define DEBUG_FOREIGN_PACKETS 0
#define BUSY_WAITING 1

#define PACKET_EVENT_QUEUED  1
#define PACKET_EVENT_QDROP   2
#define PACKET_EVENT_RDROP   3

NetGraph *netGraph;

void NetGraphEdge::prepareEmulation()
{
	rate_Bps = 1000.0 * bandwidth;
	lossRate_int = (int) (RAND_MAX * lossBernoulli);
	qcapacity = queueLength * ETH_FRAME_LEN;

	qload = 0;
	qts_head = 0;

	packets_in = 0;
	bytes = 0;
	qdrops = 0;
	rdrops = 0;
	total_qdelay = 0;

	drops_mininterval = ULLONG_MAX;
	drops_lastts = ULLONG_MAX;
	drops_history = 0;
	drops_history_dcnt = 0;

	if (recordSampledTimeline) {
		edgeTimelineItem current;
		memset(&current, 0, sizeof(current));

		quint64 ts_now = get_current_time();
		current.timestamp = (ts_now / timelineSamplingPeriod) * timelineSamplingPeriod;
		timelineSampled << current;

		// preallocate for 60 seconds (but not more than some limit)
		quint64 estimateSize = 60 * (SEC_TO_NSEC / timelineSamplingPeriod);
		estimateSize = qMin(estimateSize, 100000ULL);
		// timelineSampled.reserve(estimateSize);
	}

	if (recordFullTimeline) {
		// preallocate for some number of packets
		quint64 estimateSize = 10000;
		estimateSize = qMin(estimateSize, 100000ULL);
		// timelineFull.reserve(estimateSize);
	}
}

void NetGraphPath::prepareEmulation()
{
	packets_in = 0;
	packets_out = 0;
	bytes_in = 0;
	bytes_out = 0;
	total_theor_delay = 0;
	total_actual_delay = 0;
}

void NetGraph::prepareEmulation()
{
	edgeCache.clear();
	for (int i = 0; i < edges.count(); i++) {
		edges[i].prepareEmulation();
		edgeCache.insert(QPair<qint32,qint32>(edges[i].source, edges[i].dest), i);
	}

	pathCache.clear();
	for (int i = 0; i < paths.count(); i++) {
		paths[i].prepareEmulation();
		pathCache.insert(QPair<qint32,qint32>(paths[i].source, paths[i].dest), i);
	}
}

void loadTopology(QString graphFileName)
{
	netGraph = new NetGraph();
	netGraph->setFileName(graphFileName);
	netGraph->loadFromFile();
	netGraph->prepareEmulation();
}

/**
 * Enqueue a packet on a link.
 *
 * @p : the packet to be enqueued
 * @ts_now : the current time in ns
 * @ts_exit : on successful enqueuing, the time at which the packet ends transmission
 *
 * Returns true if the packet was enqueued, false if it was dropped.
*/
#define DECISION_QUEUE    0
#define DECISION_QDROP    1
#define DECISION_RDROP    2
bool NetGraphEdge::enqueue(Packet *p, quint64 ts_now, quint64 &ts_exit)
{
	int decision = DECISION_QUEUE;
	quint64 qdelay;
	int randomVal;

	// update the link ingress stats
	packets_in++;
	bytes += p->length;

	if (ts_now < qts_head)
		ts_now = qts_head;

	// update the queue
	if (qload > 0) {
		quint64 delta_t = ts_now - qts_head;
		// how many bytes were transmitted during delta_t
		quint64 delta_B = (delta_t * rate_Bps) / SEC_TO_NSEC;
		delta_B = qMin(delta_B, qload);
		qload -= delta_B;
	}

	//printf("%s: ts delta = +"TS_FORMAT" Edge %d: qload = %llu (%llu%%)\n", p->edgecount == 0 ? "ARRIVAL" : "EVENT  ", TS_FORMAT_PARAM(ts_now - qts_head), id, qload, (qload*100)/qcapacity);

	qts_head = ts_now;

	// queue drop?
	if (qcapacity - qload < (quint64) p->length) {
		qdrops++;
		if (DEBUG_PACKETS) printf("Edge: Drop: %d.%d.%d.%d -> %d.%d.%d.%d: plen = %d, qload = %llu, qcap = %llu\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip), p->length, qload, qcapacity);
		decision = DECISION_QDROP;
		goto stats;
	}
	// random drop?
	randomVal = rand();
	if (lossRate_int > 0 && randomVal < lossRate_int) {
		rdrops++;
		if (DEBUG_PACKETS) printf("Edge: Drop: %d.%d.%d.%d -> %d.%d.%d.%d: lossRate_int = %d, randomVal = %d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip), lossRate_int, randomVal);
		decision = DECISION_RDROP;
		goto stats;
	}
	// we are enqueuing this packet
	qload += p->length;

	// add transmission delay
	qdelay = (qload * SEC_TO_NSEC) / rate_Bps;
	ts_exit = qts_head + qdelay;
	total_qdelay += qdelay;

	// add propagation delay
	ts_exit += delay_ms * MSEC_TO_NSEC;

	p->theoretical_delay += ts_exit - ts_now;

stats:
	// update stats
	if (recordSampledTimeline) {
		if (ts_now >= timelineSampled.last().timestamp + timelineSamplingPeriod) {
			// new time bracket, insert new aggregate
			edgeTimelineItem current;
			memset(&current, 0, sizeof(current));

			current.timestamp = (ts_now / timelineSamplingPeriod) * timelineSamplingPeriod;
			current.queue_sampled = qload;

			timelineSampled << current;
		}
		// we're in the same time bracket, update the last item
		timelineSampled.last().arrivals_p++;
		timelineSampled.last().arrivals_B += p->length;
		if (decision == DECISION_QDROP) {
			timelineSampled.last().qdrops_p++;
			timelineSampled.last().qdrops_B += p->length;
		}
		if (decision == DECISION_RDROP) {
			timelineSampled.last().rdrops_p++;
			timelineSampled.last().rdrops_B += p->length;
		}
		timelineSampled.last().queue_avg += qload;
		timelineSampled.last().queue_max = qMax(timelineSampled.last().queue_max, qload);
	}

	if (recordFullTimeline) {
		packetEvent current;
		current.timestamp = ts_now;
		current.type = (decision == DECISION_QUEUE) ? PACKET_EVENT_QUEUED : (decision == DECISION_QDROP) ? PACKET_EVENT_QDROP : PACKET_EVENT_RDROP;
		timelineFull << current;
	}

	if (decision == DECISION_QDROP || decision == DECISION_RDROP) {
		if (drops_lastts < ULLONG_MAX) {
			drops_mininterval = qMin(drops_mininterval, ts_now - drops_lastts);
		}
		drops_lastts = ts_now;
	}

	if (decision == DECISION_QDROP || decision == DECISION_RDROP) {
		drops_history = (drops_history << 1) | 1ULL;
		if (recordSampledTimeline) {
			packetEvents << 1;
		}
	} else {
		if (drops_history & (1ULL << 63))
			drops_history_dcnt--;
		drops_history = (drops_history << 1);
		if (recordSampledTimeline) {
			packetEvents << 0;
		}
	}

	// success
	return (decision == DECISION_QUEUE);
}

#define PKT_QUEUED    0
#define PKT_DROPPED   1
#define PKT_FORWARDED 2
int routePacket(Packet *p, quint64 ts_now, quint64 &ts_next)
{
	NetGraphPath &path = netGraph->pathByNodeIndex(p->src_id, p->dst_id);

	// is this a new packet?
	if (p->trace.isEmpty()) {
		// yes, update path ingress stats
		p->trace << p->src_id;
		if (DEBUG_PACKETS) printf("New packet %d.%d.%d.%d -> %d.%d.%d.%d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip));

		path.packets_in++;
		path.bytes_in += p->length;

		if (path.recordSampledTimeline) {
			if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
				pathTimelineItem current;
				memset(&current, 0, sizeof(current));
				current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
				path.timelineSampled << current;
			}
			path.timelineSampled.last().arrivals_p++;
			path.timelineSampled.last().arrivals_B += p->length;
		}
	} // did it reach the destination?

	if (p->trace.last() == p->dst_id) {
		// yes, forward the packet
		p->ts_start_send = ts_now;
		if (DEBUG_PACKETS) printf("Forwarding packet %d.%d.%d.%d -> %d.%d.%d.%d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip));

		// update path egress stats
		path.packets_out++;
		path.bytes_out += p->length;
		path.total_theor_delay += p->theoretical_delay;
		path.total_actual_delay += p->ts_start_send - p->ts_driver_rx;
		if (path.recordSampledTimeline) {
			if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
				pathTimelineItem current;
				memset(&current, 0, sizeof(current));
				current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
				current.delay_min = ULLONG_MAX;
				path.timelineSampled << current;
			}
			path.timelineSampled.last().exits_p++;
			path.timelineSampled.last().exits_B += p->length;
			path.timelineSampled.last().delay_total += p->theoretical_delay;
			path.timelineSampled.last().delay_max = qMax(path.timelineSampled.last().delay_max, p->theoretical_delay);
			path.timelineSampled.last().delay_min = qMin(path.timelineSampled.last().delay_min, p->theoretical_delay);
		}

		packetsOut.enqueue(p);
		return PKT_FORWARDED;
	}

	// we need to forward it, find the route
	Route r = netGraph->nodes[p->trace.last()].routes.routes.value(p->dst_id, Route(-1, -1));
	if (r.destination < 0) {
		// no route, update path stats
		if (DEBUG_PACKETS) printf("No route for packet %d.%d.%d.%d -> %d.%d.%d.%d, node=%d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip), p->trace.last());
		if (path.recordSampledTimeline) {
			if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
				pathTimelineItem current;
				memset(&current, 0, sizeof(current));
				current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
				path.timelineSampled << current;
			}
			path.timelineSampled.last().drops_p++;
			path.timelineSampled.last().drops_B += p->length;
		}
		return PKT_DROPPED;
	} else {
		NetGraphEdge e = netGraph->edgeByNodeIndex(p->trace.last(), r.nextHop);
		if (DEBUG_PACKETS) printf("Found route for packet %d.%d.%d.%d -> %d.%d.%d.%d, node=%d, next hop=%d, edge = %d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip), p->trace.last(), r.nextHop, e.index);
		p->trace << r.nextHop;
		if (netGraph->edges[e.index].enqueue(p, ts_now, ts_next)) {
			return PKT_QUEUED;
		} else {
			// packet dropped, update path stats
			if (path.recordSampledTimeline) {
				if (ts_now >= path.timelineSampled.last().timestamp + path.timelineSamplingPeriod) {
					pathTimelineItem current;
					memset(&current, 0, sizeof(current));
					current.timestamp = (ts_now / path.timelineSamplingPeriod) * path.timelineSamplingPeriod;
					path.timelineSampled << current;
				}
				path.timelineSampled.last().drops_p++;
				path.timelineSampled.last().drops_B += p->length;
			}
			return PKT_DROPPED;
		}
	}
}

QString timeToString(quint64 value)
{
	if (value == 0)
		return "0ns";
	if (value == ULONG_LONG_MAX)
		return "Infinity";

	quint64 s, ms, us, ns;

	s = value / 1000ULL / 1000ULL / 1000ULL;
	ms = (value / 1000ULL / 1000ULL) % 1000ULL;
	us = (value / 1000ULL) % 1000ULL;
	ns = value % 1000ULL;

	if (ns) {
		return QString::number(value) + "ns";
	} else if (us) {
		return QString::number(value / 1000ULL) + "us";
	} else if (ms) {
		return QString::number(value / 1000ULL / 1000ULL) + "ms";
	} else {
		return QString::number(value / 1000ULL / 1000ULL / 1000ULL) + "s";
	}
}

void saveEdgeTimelinesBinary(NetGraphEdge e, quint64 tsMin, quint64 tsMax)
{
	// now create arrays for everything
	QVector<quint64> vector_timestamp;
	QVector<quint64> vector_arrivals_p;
	QVector<quint64> vector_arrivals_B;
	QVector<quint64> vector_qdrops_p;
	QVector<quint64> vector_qdrops_B;
	QVector<quint64> vector_rdrops_p;
	QVector<quint64> vector_rdrops_B;
	QVector<quint64> vector_queue_sampled;
	QVector<quint64> vector_queue_avg;
	QVector<quint64> vector_queue_max;

	{
		QFile file(QString("timelines-edge-%1.dat").arg(e.index));
		file.open(QIODevice::WriteOnly);
		QDataStream out(&file);
		out.setVersion(QDataStream::Qt_4_0);

		// edge timeline range
		out << tsMin;
		out << tsMax;

		// edge properties
		out << e.timelineSamplingPeriod;
		out << e.rate_Bps;
		out << e.delay_ms;
		out << e.qcapacity;

		// unroll RLE data
		quint64 lastTs = 0;
		quint64 samplingPeriod = e.timelineSamplingPeriod;
		quint64 lastQueueSampled = 0;
		quint64 lastQueueAvg = 0;
		quint64 lastQueueMax = 0;

		foreach (edgeTimelineItem item, e.timelineSampled) {
			// "extrapolate"
			while (item.timestamp > tsMin + lastTs + samplingPeriod) {
				quint64 delta = ((e.rate_Bps * samplingPeriod) / SEC_TO_NSEC);
				lastQueueSampled = (delta < lastQueueSampled) ? lastQueueSampled-delta : 0;
				lastQueueAvg = (delta < lastQueueAvg) ? lastQueueAvg-delta : 0;
				lastQueueMax = (delta < lastQueueMax) ? lastQueueMax-delta : 0;

				lastTs += samplingPeriod;
				vector_timestamp << lastTs;
				vector_arrivals_p << 0;
				vector_arrivals_B << 0;
				vector_qdrops_p << 0;
				vector_qdrops_B << 0;
				vector_rdrops_p << 0;
				vector_rdrops_B << 0;
				vector_queue_sampled << lastQueueSampled;
				vector_queue_avg << lastQueueAvg;
				vector_queue_max << lastQueueMax;
			}

			lastTs = item.timestamp - tsMin;
			lastQueueSampled = item.queue_sampled;
			lastQueueAvg = item.arrivals_p ? item.queue_avg / item.arrivals_p : 0;
			lastQueueMax = item.queue_max;

			vector_timestamp << lastTs;
			vector_arrivals_p << item.arrivals_p;
			vector_arrivals_B << item.arrivals_B;
			vector_qdrops_p << item.qdrops_p;
			vector_qdrops_B << item.qdrops_B;
			vector_rdrops_p << item.rdrops_p;
			vector_rdrops_B << item.rdrops_B;
			vector_queue_sampled << lastQueueSampled;
			vector_queue_avg << lastQueueAvg;
			vector_queue_max << lastQueueMax;
		}

		// write the data
		out << vector_timestamp;
		out << vector_arrivals_p;
		out << vector_arrivals_B;
		out << vector_qdrops_p;
		out << vector_qdrops_B;
		out << vector_rdrops_p;
		out << vector_rdrops_B;
		out << vector_queue_sampled;
		out << vector_queue_avg;
		out << vector_queue_max;
	}

	{
		QFile file(QString("packetevents-edge-%1.dat").arg(e.index));
		file.open(QIODevice::WriteOnly);
		QDataStream out(&file);
		out.setVersion(QDataStream::Qt_4_0);

		out << e.packetEvents.toVector();
	}
}

void saveRecordedData()
{
	saveFile("simulation.txt", QString("graph=%1").arg(netGraph->fileName.replace(".graph", "").split('/', QString::SkipEmptyParts).last()));

	foreach (NetGraphEdge e, netGraph->edges) {
		qDebug() << "Edge" << e.index << "minimum interval between drops =" << timeToString(e.drops_mininterval);
	}

	TomoData tomoData;

	tomoData.m = netGraph->paths.count();
	tomoData.n = netGraph->edges.count();

	// path data: pathCount items
	foreach (NetGraphPath p, netGraph->paths) {
		// path starts: src host index
		tomoData.pathstarts << p.source;
		// path ends: dest host index
		tomoData.pathends << p.dest;
		// the edge list for each path (m items of the form path(index).edges = [id1 ... idx]; these are indices, not real edge ids!
		QList<qint32> edgelist;
		foreach (NetGraphEdge e, p.edgeList) {
			edgelist << e.index;
		}
		tomoData.pathedges << edgelist;
	}

	// compute the routing matrix, m x n of quint8 with values of either 0 or 1
	foreach (NetGraphPath p, netGraph->paths) {
		QVector<quint8> line;
		foreach (NetGraphEdge e, netGraph->edges) {
			if (p.edgeSet.contains(e)) {
				line << 1;
			} else {
				line << 0;
			}
		}
		tomoData.A << line;
	}

	// write the path transmission rate vector: m x 1 of [0..1]
	foreach (NetGraphPath p, netGraph->paths) {
		if (p.packets_in == 0) {
			tomoData.y << 1.0;
		} else {
			tomoData.y << p.packets_out / (qreal)(p.packets_in);
		}
	}

	// write the measured edge transmission rate vector: 1 x n of [0..1]
	foreach (NetGraphEdge e, netGraph->edges) {
		if (e.packets_in == 0) {
			tomoData.xmeasured << 1.0;
		} else {
			tomoData.xmeasured << (e.packets_in - e.qdrops - e.rdrops) / (qreal)(e.packets_in);
		}
	}

	// first compute the time range
	tomoData.tsMin = ULLONG_MAX;
	tomoData.tsMax = 0;

	foreach (NetGraphEdge e, netGraph->edges) {
		if (e.recordFullTimeline && !e.timelineFull.isEmpty()) {
			tomoData.tsMin = qMin(tomoData.tsMin, e.timelineFull.first().timestamp);
			tomoData.tsMax = qMax(tomoData.tsMax, e.timelineFull.last().timestamp);
		}
		if (e.recordSampledTimeline && !e.timelineSampled.isEmpty()) {
			tomoData.tsMin = qMin(tomoData.tsMin, e.timelineSampled.first().timestamp);
			tomoData.tsMax = qMax(tomoData.tsMax, e.timelineSampled.last().timestamp);
		}
	}
	foreach (NetGraphPath p, netGraph->paths) {
		if (p.recordSampledTimeline && !p.timelineSampled.isEmpty()) {
			tomoData.tsMin = qMin(tomoData.tsMin, p.timelineSampled.first().timestamp);
			tomoData.tsMax = qMax(tomoData.tsMax, p.timelineSampled.last().timestamp);
		}
	}

	tomoData.save("tomo-records.dat");

	// edge timeline aggregates/samples
	foreach (NetGraphEdge e, netGraph->edges) {
		saveEdgeTimelinesBinary(e, tomoData.tsMin, tomoData.tsMax);
	}
}

void* packet_scheduler_thread(void* )
{
	u_int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
	u_long core_id = CORE_SCHEDULER % numCPU;

	if (numCPU > 1) {
		if (bind2core(core_id) == 0) {
			printf("Set thread scheduler affinity to core %lu/%u\n", core_id, numCPU);
		} else {
			printf("Failed to set thread scheduler affinity to core %lu/%u\n", core_id, numCPU);
		}
	}

	QPairingHeap<Packet*> eventQueue;

	quint64 max_loop_delay = 0;
	quint64 total_loop_delay = 0;
	quint64 total_loops = 0;
	quint64 packetsQdropped = 0;
	while (1) {
		if (do_shutdown) {
			printf("Scheduler loop took: avg "TS_FORMAT", max "TS_FORMAT"\n", TS_FORMAT_PARAM(total_loop_delay / total_loops), TS_FORMAT_PARAM(max_loop_delay));
			break;
		}

		// process new packets
		QLinkedList<Packet*> newPackets = packetsIn.dequeueAll();
		quint64 ts_now = get_current_time();

		bool receivedPackets = !newPackets.isEmpty();
		while (!newPackets.isEmpty()) {
			// new packet arrived
			Packet *p = newPackets.takeFirst();
			p->ts_start_proc = ts_now;
			if (p->src_id < 0 || p->src_id >= netGraph->nodes.count() ||
				p->dst_id < 0 || p->dst_id >= netGraph->nodes.count()) {
				// foreign packet
				if (DEBUG_PACKETS) printf("Bad packet %d.%d.%d.%d -> %d.%d.%d.%d (src = %d, dst = %d)\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip), p->src_id, p->dst_id);
				delete p;
				continue;
			}
			quint64 ts_next_event;
			int pkt_state = routePacket(p, p->ts_driver_rx, ts_next_event);
			if (pkt_state == PKT_QUEUED) {
				if (DEBUG_PACKETS) printf("Enqueue: %d.%d.%d.%d -> %d.%d.%d.%d, for time = +%llu ns, edgecount = %d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip), ts_next_event - ts_now, p->edgecount);
				eventQueue.insert(p, ts_next_event);
			} else if (pkt_state == PKT_DROPPED) {
				if (DEBUG_PACKETS) printf("Drop: %d.%d.%d.%d -> %d.%d.%d.%d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip));
				packetsQdropped++;
				delete p;
			}
		}

		// process events
		bool receivedEvents = false;
		quint64 ts_next_queued_event = ULLONG_MAX;
		while (!eventQueue.isEmpty()) {
			QPair<Packet*, quint64> event = eventQueue.findMin();
			if (event.second <= ts_now) {
				eventQueue.deleteMin();
				receivedEvents = true;

				Packet *p = event.first;
				// quint64 ts_event = event.second;

				quint64 ts_next_event;
				int pkt_state = routePacket(p, event.second, ts_next_event);
				if (pkt_state == PKT_QUEUED) {
					if (DEBUG_PACKETS) printf("Enqueue: %d.%d.%d.%d -> %d.%d.%d.%d, for time = +%llu ns, edgecount = %d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip), ts_next_event - ts_now, p->edgecount);
					eventQueue.insert(p, ts_next_event);
				} else if (pkt_state == PKT_DROPPED) {
					if (DEBUG_PACKETS) printf("Drop: %d.%d.%d.%d -> %d.%d.%d.%d\n", NIPQUAD(p->src_ip), NIPQUAD(p->dst_ip));
					packetsQdropped++;
					delete p;
				}
			} else {
				ts_next_queued_event = event.second;
				break;
			}
		}
		// begin stats
		quint64 ts_after = get_current_time();
		max_loop_delay = qMax(max_loop_delay, ts_after - ts_now);
		total_loop_delay += ts_after - ts_now;
		total_loops++;
		// end stats

#if BUSY_WAITING
		if (!receivedPackets && !receivedEvents && ts_next_queued_event > ts_after + 10 * MSEC_TO_NSEC)
			sched_yield();
#endif

		// qDebug() << "Loop took < " << max_loop_delay << "ns";
	}

	printf("Total packets qdropped: %llu\n", packetsQdropped);

	saveRecordedData();

	return(NULL);
}
