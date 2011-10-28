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

#include "bgp.h"

#define DEBUG_IGP 0
#define DEBUG_BGP 0

class IGPRoute {
public:
	IGPRoute(int destination, int nextHop, double distance) :
		destination(destination), nextHop(nextHop), distance(distance)
	{}
	IGPRoute() {}
	// node ID
	int destination;
	// nextHop = ID of next node on the path to that destination
	int nextHop;
	// path metric according to the IGP
	double distance;

	// two routes are equal of they have the same destination and the same next hop
	bool operator==(const IGPRoute &other) const {
		return (this->destination == other.destination) && (this->nextHop == other.nextHop);
	}
	inline bool operator!=(const IGPRoute &other) const { return !(*this == other); }
};

inline QDebug operator<<(QDebug debug, const IGPRoute &route)
{
	debug.nospace() << "to " << route.destination << " via " << route.nextHop << " cost " << route.distance;
	return debug.space();
}

class IgpRoutingTable {
public:
	// Maps a destionation (node id) to one or more routes (by next hop) to that destination
	// If there are several routes, all have the same cost
	QMultiHash<int, IGPRoute> routes;
};

typedef QPair<int, int> QIntPair;

class BGPRoute {
public:
	BGPRoute(int destination, int nextHop, QString origin, QList<int> ASPath, double distToNextHop) :
		destination(destination), nextHop(nextHop), origin(origin), ASPath(ASPath), distToNextHop(distToNextHop)
	{}
	BGPRoute()
	{}
	// node ID
	int destination;
	// node ID, only set if the next hop is a border router
	int nextHop;
	// IGP, eBGP, iBGP
	QString origin;
	// in normal order, does not include the current AS;
	// the current AS is prepended only in eBGP updates
	QList<int> ASPath;
	// only set if nextHop is a border router in the current AS (iBGP updates)
	double distToNextHop;

	// this is better: return -1
	// other is better: return +1
	// returns 0 at tie
	int compareTo(BGPRoute other) {
		const int thisIsBetter = -1;
		const int otherIsBetter = +1;
		const int theSame = 0;
		if (other.destination != this->destination) {
			qDebug() << __FILE__ << __LINE__ << "illegal operation";
			exit(1);
		}
		if (this->origin == "IGP") {
			if (other.origin != "IGP")
				return thisIsBetter;
			// IGP vs IGP
			if (this->distToNextHop < other.distToNextHop)
				return thisIsBetter;
			if (this->distToNextHop == other.distToNextHop)
				return theSame;
			if (this->distToNextHop > other.distToNextHop)
				return otherIsBetter;
			return theSame;
		}
		if (other.origin == "IGP") {
			return otherIsBetter;
		}

		// we are comparing routes coming from eBGP or iBGP
		if (this->ASPath.length() < other.ASPath.length())
			return thisIsBetter;
		if (this->ASPath.length() > other.ASPath.length())
			return otherIsBetter;
		// ASPath tie

		if (this->origin == "eBGP" && other.origin == "iBGP")
			return thisIsBetter;
		if (this->origin == "iBGP" && other.origin == "eBGP")
			return otherIsBetter;

		// origin tie
		if (this->distToNextHop < other.distToNextHop)
			return thisIsBetter;
		if (this->distToNextHop > other.distToNextHop)
			return otherIsBetter;

		// distance tie
		if (this->nextHop < other.nextHop)
			return thisIsBetter;
		if (this->nextHop > other.nextHop)
			return otherIsBetter;

		return theSame;
	}

	QString toString() {
		QString ASPathStr = "(";
		foreach (int ASN, ASPath) {
			ASPathStr += QString("%1AS%2").arg(ASPathStr.endsWith("(") ? "" : " -> ").arg(ASN);
		}
		ASPathStr += ")";

		return QString("dest = %1, nextHop = %2, distance = %3, origin = %4, ASPath = %5").arg(destination).arg(nextHop).arg(distToNextHop).arg(origin).arg(ASPathStr);
	}
};

class BGPRoutingTable {
public:
	// Maps a destionation (node id) to a route
	QHash<int, BGPRoute > bestRoutes;
	// Maps a destionation (node id) to a route
	// QHash<int, BGPRoute > localRoutes;
};

class PrioQueue {
public:
	// does not check for duplicates
	void insert(int key, double prio) {
		queue.append(QPair<int, double>(key, prio));
	}

	void update(int key, double prio) {
		for (int i = 0; i < queue.size(); i++) {
			if (queue[i].first == key) {
				queue[i].second = prio;
				return;
			}
		}
	}

	// does not check for empty
	QPair<int, double> takeSmallest() {
		int imin = 0;
		double min = queue.at(imin).second;

		for (int i = 0; i < queue.count(); i++) {
			if (queue[i].second < min) {
				min = queue[i].second;
				imin = i;
			}
		}

		// qDebug() << "take smallest" << queue << "result:" << queue.at(imin);

		return queue.takeAt(imin);
	}

	double peek(int key) {
		for (int i = 0; i < queue.size(); i++) {
			if (queue[i].first == key) {
				return queue[i].second;
			}
		}
		// you are in trouble
		return 1;
	}

	bool isEmpty() {
		return queue.isEmpty();
	}

private:
	QList< QPair<int, double> > queue;
};

bool almostEqual(qreal a, qreal b)
{
	if (a == b)
		return true;
	const qreal e = 1.0e-10;
	return (((b - e) < a) && (a < (b + e)));
}

// nodes = the IDs of the nodes in the graph
// root = the ID of the root node
// neighbours = adjacencies
// metrics = (node ID, node ID) -> edge length
// destinations = subset of 'nodes'; only compute routes to these node IDs
void dijkstra(QSet<int> nodes, int root, QHash<int, QList<int> > neighbours, QHash<QIntPair, double> metrics, QSet<int> destinations,
			  QHash<int, IgpRoutingTable> &IGPRoutes)
{
	// set of (node, distance), for each node that has not been visited yet
	PrioQueue queue;
	// node -> predecessor
	QMultiHash<int, int> parent;
	// visited nodes
	QSet<int> visited;
	// best distances
	QHash<int, double> distances;

	foreach (int n, nodes) {
		queue.insert(n, 1.0e100);
	}
	queue.update(root, 0);

	while (!queue.isEmpty()) {
		QPair<int, double> best = queue.takeSmallest();
		int n = best.first;
		double dist = best.second;
		visited.insert(n);
		distances[n] = dist;
		//qDebug() << QString("visited: %1, dist = %2").arg(n).arg(dist);

		foreach (int n2, neighbours[n]) {
			if (!visited.contains(n2)) {
				double metric = metrics.value(QPair<int, int>(n, n2));
				double d2 = queue.peek(n2);
				if (almostEqual(dist + metric, d2)) {
					parent.insert(n2, n);
				} else if (dist + metric < d2) {
					queue.update(n2, dist + metric);
					parent.remove(n2);
					parent.insert(n2, n);
					//qDebug() << QString("new parent: %1 -> %2").arg(n).arg(n2);
				}
			}
		}
	}

	if (DEBUG_IGP) qDebug() << "dijkstra root =" << root << ":";
	foreach (int d, destinations) {
		if (DEBUG_IGP) qDebug() << "dijkstra dest =" << d << ":";
		QList<int> leaves;
		leaves << d;
		while (!leaves.isEmpty()) {
			int n = leaves.takeLast();
			if (DEBUG_IGP) qDebug() << "parents of node" << n << ":" << parent.values(n);
			foreach (int p, parent.values(n)) {
				// a route from p to d, via n
				IGPRoute route (d, n, distances[d] - distances[p]);
				if (DEBUG_IGP) qDebug() << "setting route of node" << p << route;
				if (!IGPRoutes[p].routes.contains(d, route)) {
					IGPRoutes[p].routes.insert(d, route);
				}
				if (p != root) {
					leaves << p;
				}
			}
		}
	}
}

// asNodes: Maps an ASN to the set of nodes (all routers and hosts) in that AS
void computeIGPRoutingTable(NetGraph &g, int root, QSet<int> destinations, QHash<int, QSet<int> > asNodes, QHash<int, IgpRoutingTable> &IGPRoutes)
{
	int asNumber = g.nodes[root].ASNumber;
	QSet<int> nodeSet = asNodes.value(asNumber);

	// For each node, we keep an adjacency list
	QHash<int, QList<int> > neighbours;
	// For each adjacency, we store the metric
	QHash<QIntPair, double> metrics;

	// build the adjacency lists
	foreach (int n, nodeSet) {
		foreach (NetGraphEdge e, g.edges) {
			if (e.source == n && nodeSet.contains(e.dest)) {
				neighbours[n] << e.dest;
				QPair<int, int> link = QPair<int, int>(e.source, e.dest);
				metrics[link] = e.metric();
			}
		}
	}
	if (DEBUG_IGP) qDebug() << "Adjacencies" << neighbours;
	if (DEBUG_IGP) qDebug() << "Metrics" << metrics;

	dijkstra(nodeSet, root, neighbours, metrics, destinations, IGPRoutes);
}

// IGPRoutes: Maps a node (node id) to its IGP routing table
QHash<int, BGPRoutingTable> computeBGPRoutingTables(QHash<int, QSet<int> > asBorders, QHash<int, QSet<int> > asDestinations, QHash<int, IgpRoutingTable> IGPRoutes,
					    QHash<QPair<int, int>, double> eBGPPeers, QHash<QPair<int, int>, double> iBGPPeers,
					    QHash<int, int> borderToASNumber)
{
	// For each border router, keep the BGP routing table
	QHash<int, BGPRoutingTable> BGPRoutingTables;

	// initialize local routes
	if (DEBUG_BGP) qDebug() << "Advertising local networks:";
	foreach (int ASNumber, asBorders.keys()) {
		foreach (int border, asBorders[ASNumber]) {
			foreach (int dest, asDestinations[ASNumber]) {
				double dist = IGPRoutes[border].routes.value(dest).distance;
				BGPRoutingTables[border].bestRoutes.insert(dest, BGPRoute(dest, -1, "IGP", QList<int>(), dist));
				if (DEBUG_BGP) qDebug() << QString("AS %1, border %2: IGP route to %3, dist = %4, next hop:").arg(ASNumber).arg(border).arg(dest).arg(dist) << IGPRoutes[border].routes.value(dest).nextHop;
			}
		}
	}

	// BGP rounds
	for (int roundIndex = 1; ; roundIndex++) {
		if (DEBUG_BGP) qDebug() << QString("BGP round %1").arg(roundIndex);
		bool changed = false;
		foreach (int ASNumber, asBorders.keys()) {
			foreach (int border, asBorders[ASNumber]) {
				// we are trying to improve the routing table of router "border"
				BGPRoutingTable &myTable = BGPRoutingTables[border];

				// iterate over eBGP peers, and for each destination they have see if their route is better
				foreach (QIntPair peering, eBGPPeers.keys()) {
					if (peering.first == border) {
						int peer = peering.second;
						double distance = eBGPPeers[peering];
						int peerAS = borderToASNumber[peer];
						BGPRoutingTable &peerTable = BGPRoutingTables[peer];

						// iterate over his routes
						foreach (int d, peerTable.bestRoutes.keys()) {
							BGPRoute peerRoute = peerTable.bestRoutes[d];
							// simulate an eBGP update
							peerRoute.ASPath.prepend(peerAS);
							peerRoute.nextHop = peer;
							peerRoute.distToNextHop = distance;
							peerRoute.origin = "eBGP";

							// avoid loops
							if (peerRoute.ASPath.contains(ASNumber))
								continue;

							if (!myTable.bestRoutes.contains(d)) {
								// anything is better than nothing
								myTable.bestRoutes.insert(d, peerRoute);
								changed = true;
							} else {
								BGPRoute myRoute = myTable.bestRoutes[d];
								if (myRoute.compareTo(peerRoute) > 0) {
									// his is better
									myTable.bestRoutes.insert(d, peerRoute);
									changed = true;
								}
							}
						}
					}
				}

				// iterate over iBGP peers, and for each destination they have see if their route is better
				foreach (QIntPair peering, iBGPPeers.keys()) {
					if (peering.first == border) {
						int peer = peering.second;
						double distance = iBGPPeers[peering];
						BGPRoutingTable &peerTable = BGPRoutingTables[peer];

						// iterate over his routes
						foreach (int d, peerTable.bestRoutes.keys()) {
							BGPRoute peerRoute = peerTable.bestRoutes[d];
							// simulate an iBGP update
							// chained updates are forbidden via iBGP
							if (peerRoute.origin == "iBGP")
								continue;
							peerRoute.nextHop = peer;
							peerRoute.distToNextHop = distance;
							peerRoute.origin = "iBGP";

							// avoid loops
							if (peerRoute.ASPath.contains(ASNumber))
								continue;

							if (!myTable.bestRoutes.contains(d)) {
								// anything is better than nothing
								myTable.bestRoutes.insert(d, peerRoute);
								changed = true;
							} else {
								BGPRoute myRoute = myTable.bestRoutes[d];
								if (myRoute.compareTo(peerRoute) > 0) {
									// his is better
									myTable.bestRoutes.insert(d, peerRoute);
									changed = true;
								}
							}
						}
					}
				}
			}
		}
		if (!changed)
			break;
	}

	// print result
	if (DEBUG_BGP) qDebug() << "BGP result:";
	foreach (int ASNumber, asBorders.keys()) {
		if (DEBUG_BGP) qDebug() << QString("AS%1:").arg(ASNumber);
		foreach (int border, asBorders[ASNumber]) {
			if (DEBUG_BGP) qDebug() << QString("AS%1 router %2:").arg(ASNumber).arg(border);
			foreach (int d, BGPRoutingTables[border].bestRoutes.keys()) {
				if (DEBUG_BGP) qDebug() << QString("To node %1: %2").arg(d).arg(BGPRoutingTables[border].bestRoutes[d].toString());
			}
		}
	}

	return BGPRoutingTables;
}

// Populates the routing tables with the necessary information to route between source and routeDest via localDest.
// routes = routing tables; we only fill the tables of the nodes between source and (exclusively) localDest.
// source = the ID of the source node (the node whose routing table we compute).
// localDest = local destination
// routeDest = route destination (what will be entered in the routing table)
//   For intra-domain routes: localDest == routeDest.
//   For inter-domain routes: routeDest = packet destination, localDest = domain exit point (border router), the last node
//   on the path (source..routeDest) that is known via IGP routing.
// IGPRoutes = the already computed IGP routing tables
// Restriction: AS(source) == AS(localDest).
void computeFinalRoutesIGP(QHash<int, RoutingTable> &routes, int source, int localDest, int routeDest, QHash<int, IgpRoutingTable> IGPRoutes)
{
	QList<int> queue;
	queue << source;
	while (!queue.isEmpty()) {
		source = queue.takeFirst();
		foreach (IGPRoute igpR, IGPRoutes[source].routes.values(localDest)) {
			if (!routes[source].routes.contains(routeDest, Route(routeDest, igpR.nextHop))) {
				routes[source].routes.insert(routeDest, Route(routeDest, igpR.nextHop));
				if (igpR.nextHop != localDest)
					queue << igpR.nextHop;
			}
		}
	}
}

// Computes the ID of the border router which is the optimal exit point from AS(n1) on the path between n1 and n2.
// Also returns the next hop of that border router.
// If no route is found, closestBorder is -1.
// Restriction: AS(n1) != AS(n2).
void bestBGPExitPoint(NetGraph &g, int &closestBorder, int &nextHop,
					  int n1, int n2, QHash<int, IgpRoutingTable> IGPRoutes, QHash<int, BGPRoutingTable> BGPRoutingTables,
					  QHash<int, QSet<int> > asBorders)
{
	// if different AS, take the IGP route to the closest BGP border router that has next hop in a different AS
	// pick the best border router from the source, route via IGP to there
	// this corresponds to a situation where the border routers redistribute routes to IGP
	closestBorder = -1;
	double closestDist = 1.0e100;
	int AS = g.nodes[n1].ASNumber;
	foreach (int border, asBorders[AS]) {
		if (IGPRoutes[n1].routes.contains(border)) {
			// we only care about one route, as all have the same dist and that's
			// what we actually want to find out
			IGPRoute directRoute = IGPRoutes[n1].routes.value(border);

			// border2 can be an eBGP or an iBGP peer
			// ignore iBGP routes; this way we prevent suboptimal routing in the
			// source AS
			int border2 = BGPRoutingTables[border].bestRoutes[n2].nextHop;
			if (g.nodes[border].ASNumber == g.nodes[border2].ASNumber) {
				// iBGP, ignore it
			} else {
				if (directRoute.distance < closestDist) {
					closestDist = directRoute.distance;
					closestBorder = border;
					nextHop = border2;
				}
			}
		}
	}
}

void computeFinalRoutes(QHash<int, RoutingTable> &routes, NetGraph &g, int n1, int n2, QHash<int, IgpRoutingTable> IGPRoutes,
				    QHash<int, BGPRoutingTable> BGPRoutingTables, QHash<int, QSet<int> > asBorders)
{
	while (g.nodes[n1].ASNumber != g.nodes[n2].ASNumber) {
		// if different AS, take the IGP route to the closest BGP border router that has next hop in a different AS
		// pick the best border router from the source, route via IGP to there
		// this corresponds to a situation where the border routers redistribute routes to IGP
		int closestBorder;
		int nextBorder;
		bestBGPExitPoint(g, closestBorder, nextBorder, n1, n2, IGPRoutes, BGPRoutingTables, asBorders);
		if (closestBorder < 0)
			return;
		// add routes from n1 to n2 via closestBorder
		computeFinalRoutesIGP(routes, n1, closestBorder, n2, IGPRoutes);
		// add routes from closestBorder to n2 via nextBorder
		n1 = closestBorder;
		if (!routes[n1].routes.contains(n2, Route(n2, nextBorder))) {
			routes[n1].routes.insert(n2, Route(n2, nextBorder));
			if (n1 == 7 && n2 == 2 && nextBorder == 8) qDebug() << __FILE__ << __LINE__;
		}
		// continue by populating the routing table of the next border router
		n1 = nextBorder;
	}
	// if same AS, take the IGP route
	if (g.nodes[n1].ASNumber == g.nodes[n2].ASNumber) {
		computeFinalRoutesIGP(routes, n1, n2, n2, IGPRoutes);
	}
}

void Bgp::computeRoutes(NetGraph &g)
{
	// Maps an ASN to the set of nodes (all routers and hosts) in that AS
	QHash<int, QSet<int> > asNodes;

	// Maps an ASN to the set of border routers in that AS
	QHash<int, QSet<int> > asBorders;

	// Maps a node (node id) to its IGP routing table
	QHash<int, IgpRoutingTable> IGPRoutes;

	// Maps a node (node id) to its routing table; computing this is our final goal
	QHash<int, RoutingTable> routes;

	// First we fill up all the data structures
	foreach (NetGraphNode n, g.nodes) {
		asNodes[n.ASNumber].insert(n.index);
		if (n.nodeType == NETGRAPH_NODE_BORDER) {
			asBorders[n.ASNumber].insert(n.index);
		}
	}

	if (DEBUG_BGP) qDebug() << "AS Numbers:" << asNodes.keys();
	if (DEBUG_BGP) qDebug() << "Border routers:" << asBorders.values();

	// For each AS compute the interior routing table
	// Only do it for interesting nodes
	QSet<int> roots;
	foreach (NetGraphNode n, g.nodes) {
		if (n.nodeType == NETGRAPH_NODE_BORDER) {
			roots.insert(n.index);
		}
	}
	foreach (NetGraphConnection c, g.connections) {
		roots.insert(c.source);
		roots.insert(c.dest);
	}
	if (DEBUG_BGP) qDebug() << "Roots:" << roots;

	// initialize the IGP routing tables
	foreach (NetGraphNode n, g.nodes) {
		IgpRoutingTable t;
		IGPRoutes[n.index] = t;
	}

	foreach (int r, roots) {
		// we only keep the routes to interesting destinations
		QSet<int> destinations;
		int asNumber = g.nodes[r].ASNumber;
		foreach (int n, roots) {
			if (g.nodes[n].ASNumber == asNumber) {
				destinations << n;
			}
		}
		if (DEBUG_BGP) qDebug() << "Destinations for node" << r << ":" << destinations;

		computeIGPRoutingTable(g, r, destinations, asNodes, IGPRoutes);
	}
	foreach (NetGraphNode n, g.nodes) {
		if (DEBUG_IGP) qDebug() << "IGPRoutes for node" << n.index << ":" << IGPRoutes[n.index].routes.values();
	}

	// for each AS, keep a set of the destinations (advertised routes i.e. what the network command does)
	QHash<int, QSet<int> > asDestinations;
	foreach (NetGraphConnection c, g.connections) {
		int n = c.source;
		int ASNumber = g.nodes[n].ASNumber;
		asDestinations[ASNumber].insert(n);
		n = c.dest;
		ASNumber = g.nodes[n].ASNumber;
		asDestinations[ASNumber].insert(n);
	}

	// compute peerings; the double value is the path or link metric between peers
	QHash<QPair<int, int>, double> eBGPPeers;
	QHash<QPair<int, int>, double> iBGPPeers;

	foreach (NetGraphEdge e, g.edges) {
		if (g.nodes[e.source].nodeType == NETGRAPH_NODE_BORDER && g.nodes[e.dest].nodeType == NETGRAPH_NODE_BORDER &&
				g.nodes[e.source].ASNumber != g.nodes[e.dest].ASNumber) {
			eBGPPeers.insert(QPair<int, int>(e.source, e.dest), e.metric());
		}
	}
	if (DEBUG_BGP) qDebug() << "eBGP peerings:" << eBGPPeers;

	// iBGP needs a full mesh between the border routers within an AS
	foreach (int ASNumber, asBorders.keys()) {
		foreach (int br1, asBorders[ASNumber]) {
			foreach (int br2, asBorders[ASNumber]) {
				if (br1 != br2) {
					iBGPPeers.insert(QPair<int, int>(br1, br2), IGPRoutes[br1].routes.value(br2).distance);
				}
			}
		}
	}
	if (DEBUG_BGP) qDebug() << "iBGP peerings:" << iBGPPeers;

	// Map border routers to their ASNumbers
	QHash<int, int> borderToASNumber;
	foreach (int ASNumber, asBorders.keys()) {
		foreach (int br, asBorders[ASNumber]) {
			borderToASNumber[br] = ASNumber;
		}
	}

	// compute BGP routes
	QHash<int, BGPRoutingTable> BGPRoutingTables = computeBGPRoutingTables(asBorders, asDestinations, IGPRoutes, eBGPPeers, iBGPPeers, borderToASNumber);

	// We can finally compute the final routes
	foreach (NetGraphConnection c, g.connections) {
		int n1 = c.source;
		int n2 = c.dest;
		if (n1 == n2)
			continue;

		computeFinalRoutes(routes, g, n1, n2, IGPRoutes, BGPRoutingTables, asBorders);
		computeFinalRoutes(routes, g, n2, n1, IGPRoutes, BGPRoutingTables, asBorders);
	}
	// ...and save them
	foreach (NetGraphNode n, g.nodes) {
		g.nodes[n.index].routes = routes[n.index];
		if (DEBUG_BGP) qDebug() << "Final routes for" << n.index << ":" << routes[n.index].routes.values();
	}
}

void Bgp::testIGP()
{
	NetGraph g;
	int n0 = g.addNode(NETGRAPH_NODE_GATEWAY, QPointF(), 0);
	int n1 = g.addNode(NETGRAPH_NODE_GATEWAY, QPointF(), 0);
	int n2 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), 0);
	int n3 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), 0);
	int n4 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), 0);
	int n5 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), 0);
	int n6 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), 0);
	int n7 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), 0);
	int n8 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), 0);

/*
*                              -----100---> 7 <---1000---> 8 <---1000------
*                             /                                            \
*                             |                                            |
*                             v                                            v
*    [9] <---> 0 <---1000---> 3 <---1000---> 4 <---1000---> 5 <----100---> 1 <---> [10]
*                             ^                                            ^
*                             |                                            |
*                             \                                            /
*                              -----10---> 6 <------------------10---------
*                                          ^
*                                          |
*                                          \
*                                           ---10---> (2)
*/

	g.addEdgeSym(n0, n3, 1000, 100, 0, 100);
	g.addEdgeSym(n3, n4, 1000, 100, 0, 100);
	g.addEdgeSym(n4, n5, 1000, 100, 0, 100);
	g.addEdgeSym(n5, n1, 100, 100, 0, 100);
	g.addEdgeSym(n3, n6, 10, 100, 0, 100);
	g.addEdgeSym(n6, n1, 10, 100, 0, 100);
	g.addEdgeSym(n6, n2, 10, 100, 0, 100);
	g.addEdgeSym(n3, n7, 100, 100, 0, 100);
	g.addEdgeSym(n7, n8, 1000, 100, 0, 100);
	g.addEdgeSym(n8, n1, 1000, 100, 0, 100);

	int n9 = g.addNode(NETGRAPH_NODE_HOST, QPointF(), 0);
	int n10 = g.addNode(NETGRAPH_NODE_HOST, QPointF(), 0);
	g.addEdgeSym(n9, n0, 1000, 100, 0, 100);
	g.addEdgeSym(n10, n1, 1000, 100, 0, 100);

	g.addConnection(NetGraphConnection(n9, n10, "", QByteArray()));

	g.computeRoutes();
}

void Bgp::testBGP()
{
/*
*
*   [0,AS5] ---- 21,AS5 ---- (3,AS5) ==== (4,AS4) ---- 8,AS4 ---- (7,AS4) ====================================\\
*                                                      \---- 9,AS4 ---- 10,AS4 ---------- (5,AS4)             ||
*                                                                        \---- (6,AS4)      ||                ||
*                                                                                ||         ||                ||
*                               //================== (17,AS3) ---- 16,AS3 ---- (15,AS3)     ||                ||
*                               ||                                                          ||                ||
*                               ||                                                          ||                ||
*                               ||                                                          ||                ||
*   [1,AS6] ---- 22,AS6 ----------------------------------------------------------------- (11,AS6)            ||
*                               ||                                                                            ||
*                               ||                                                                            ||
*                               ||                                                                            ||
*                               ||                                                                            ||
*                               ||                                                                            ||
*                               ||                                                                            ||
*                               ||                                                                            ||
*   [2,AS1] ---- 20,AS1 ---- (18,AS1)                                                                         ||
*                  |                                                                                          ||
*                  |                                                                                          ||
*               (19,AS1) =============== (14,AS2) ---- 13,AS2 ---- (12,AS2) ==================================//
*/

	NetGraph g;
	int AS1 = 1;
	int AS2 = 2;
	int AS3 = 3;
	int AS4 = 4;
	int AS5 = 5;
	int AS6 = 6;
	int n0 = g.addNode(NETGRAPH_NODE_HOST, QPointF(), AS5);//
	int n1 = g.addNode(NETGRAPH_NODE_HOST, QPointF(), AS6);//
	int n2 = g.addNode(NETGRAPH_NODE_HOST, QPointF(), AS1);//
	int n3 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS5);//
	int n4 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS4);//
	int n5 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS4);//
	int n6 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS4);//
	int n7 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS4);//
	int n8 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), AS4);//
	int n9 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), AS4);//
	int n10 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), AS4);//
	int n11 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS6);//
	int n12 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS2);//
	int n13 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), AS2);//
	int n14 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS2);//
	int n15 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS3);//
	int n16 = g.addNode(NETGRAPH_NODE_ROUTER, QPointF(), AS3);//
	int n17 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS3);//
	int n18 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS1);//
	int n19 = g.addNode(NETGRAPH_NODE_BORDER, QPointF(), AS1);//
	int n20 = g.addNode(NETGRAPH_NODE_GATEWAY, QPointF(), AS1);//
	int n21 = g.addNode(NETGRAPH_NODE_GATEWAY, QPointF(), AS5);//
	int n22 = g.addNode(NETGRAPH_NODE_GATEWAY, QPointF(), AS6);//

	g.addEdgeSym(n0, n21, 1250, 100, 0, 100);//
	g.addEdgeSym(n21, n3, 1250, 100, 0, 100);//
	g.addEdgeSym(n3, n4, 1250, 100, 0, 100);//
	g.addEdgeSym(n4, n8, 1250, 100, 0, 100);//
	g.addEdgeSym(n8, n7, 1250, 100, 0, 100);//
	g.addEdgeSym(n8, n9, 1250, 100, 0, 100);//
	g.addEdgeSym(n9, n10, 1250, 100, 0, 100);//
	g.addEdgeSym(n10, n5, 1250, 100, 0, 100);//
	g.addEdgeSym(n10, n6, 1250, 100, 0, 100);//
	g.addEdgeSym(n5, n11, 1250, 100, 0, 100);//
	g.addEdgeSym(n11, n22, 1250, 100, 0, 100);//
	g.addEdgeSym(n22, n1, 1250, 100, 0, 100);//
	g.addEdgeSym(n7, n12, 1250, 100, 0, 100);//
	g.addEdgeSym(n6, n15, 1250, 100, 0, 100);//
	g.addEdgeSym(n12, n13, 1250, 100, 0, 100);//
	g.addEdgeSym(n13, n14, 1250, 100, 0, 100);//
	g.addEdgeSym(n15, n16, 1250, 100, 0, 100);//
	g.addEdgeSym(n16, n17, 1250, 100, 0, 100);//
	g.addEdgeSym(n17, n18, 1250, 100, 0, 100);//
	g.addEdgeSym(n14, n19, 1250, 100, 0, 100);//
	g.addEdgeSym(n19, n20, 1250, 100, 0, 100);//
	g.addEdgeSym(n18, n20, 1250, 100, 0, 100);//
	g.addEdgeSym(n20, n2, 1250, 100, 0, 100);//

	g.addConnection(NetGraphConnection(n0, n2, "", QByteArray()));

	g.computeRoutes();
}
