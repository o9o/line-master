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

#include "netgraph.h"

#include <QtXml>
#include "util.h"

#ifndef LINE_EMULATOR
#include "bgp.h"
#include "gephilayout.h"
#include "convexhull.h"
#endif

NetGraph::NetGraph()
{
}

void NetGraph::clear()
{
	nodes.clear();
	edges.clear();
	connections.clear();
	paths.clear();
	fileName.clear();
}

int NetGraph::addNode(int type, QPointF pos, int ASNumber)
{
	NetGraphNode node;
	node.index = nodes.count();
	node.x = pos.x();
	node.y = pos.y();
	node.nodeType = type;
	node.ASNumber = ASNumber;
	nodes.append(node);
	return node.index;
}

int NetGraph::addEdge(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength)
{
	if (!canAddEdge(nodeStart, nodeEnd)) {
		qDebug() << __FILE__ << __LINE__ << "illegal operation";
		exit(1);
	}
	NetGraphEdge edge;
	edge.index = edges.count();
	edge.source = nodeStart;
	edge.dest = nodeEnd;
	edge.bandwidth = bandwidth;
	edge.delay_ms = delay;
	edge.lossBernoulli = loss;
	edge.queueLength = queueLength;
	edges.append(edge);
	return edge.index;
}

int NetGraph::addEdgeSym(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength)
{
	addEdge(nodeStart, nodeEnd, bandwidth, delay, loss, queueLength);
	return addEdge(nodeEnd, nodeStart, bandwidth, delay, loss, queueLength);
}

bool NetGraph::canAddEdge(int nodeStart, int nodeEnd)
{
	// no edge to self
	if (nodeStart == nodeEnd)
		return false;
	// no duplicates
	foreach (NetGraphEdge edge, edges) {
		if (edge.source == nodeStart && edge.dest == nodeEnd)
			return false;
	}
	// no edges between hosts
	if (nodes[nodeStart].nodeType == NETGRAPH_NODE_HOST &&
	    nodes[nodeEnd].nodeType == NETGRAPH_NODE_HOST)
		return false;
	// no edges between hosts and routers
	if (nodes[nodeStart].nodeType == NETGRAPH_NODE_HOST &&
	    nodes[nodeEnd].nodeType == NETGRAPH_NODE_ROUTER)
		return false;
	if (nodes[nodeEnd].nodeType == NETGRAPH_NODE_HOST &&
	    nodes[nodeStart].nodeType == NETGRAPH_NODE_ROUTER)
		return false;
	// no edges between hosts and border routers
	if (nodes[nodeStart].nodeType == NETGRAPH_NODE_HOST &&
	    nodes[nodeEnd].nodeType == NETGRAPH_NODE_BORDER)
		return false;
	if (nodes[nodeEnd].nodeType == NETGRAPH_NODE_HOST &&
	    nodes[nodeStart].nodeType == NETGRAPH_NODE_BORDER)
		return false;
	// if edge is inter-AS, then the nodes must be border routers
	if (nodes[nodeStart].ASNumber != nodes[nodeEnd].ASNumber) {
		if (nodes[nodeStart].nodeType != NETGRAPH_NODE_BORDER ||
		    nodes[nodeEnd].nodeType != NETGRAPH_NODE_BORDER)
			return false;
	}
	return true;
}

void NetGraph::deleteNode(int index)
{
	nodes.removeAt(index);
	for (int i = 0; i < nodes.length(); i++) {
		nodes[i].index = i;
	}

	for (int iEdge = 0; iEdge < edges.length(); iEdge++) {
		if (edges[iEdge].source == index || edges[iEdge].dest == index) {
			deleteEdge(iEdge);
			iEdge--;
			continue;
		}

		if (edges[iEdge].source > index)
			edges[iEdge].source--;
		if (edges[iEdge].dest > index)
			edges[iEdge].dest--;
	}
}

void NetGraph::deleteEdge(int index)
{
	edges.removeAt(index);
	for (int i = 0; i < edges.length(); i++) {
		edges[i].index = i;
	}
}

void NetGraph::deleteConnection(int index)
{
	connections.removeAt(index);
	for (int i = 0; i < connections.length(); i++) {
		connections[i].index = i;
	}
}


int NetGraph::addConnection(NetGraphConnection c)
{
	c.index = connections.count();
	connections << c;
	updateUsed();
	return c.index;
}

#ifndef LINE_EMULATOR
void NetGraph::computeRoutes()
{
	Bgp::computeRoutes(*this);
}
#endif

void NetGraph::setFileName(QString newFileName)
{
	this->fileName = newFileName;
}

QDataStream& operator>>(QDataStream& s, NetGraph& n)
{
	s >> n.nodes;
	s >> n.edges;
	s >> n.connections;
	s >> n.domains;
	s >> n.paths;
	s >> n.fileName;
	return s;
}

QDataStream& operator<<(QDataStream& s, const NetGraph& n)
{
	s << n.nodes;
	s << n.edges;
	s << n.connections;
	s << n.domains;
	s << n.paths;
	s << n.fileName;
	return s;
}

bool NetGraph::saveToFile()
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << *this;

	return true;
}

bool NetGraph::loadFromFile()
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> *this;

	return true;
}

#ifndef LINE_EMULATOR
NetGraphPath NetGraph::pathByNodeIndex(int src, int dst)
{
	foreach (NetGraphPath p, paths) {
		if (p.source == src && p.dest == dst)
			return p;
	}
	fprintf(stderr, "%s %d: %s(%d, %d)\n", __FILE__, __LINE__, __FUNCTION__, src, dst);
	printBacktrace();
	exit(1);
	return NetGraphPath(*this, -1, -1);
}
#else
NetGraphPath& NetGraph::pathByNodeIndex(int src, int dst)
{

	return paths[pathCache[QPair<qint32,qint32>(src,dst)]];
}
#endif

#ifndef LINE_EMULATOR
NetGraphEdge NetGraph::edgeByNodeIndex(int src, int dst)
{
	foreach (NetGraphEdge e, edges) {
		if (e.source == src && e.dest == dst)
			return e;
	}
	fprintf(stderr, "%s %d: %s(%d, %d)\n", __FILE__, __LINE__, __FUNCTION__, src, dst);
	printBacktrace();
	exit(1);
	return NetGraphEdge();
}
#else
NetGraphEdge& NetGraph::edgeByNodeIndex(int src, int dst)
{
	return edges[edgeCache[QPair<qint32,qint32>(src,dst)]];
}
#endif

int NetGraph::optimalQueueLength(double bandwidth, int delay)
{
	const double frameSize = 1500;
	double propDelay = delay;
	double transDelay = frameSize / bandwidth;
	double rtt = 2 * propDelay + 2 * transDelay;

	double trafficRTT = bandwidth * rtt;
	double qSize = ceil(trafficRTT / frameSize);
	if (qSize < 1)
		qSize = 1;

	return (int)qSize;
}

#ifndef LINE_EMULATOR
bool NetGraph::layoutGephi()
{
	return GephiLayout::layout(*this);
}
#endif

void NetGraph::updateUsed()
{
	// Remove unused paths
	for (int iPath = 0; iPath < paths.count(); iPath++) {
		bool used = false;
		NetGraphPath p = paths[iPath];
		foreach (NetGraphConnection c, connections) {
			if (c.source == c.dest)
				continue;
			if ((c.source == p.source && c.dest == p.dest) ||
				(c.source == p.dest && c.dest == p.source)) {
				used = true;
				break;
			}
		}
		if (!used) {
			paths.removeAt(iPath);
			iPath--;
		}
	}

	// Add new paths
	foreach (NetGraphConnection c, connections) {
		// forward
		bool found;
		found = false;
		for (int iPath = 0; iPath < paths.count(); iPath++) {
			NetGraphPath &p = paths[iPath];
			if (c.source == p.source && c.dest == p.dest) {
				found = true;
				if (p.edgeSet.isEmpty())
					p.retrace(*this);
				break;
			}
		}
		if (!found && (c.source != c.dest)) {
			paths << NetGraphPath(*this, c.source, c.dest);
		}
		// reverse
		found = false;
		for (int iPath = 0; iPath < paths.count(); iPath++) {
			NetGraphPath &p = paths[iPath];
			if (c.source == p.dest && c.dest == p.source) {
				found = true;
				if (p.edgeSet.isEmpty())
					p.retrace(*this);
				break;
			}
		}
		if (!found && (c.source != c.dest)) {
			paths << NetGraphPath(*this, c.dest, c.source);
		}
	}

	// Mark edges as used/not used
	foreach (NetGraphEdge e, edges) {
		// An edge is not used...
		edges[e.index].used = false;
		// unless there is a path that contains it.
		foreach (NetGraphPath p, paths) {
			foreach (NetGraphEdge pe, p.edgeSet) {
				if (pe.index == e.index) {
					edges[e.index].used = true;
					break;
				}
			}
			if (edges[e.index].used)
				break;
		}
	}

	// Mark nodes as used/not used
	// A node is not used...
	foreach (NetGraphNode n, nodes) {
		nodes[n.index].used = false;
	}
	// unless there is at least a used edge that contains it.
	foreach (NetGraphEdge e, edges) {
		if (e.used) {
			nodes[e.source].used = true;
			nodes[e.dest].used = true;
		}
	}
}

#ifndef LINE_EMULATOR
void NetGraph::computeASHulls(int ASNumber)
{
	// AS -> set of nodes
	QHash<int, QSet<int> > asNodes;

	foreach (NetGraphNode n, nodes) {
		if (ASNumber >= 0) {
			// Only the specified domain
			if (n.ASNumber == ASNumber)
				asNodes[n.ASNumber] << n.index;
		} else {
			// All domains
			asNodes[n.ASNumber] << n.index;
		}
	}

	int ASIndex = -1;
	if (ASNumber >= 0) {
		foreach (NetGraphAS d, domains) {
			if (d.ASNumber == ASNumber) {
				ASIndex = d.index;
				break;
			}
		}
		if (ASIndex < 0) {
			// this domain has not been seen previously, see if it is a new one
			if (asNodes.contains(ASNumber) && !asNodes.value(ASNumber, QSet<int>()).isEmpty()) {
				// we have a new AS
				ASIndex = domains.count();
				domains << NetGraphAS(domains.count(), ASNumber);
			} else {
				// there is no node within the specified AS
				return;
			}
		}
	}

	if (ASNumber < 0) {
		domains.clear();
	} else {
		domains[ASIndex].hull.clear();
	}

	foreach (int AS, asNodes.keys()) {
		QList<QPointF> points;
		foreach (int n, asNodes[AS]) {
			points << QPointF(nodes[n].x, nodes[n].y);
		}
		QList<QPointF> hull = ConvexHull::giftWrap(points);
		if (ASNumber < 0) {
			domains << NetGraphAS(domains.count(), AS, hull);
		} else {
			domains[ASIndex].hull = hull;
		}
	}
}
#endif

QList<NetGraphNode> NetGraph::getHostNodes()
{
	QList<NetGraphNode> result;
	foreach (NetGraphNode n, nodes) {
		if (n.nodeType == NETGRAPH_NODE_HOST)
			result << n;
	}
	return result;
}
