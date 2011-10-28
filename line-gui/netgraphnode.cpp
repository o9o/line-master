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

#include "netgraphnode.h"

NetGraphNode::NetGraphNode()
{
	used = true;
}

QDataStream& operator>>(QDataStream& s, NetGraphNode& n)
{
	s >> n.index;
	s >> n.x;
	s >> n.y;
	s >> n.nodeType;
	s >> n.ASNumber;
	s >> n.used;
	s >> n.routes;
	return s;
}

QDataStream& operator<<(QDataStream& s, const NetGraphNode& n)
{
	s << n.index;
	s << n.x;
	s << n.y;
	s << n.nodeType;
	s << n.ASNumber;
	s << n.used;
	s << n.routes;
	return s;
}

QString NetGraphNode::routeTooltip()
{
	QString result;
	result += QString("<b>Routing table for node %1 (%2)</b><br/>").arg(index).arg(ip());
	result += QString("<table border='1' cellspacing='-1' cellpadding='2'>");
	result += QString("<tr><th>Destination</th><th>Next node</th></tr>");
	foreach (Route r, routes.routes.values()) {
		result += QString("<tr><td>%1 (%2)</td><td>%3 (%4)</td></tr>").arg(r.destination).arg(ip(r.destination)).arg(r.nextHop).arg(ip(r.nextHop));
	}
	result += QString("</table>");
	return result;
}
