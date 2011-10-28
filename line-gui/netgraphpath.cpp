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

#include "netgraphpath.h"
#include "netgraph.h"

NetGraphPath::NetGraphPath()
{
	recordSampledTimeline = false;
}

NetGraphPath::NetGraphPath(NetGraph &g, int source, int dest) :
	source(source), dest(dest)
{
	recordSampledTimeline = false;
	retrace(g);
}

void NetGraphPath::retrace(NetGraph &g)
{
	// compute edge set
	QList<int> nodeQueue;
	QSet<int> nodesEnqueued;
	nodeQueue << source;
	nodesEnqueued << source;
	bool loadBalanced = false;
	while (!nodeQueue.isEmpty()) {
		int n = nodeQueue.takeFirst();
		QList<Route> routes = g.nodes[n].routes.routes.values(dest);
		loadBalanced = loadBalanced || (routes.count() > 1);
		foreach (Route r, routes) {
			NetGraphEdge e = g.edgeByNodeIndex(n, r.nextHop);
			edgeSet.insert(e);
			if (!loadBalanced) {
				edgeList << e;
			}
			if (r.nextHop != dest && !nodesEnqueued.contains(r.nextHop)) {
				nodeQueue << r.nextHop;
				nodesEnqueued << r.nextHop;
			}
		}
	}
	if (loadBalanced)
		edgeList.clear();
}

QString NetGraphPath::toString()
{
	QString result;

	foreach (NetGraphEdge e, edgeList) {
		result += QString::number(e.source) + " -> " + QString::number(e.dest) + " (" + e.tooltip() + ") ";
	}
	result += "Bandwidth: " + QString::number(bandwidth()) + " KB/s ";
	result += "Delay for 500B frames: " + QString::number(computeFwdDelay(500)) + " ms ";
	result += "Loss (Bernoulli): " + QString::number(lossBernoulli() * 100.0) + "% ";
	return result;
}

double NetGraphPath::computeFwdDelay(int frameSize)
{
	double result = 0;
	foreach (NetGraphEdge e, edgeList) {
		result += e.delay_ms + frameSize/e.bandwidth;
	}
	return result;
}

double NetGraphPath::bandwidth()
{
	double result = 1.0e99;
	foreach (NetGraphEdge e, edgeList) {
		if (e.bandwidth < result)
			result = e.bandwidth;
	}
	return result;
}

double NetGraphPath::lossBernoulli()
{
	double success = 1.0;
	foreach (NetGraphEdge e, edgeList) {
		success *= 1.0 - e.lossBernoulli;
	}
	return 1.0 - success;
}

QDataStream& operator>>(QDataStream& s, NetGraphPath& p)
{
	s >> p.edgeSet;
	s >> p.edgeList;
	s >> p.source;
	s >> p.dest;
	s >> p.recordSampledTimeline;
	s >> p.timelineSamplingPeriod;
	return s;
}

QDataStream& operator<<(QDataStream& s, const NetGraphPath& p)
{
	s << p.edgeSet;
	s << p.edgeList;
	s << p.source;
	s << p.dest;
	s << p.recordSampledTimeline;
	s << p.timelineSamplingPeriod;
	return s;
}
