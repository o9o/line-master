/****************************************************************************
**
** Copyright (C) 2011 Ovidiu Mara
** Based on the fortuneclient example from the Qt SDK.
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QCoreApplication>

#include "dialog.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    argc--, argv++;

    if (argc < 1) {
	    qDebug() << "Usage:";
		qDebug() << "Client mode: tcptransfer client <serverIP> <serverPort> <transferSizeMB | 0> [--start-stop]";
		qDebug() << "Server mode: tcptransfer server <listenAddress> <serverPort>";
	    exit(1);
    }
    QString mode = argv[0];

    argc--, argv++;

    Dialog *dialog;
    if (mode == "client") {
	    if (argc < 3) {
		    qDebug() << "Wrong args";
		    exit(1);
	    }
	    QString serverAddress = argv[0];
	    int serverPort = QString(argv[1]).toInt();
	    int transferSizeMB = QString(argv[2]).toInt();
	    argc--, argv++;
	    argc--, argv++;
	    argc--, argv++;

	    bool startStop = false;
	    for (int i = 0; i < argc; i++) {
		    startStop = startStop || (QString(argv[i]) == "--start-stop");
	    }

	    dialog = new Dialog(serverAddress, serverPort, transferSizeMB, startStop);
	    dialog->startClient();
    } else if (mode == "server") {
	    if (argc < 2) {
		    qDebug() << "Wrong args";
		    exit(1);
	    }
	    QString serverAddress = argv[0];
	    int serverPort = QString(argv[1]).toInt();

	    dialog = new Dialog(serverAddress, serverPort, -1, false);
	    dialog->startServer();
    } else {
	    qDebug() << "Wrong args";
	    exit(1);
    }

    return app.exec();
}
