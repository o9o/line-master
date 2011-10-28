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

#include "remoteprocessssh.h"

RemoteProcessSsh::RemoteProcessSsh()
{
	childrenKeys = QSet<QString>();
	childCount = 0;
}

RemoteProcessSsh::~RemoteProcessSsh()
{
	if (ssh.state() == QProcess::NotRunning)
		return;
	disconnect();
}

bool RemoteProcessSsh::connect(QString host)
{
	bool result;
	ssh.setProcessChannelMode(QProcess::SeparateChannels);
	ssh.setReadChannel(QProcess::StandardOutput);
	ssh.start("ssh", QStringList() << QString("root@%1").arg(host));
	result = ssh.waitForStarted(10000);

//	QByteArray motd;
//	while ((motd = ssh.readAllStandardOutput()).length() == 0)
//		ssh.waitForReadyRead(100);
//	qDebug() << "motd" << motd;
	return result;
}

void RemoteProcessSsh::disconnect()
{
	ssh.terminate();
	ssh.waitForFinished(10000);
}

QString RemoteProcessSsh::startProcess(QString program, QStringList args)
{
	qDebug() << __FILE__ << __FUNCTION__;
	QString key = QString("subproc_%1").arg(childCount);

	QString cmdLine = program + ' ' + args.join(" ");

	QString command = QString("rm %1.* ; (%2 1>%1.out 2>%1.err & pid=$! ; echo $pid >%1.pid ; wait $pid ; echo $? > %1.done ) &").arg(key, cmdLine);
	if (!sendCommand(command)) {
		return QString();
	}

	childrenKeys << key;
	childCount++;

	return key;
}

bool RemoteProcessSsh::isProcessRunning(QString key)
{
	qDebug() << __FILE__ << __FUNCTION__;
	if (!childrenKeys.contains(key)) {
		qDebug() << __FILE__ << __LINE__ << "No such process" << key;
		return false;
	}

	// flush current output
	flush();

	// send command
	QString command = QString("[ -f %1.done ] && echo Y || echo N").arg(key);
	if (!sendCommand(command)) {
		return false;
	}

	QString result = readLine().trimmed();
	if (result == "Y")
		return false;
	if (result == "N")
		return true;

	qDebug() << __FILE__ << __LINE__ << "problem, result is:" << result;
	return false;
}

bool RemoteProcessSsh::signalProcess(QString key, int sigNo)
{
	qDebug() << __FILE__ << __FUNCTION__;
	if (!childrenKeys.contains(key)) {
		qDebug() << __FILE__ << __LINE__ << "No such process" << key;
		return false;
	}

	// signal pid
	QString command = QString("kill -%2 `cat %1.pid`").arg(key).arg(sigNo);
	return sendCommand(command);
}

bool RemoteProcessSsh::waitForFinished(QString key, int timeout_s)
{
	qDebug() << __FILE__ << __FUNCTION__;
	while (timeout_s != 0) {
		if (!isProcessRunning(key))
			return true;
		sleep(1);
		if (timeout_s > 0)
			timeout_s--;
	}
	return !isProcessRunning(key);
}

QString RemoteProcessSsh::readAllStdout(QString key)
{
	qDebug() << __FILE__ << __FUNCTION__;
	// flush current output
	flush();

	// send command
	QString command = QString("cp %1.out %1.outtmp; wc -l %1.outtmp; cat %1.outtmp; rm %1.outtmp").arg(key);
	if (!sendCommand(command)) {
		return QString();
	}

	QString line = readLine();
	if (line.isEmpty())
		return QString();

	int lineCount = line.split(' ', QString::SkipEmptyParts).first().toInt();
	QString result;
	for (int i = 0; i < lineCount; i++) {
		line = readLine();
		result += line;
	}
	return result;
}

QString RemoteProcessSsh::readAllStderr(QString key)
{
	qDebug() << __FILE__ << __FUNCTION__;
	// flush current output
	flush();

	// send command
	QString command = QString("cp %1.err %1.errtmp; wc -l %1.errtmp; cat %1.errtmp; rm %1.errtmp").arg(key);
	if (!sendCommand(command)) {
		return QString();
	}

	QString line = readLine();
	if (line.isEmpty())
		return QString();

	int lineCount = line.split(' ', QString::SkipEmptyParts).first().toInt();
	QString result;
	for (int i = 0; i < lineCount; i++) {
		line = readLine();
		result += line;
	}
	return result;
}

bool RemoteProcessSsh::sendCommand(QString cmd)
{
	qDebug() << __FILE__ << __FUNCTION__ << cmd;
	if (!cmd.endsWith('\n'))
		cmd = cmd + '\n';
	// qDebug() << "WRITE:" << cmd;
	QByteArray data = cmd.toAscii();
	while (!data.isEmpty()) {
		qint64 writtenCount = ssh.write(data);
		if (writtenCount < 0) {
			qDebug() << __FILE__ << __LINE__ << "Could not write to ssh (broken pipe)";
			return false;
		}
		data = data.mid(writtenCount);
		bool ok = ssh.waitForBytesWritten(10000);
		if (!ok) {
			qDebug() << __FILE__ << __LINE__ << "Could not write to ssh (timeout)";
			return false;
		}
	}
	return true;
}

QString RemoteProcessSsh::readLine()
{
	qDebug() << __FILE__ << __FUNCTION__;
	int sleepTime_ms = 1000;
	int maxTries = 20;

	QByteArray line = QByteArray();
	for (int tries = 0; tries < maxTries; tries++) {
		line.append(ssh.readLine());
		if (!line.isEmpty()) {
			if (line.endsWith('\n'))
				break;
			tries = -1;
		}
		ssh.waitForReadyRead(sleepTime_ms);
	}

	if (line.isEmpty())
		qDebug() << __FILE__ << __LINE__ << "Could not read from ssh (timeout)";

	// qDebug() << "READ:" << line;

	return QString(line);
}

void RemoteProcessSsh::flush()
{
	ssh.waitForReadyRead(100);
	qDebug() << "FLUSH:" << ssh.readAllStandardOutput();
	qDebug() << "FLUSH:" << ssh.readAllStandardError();
}
