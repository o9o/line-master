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

#include "tomodata.h"

#include "../util/util.h"

TomoData::TomoData()
{
	m = n = 0;
	tsMin = tsMax = 0;
}

bool TomoData::load(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	in >> m;
	in >> n;
	in >> pathstarts;
	in >> pathends;
	in >> pathedges;
	in >> A;
	in >> y;
	in >> xmeasured;
	in >> tsMin;
	in >> tsMax;

	return true;
}

bool TomoData::save(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qDebug() << __FILE__ << __LINE__ << "Failed to open file:" << fileName;
		return false;
	}
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_0);

	out << m;
	out << n;
	out << pathstarts;
	out << pathends;
	out << pathedges;
	out << A;
	out << y;
	out << xmeasured;
	out << tsMin;
	out << tsMax;

	return true;
}
