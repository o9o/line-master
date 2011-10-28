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

#ifndef NETGRAPH_H
#define NETGRAPH_H

#include <QtCore>

#include "netgraphnode.h"
#include "netgraphedge.h"
#include "netgraphpath.h"
#include "netgraphas.h"
#include "netgraphconnection.h"

class NetGraph
{
public:
	NetGraph();

	QList<NetGraphNode> nodes;
	QList<NetGraphEdge> edges;
	QList<NetGraphConnection> connections;

	// updated by computeASHulls
	QList<NetGraphAS> domains;

	// not updated by anything
	QList<NetGraphPath> paths;
	QString fileName;

#ifdef LINE_EMULATOR
	// maps (node ID, node ID) -> edge ID
	QHash<QPair<qint32, qint32>, qint32> edgeCache;
	// maps (node ID, node ID) -> path ID
	QHash<QPair<qint32, qint32>, qint32 > pathCache;
#endif

	// Adds a node of type NETGRAPH_NODE_something, at a scene position pos
	// Returns the node index
	int addNode(int type, QPointF pos = QPointF(), int ASNumber = 0);

	// Adds an edge between two nodes
	// Returns the edge index
	int addEdge(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength);

	// Adds two edges with identical attrbiutes in both directions
	// Returns the index of the second one
	int addEdgeSym(int nodeStart, int nodeEnd, double bandwidth, int delay, double loss, int queueLength);
	// Checks whether an edge can be added between two nodes
	bool canAddEdge(int nodeStart, int nodeEnd);

	// Adds a connection
	int addConnection(NetGraphConnection c);

	void deleteNode(int index);
	void deleteEdge(int index);
	void deleteConnection(int index);

	// deletes everything
	void clear();

	// sets the file name
	void setFileName(QString fileName);

	bool saveToFile();
	bool loadFromFile();

#ifndef LINE_EMULATOR
	// computes the routing tables and then the paths
	void computeRoutes();

	// layouts the graph using Gephi
	bool layoutGephi();

	// computes the AS list and their GUI convex hulls
	void computeASHulls(int ASNumer = -1);
#endif

	// searches the path with given src and dst node IDs
#ifndef LINE_EMULATOR
	NetGraphPath pathByNodeIndex(int src, int dst);
#else
	NetGraphPath& pathByNodeIndex(int src, int dst);
#endif

	// searches the edge with given src and dst node IDs
#ifndef LINE_EMULATOR
	NetGraphEdge edgeByNodeIndex(int src, int dst);
#else
	NetGraphEdge& edgeByNodeIndex(int src, int dst);
#endif

	// Marks nodes and edges as used/not used. The paths (actually the routes) need to be computed.
	void updateUsed();

	// Enumerates the nodes that are hosts
	QList<NetGraphNode> getHostNodes();

#ifdef LINE_EMULATOR
	void prepareEmulation();
#endif

	// Returns the optimal queue length (i.e. 1 RTT of traffic for 400B frames) in slots, given the bandwidth (KB/s) and delay (ms) over a link
	static int optimalQueueLength(double bandwidth, int delay);
	static void testComputePaths();
};

QDataStream& operator>>(QDataStream& s, NetGraph& n);

QDataStream& operator<<(QDataStream& s, const NetGraph& n);

#endif // NETGRAPH_H
