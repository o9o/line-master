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

#include <QtNetwork>

#include "dialog.h"

static qint64 PayloadSize = 100 * 1024; // bytes

quint64 get_current_time()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ((quint64)ts.tv_sec) * 1000ULL * 1000ULL * 1000ULL + ((quint64)ts.tv_nsec);
}

Dialog::Dialog(QString serverAddress, int serverPort, int transferSizeMB, bool startStop) {
	//qDebug() << __FUNCTION__;
	this->serverAddress = serverAddress;
	this->serverPort = serverPort;
	this->TotalBytes = transferSizeMB * 1024 * 1024;
	this->startStop = startStop;

	if (this->startStop) {
		qDebug() << QString("Start-stop mode enabled");
		onOffState = true;
		timeLeft = 0;
	}

	srand(qHash(serverAddress) + serverPort + get_current_time());

	connect(&tcpServer, SIGNAL(newConnection()), this, SLOT(acceptConnection()));

	connect(&tcpClient, SIGNAL(connected()), this, SLOT(startTransfer()));
	connect(&tcpClient, SIGNAL(bytesWritten(qint64)), this, SLOT(clientBytesWritten(qint64)));
	connect(&tcpClient, SIGNAL(readyRead()), this, SLOT(clientReadyRead()));
	connect(&tcpClient, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
	connect(&tcpClient, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(displayError(QAbstractSocket::SocketError)));
}

void Dialog::startClient() {
	//qDebug() << __FUNCTION__;
	bytesWritten = 0;
	bytesReceived = 0;
	mode = "client";

	tcpClient.connectToHost(QHostAddress(serverAddress), serverPort);
	qDebug() << QString("Connecting to %1:%2").arg(QHostAddress(serverAddress).toString(), QString::number(serverPort));
}

void Dialog::startServer() {
	//qDebug() << __FUNCTION__;
	bytesWritten = 0;
	bytesReceived = 0;
	mode = "server";

    while (!tcpServer.isListening() && !tcpServer.listen(QHostAddress::Any, serverPort)) {
        qDebug() << QString("Unable to start server on %1:%2: %3.")
                    .arg(serverAddress)
                    .arg(serverPort)
                    .arg(tcpServer.errorString());
		exit(1);
	}
	qDebug() << QString("Listening on port %1").arg(serverPort);
}

void Dialog::acceptConnection()
{
	//qDebug() << __FUNCTION__;
	tcpServerConnection = tcpServer.nextPendingConnection();
	connect(tcpServerConnection, SIGNAL(readyRead()), this, SLOT(serverReadyRead()));
	connect(tcpServerConnection, SIGNAL(disconnected()), this, SLOT(serverDisconnected()));
	connect(tcpServerConnection, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(displayError(QAbstractSocket::SocketError)));

	qDebug() << "Accepted connection";
	tcpServer.close();

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
	bytesPerTick = 0;
	timer->start(1000);
}

void Dialog::startTransfer()
{
	//qDebug() << __FUNCTION__;
	// called when the TCP client connected to the server
	qDebug() << "Client connected";

	QByteArray buffer;
	buffer.append((const char*)&TotalBytes, sizeof(TotalBytes));

	bytesToWrite = TotalBytes;

	while (!buffer.isEmpty()) {
		qint64 written = tcpClient.write(buffer);
		if (written < 0) {
			qDebug() << "Cannot write to socket";
			exit(1);
		}
		buffer = buffer.mid(written);
		bytesToWrite -= written;
	}

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
	bytesPerTick = 0;
	timer->start(1000);
}

void Dialog::tick()
{
	printf("%s\n", QString("Rate: %1 bytes/s").arg(bytesPerTick).toAscii().data());
	fflush(stdout);
	bytesPerTick = 0;

	if (mode == "client" && startStop) {
		timeLeft--;
		if (timeLeft <= 0) {
			timeLeft = 1 + rand() % 7;
			onOffState = !onOffState;
			qDebug() << QString("Start-stop: switching to state %1 for %2 seconds").arg(onOffState ? "Start" : "Stop").arg(timeLeft);
			if (onOffState)
				clientBytesWritten(0);
		}
	}
}

void Dialog::serverReadyRead()
{
	//qDebug() << __FUNCTION__;
	bool getlength = bytesReceived < (qint64)sizeof(TotalBytes);
	bytesReceived += tcpServerConnection->bytesAvailable();
	bytesPerTick += tcpServerConnection->bytesAvailable();
	QByteArray buffer = tcpServerConnection->readAll();

	if (getlength && buffer.length() >= (qint64)sizeof(TotalBytes)) {
		memcpy(&TotalBytes, buffer.data(), sizeof(TotalBytes));
		// qDebug() << QString("Server must receive %1MB").arg(TotalBytes / (1024 * 1024));
	}

	//qDebug() << QString("Server received %1MB").arg(bytesReceived / (1024 * 1024));

	if (bytesReceived == TotalBytes && TotalBytes > 0) {
		qDebug() << QString("Transfer finished");
		tcpServerConnection->write("OK");
	}
}

void Dialog::clientBytesWritten(qint64 numBytes)
{
	//qDebug() << __FUNCTION__;
	// callen when the TCP client has written some bytes
	bytesWritten += numBytes;
	bytesPerTick += numBytes;

	if (startStop && !onOffState) {
		return;
	}

	// only write more if not finished and when the Qt write buffer is below a certain size.
	if ((bytesToWrite > 0 || TotalBytes <= 0) && tcpClient.bytesToWrite() <= 1.5*PayloadSize)
		bytesToWrite -= tcpClient.write(QByteArray(TotalBytes <= 0 ? PayloadSize : qMin(bytesToWrite, PayloadSize), '@'));

	//qDebug() << QString("Client sent %1MB bytes left to write %2").arg(bytesWritten / (1024 * 1024)).arg(bytesToWrite);
}

void Dialog::clientReadyRead()
{
	//qDebug() << __FUNCTION__;
	if (tcpClient.bytesAvailable() >= 2) {
		QString reply = QString(tcpClient.readAll());
		if (reply == "OK") {
			qDebug() << QString("Transfer finished");
			tcpClient.close();
			QCoreApplication::processEvents();
			exit(0);
		} else {
			qDebug() << QString("Uninteligible message from server") << reply;
			tcpClient.close();
			QCoreApplication::processEvents();
			exit(1);
		}
	}
}

void Dialog::clientDisconnected()
{
	//qDebug() << __FUNCTION__;
	qDebug() << QString("Client disconnected");
	tcpClient.close();
	QCoreApplication::processEvents();
	exit(0);
}

void Dialog::serverDisconnected()
{
	//qDebug() << __FUNCTION__;
	qDebug() << QString("Server disconnected");
	tcpServerConnection->close();
	QCoreApplication::processEvents();
	exit(0);
}

void Dialog::displayError(QAbstractSocket::SocketError socketError)
{
	//qDebug() << __FUNCTION__;
	if (socketError == QTcpSocket::RemoteHostClosedError)
		return;

	qDebug() << QString("Network error: %1.").arg(tcpClient.errorString());

	tcpClient.close();
	tcpServer.close();

	exit(1);
}
