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

#include "netgraphconnection.h"

NetGraphConnection::NetGraphConnection()
{
}

QDataStream& operator>>(QDataStream& s, NetGraphConnection& c)
{
	s >> c.index;
	s >> c.source;
	s >> c.dest;
	s >> c.type;
	s >> c.data;

	s >> c.serverKey;
	s >> c.clientKey;
	s >> c.port;
	s >> c.ports;
	return s;
}

QDataStream& operator<<(QDataStream& s, const NetGraphConnection& c)
{
	s << c.index;
	s << c.source;
	s << c.dest;
	s << c.type;
	s << c.data;

	s << c.serverKey;
	s << c.clientKey;
	s << c.port;
	s << c.ports;
	return s;
}
