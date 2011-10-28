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

#include <QtGui/QApplication>
#include "mainwindow.h"

#include "bgp.h"
#include "convexhull.h"

#include <QtXml>

//#define TEST_IGP
//#define TEST_BGP

int main(int argc, char *argv[])
{
	QApplication::setGraphicsSystem("raster");

	QApplication a(argc, argv);

	QFile cssFile(":/css/style.css");
	cssFile.open(QIODevice::ReadOnly);
	QTextStream in(&cssFile);
	QString css = in.readAll();
	cssFile.close();
	a.setStyleSheet(css);

	srand(time(NULL));

#if defined(TEST_IGP)
	Bgp::testIGP();
#elif defined(TEST_BGP)
	Bgp::testBGP();
#else
	MainWindow w;
	w.showMaximized();
	return a.exec();
#endif

	return 0;
}
