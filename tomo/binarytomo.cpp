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

#include "binarytomo.h"

typedef QSet<int> LinkSet;

uint qHash(const LinkSet &s)
{
	uint result = 0;

	foreach (int link, s)
		result ^= qHash(link);

	return result;
}

void binaryTomoLoss(TomoData &data)
{
	// the minimum transmission rate of a good link
	const double goodTresh = 0.98;

	// the maximum transmission rate of a bad link is goodTresh^maxPathLen
	// maxPathLen = 10 gives 90%
	double badPathTresh;

	int maxPathLength = 0;
	for (int i = 0; i < data.m; i++) {
		maxPathLength = qMax(maxPathLength, data.pathedges[i].count());
	}
	badPathTresh = pow(goodTresh, maxPathLength);

	// R = reachability matrix, R[pindex] = true iff path pindex is good
	QList<bool> R;

	for (int i = 0; i < data.m; i++) {
		R << (data.y[i] >= badPathTresh);
	}

	qDebug() << "R        = " << R;

	// The set of failure sets
	// Initialize L = empty set
	QSet<LinkSet> L;

	// Foreach bad path (R[i] = 0), L = L + L_i
	for (int i = 0; i < data.m; i++) {
		if (!R[i]) {
			// insert into L the set of links in path i
			L.insert(data.pathedges[i].toSet());
		}
	}

	qDebug() << "L        = " << L;

	// H = hypothesis set of failed links - computing this is our goal
	// Initialize H = empty set
	LinkSet H;

	// Lu = set of unexplained failure sets
	// Initialize Lu = L
	QSet<LinkSet> Lu = L;

	// U = the candidate set of failed links
	// Initialize U = union(L_i from L)
	LinkSet U;
	foreach (LinkSet Li, L) {
		U.unite(Li);
	}

	qDebug() << "Lu       = " << Lu;
	qDebug() << "U        = " << U;

	// Remove from U every link on a working path
	while (!Lu.isEmpty() && !U.isEmpty()) {
		qDebug() << "***";

		// C[link] = set of failure sets in Lu containing link
		QHash<int, QSet<LinkSet> > C;

		// compute C and score
		foreach (int link, U) {
			QSet<LinkSet> Clink;
			foreach (LinkSet s, Lu) {
				if (s.contains(link))
					Clink.insert(s);
			}
			C.insert(link, Clink);
		}

		// Lm = the set of links with the maximum score
		// score = |C[link]|, the number of unexplained failure sets containing this link
		LinkSet Lm;
		int scoreMax = 0;
		foreach (int link, C.keys())
			scoreMax = qMax(scoreMax, C[link].count());
		foreach (int link, C.keys()) {
			if (C[link].count() == scoreMax)
				Lm.insert(link);
		}

		qDebug() << "Lu       = " << Lu;
		qDebug() << "U        = " << U;
		qDebug() << "Lm       = " << Lm;

		foreach (int link, Lm) {
			// H = H + {link} : add link to the solution
			H.insert(link);
			// Lu = Lu - C[link] : all the failure sets C[link] are now explained, remove them from Lu
			Lu.subtract(C[link]);
			// U = U - {link}
			U.remove(link);
		}
	}

	qDebug() << "Bad links:";
	foreach (int link, H) {
		qDebug() << link;
	}
}
