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

#ifndef NETGRAPHCONNECTION_H
#define NETGRAPHCONNECTION_H

#include <QtCore>

class NetGraphConnection
{
public:
	NetGraphConnection();
	NetGraphConnection(int source, int dest, QString type, QByteArray data) :
		source(source), dest(dest), type(type), data(data)
	{}
	qint32 index;
	qint32 source;
	qint32 dest;
	// put everything you want in type and data
	QString type; // normally this is something meaningful, like "tcp" etc.
	QByteArray data; // this can hold the details (ports, transfer size etc.)
	// if type == "cmd"
	QString serverCmd;
	QString clientCmd;

	// various fields to be used freely by any code
	QString serverKey;
	QString clientKey;
	qint32 port;

	QList<qint32> ports;
	QStringList serverKeys;
	QStringList clientKeys;
	QStringList options;

	static NetGraphConnection cmdConnection(int source, int dest, QString serverCmd, QString clientCmd) {
		NetGraphConnection c (source, dest, "cmd", "");
		c.serverCmd = serverCmd;
		c.clientCmd = clientCmd;
		return c;
	}
};

QDataStream& operator>>(QDataStream& s, NetGraphConnection& c);

QDataStream& operator<<(QDataStream& s, const NetGraphConnection& c);

#endif // NETGRAPHCONNECTION_H
