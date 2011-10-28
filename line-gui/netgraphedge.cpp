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

#include "netgraphedge.h"

NetGraphEdge::NetGraphEdge()
{
	used = true;

	recordSampledTimeline = false;
	recordFullTimeline = false;
}

QString NetGraphEdge::tooltip()
{
	QString result;

	result += QString("(%1) %2 KB/s %3 ms q=%4 B").arg(index).arg(bandwidth).arg(delay_ms).arg(queueLength * 1500);
	// result = QString("id=%1").arg(index);

	if (lossBernoulli > 1.0e-12) {
		result += " L=" + QString::number(lossBernoulli);
	}

	return result;
}

double NetGraphEdge::metric()
{
	// Link metric = cost = ref-bandwidth/bandwidth; by default ref is 10Mbps = 1250 KBps
	// See http://www.juniper.com.lv/techpubs/software/junos/junos94/swconfig-routing/modifying-the-interface-metric.html#id-11111080
	// See http://www.juniper.com.lv/techpubs/software/junos/junos94/swconfig-routing/reference-bandwidth.html#id-11227690
	const double referenceBw_KBps = 1250.0;
	return referenceBw_KBps / bandwidth;
}

QDataStream& operator>>(QDataStream& s, NetGraphEdge& e)
{
	s >> e.index;
	s >> e.source;
	s >> e.dest;

	s >> e.delay_ms;
	s >> e.lossBernoulli;
	s >> e.queueLength;
	s >> e.bandwidth;

	s >> e.used;
	s >> e.recordSampledTimeline;
	s >> e.timelineSamplingPeriod;
	s >> e.recordFullTimeline;

	return s;
}

QDataStream& operator<<(QDataStream& s, const NetGraphEdge& e)
{
	s << e.index;
	s << e.source;
	s << e.dest;

	s << e.delay_ms;
	s << e.lossBernoulli;
	s << e.queueLength;
	s << e.bandwidth;

	s << e.used;
	s << e.recordSampledTimeline;
	s << e.timelineSamplingPeriod;
	s << e.recordFullTimeline;
	return s;
}

