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

#ifndef TOMODATA_H
#define TOMODATA_H

#include <QtCore>

class TomoData {
public:
	TomoData();

	// m = number of paths
	qint32 m;
	// n = number of edges
	qint32 n;

	// pathstarts[index] = node index, maps a path index to the source node
	QVector<qint32> pathstarts;
	// pathends[index] = node index, maps a path index to the dest node
	QVector<qint32> pathends;

	// pathedges[pindex] = list of the indices of the edges in path pindex, in order
	QVector<QList<qint32> > pathedges;

	// A = routing matrix, A[i][j] = 1 iff path i contains edge j
	QVector<QVector<quint8> > A;

	// y[pindex] = transmission rate (0..1) of path pindex
	QVector<qreal> y;

	// xmeasured[eindex] = transmission rate of edge eindex
	QVector<qreal> xmeasured;

	// time range of the simulation (ns)
	quint64 tsMin;
	quint64 tsMax;

	bool load(QString fileName);
	bool save(QString fileName);

	void testEquations() {
		for (int ip = 0; ip < pathedges.count(); ip++) {
			double rateR = y[ip];
			double rateE = 1.0;
			for (int ie = 0; ie < pathedges[ip].count(); ie++) {
				rateE *= xmeasured[pathedges[ip][ie]];
			}
			printf("%f %f\n", rateR, rateE);
		}
	}

protected:
	void resize();
};

#endif // TOMODATA_H
