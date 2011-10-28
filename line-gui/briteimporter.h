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

#ifndef BRITEIMPORTER_H
#define BRITEIMPORTER_H

#include <QtCore>

#include "netgraph.h"

class BriteImporter : public QObject
{
	Q_OBJECT
public:
	explicit BriteImporter();
	bool import(QString fromFile, QString toFile);

signals:
	void logInfo(QString s);
	void logError(QString s);

public slots:

};

#endif // BRITEIMPORTER_H
