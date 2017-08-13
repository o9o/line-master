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

#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::on_btnGeneric_clicked()
{
	if (netGraph.fileName.isEmpty()) {
		QMessageBox::warning(this, "Error", "Save the topology first");
		return;
	}

	ui->txtBatch->clear();

	bool accepted = true;
	quint64 value = 0;
	if (ui->checkGenericTimeout->isChecked()) {
		updateTimeBox(ui->txtGenericTimeout, accepted, value);
		value /= 1000000000ULL;
	}

	if (!accepted) {
		doLogError(ui->txtBatch, "Invalid duration value.");
		return;
	}

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::doGenericSimulation, value);
	voidWatcher.setFuture(future);
}

void MainWindow::on_checkGenericTimeout_toggled(bool checked)
{
	ui->txtGenericTimeout->setEnabled(checked);
}

void MainWindow::on_txtGenericTimeout_textChanged(const QString &)
{
	bool accepted;
	quint64 value;
	updateTimeBox(ui->txtGenericTimeout, accepted, value);
}

void MainWindow::on_btnGenericStop_clicked()
{
	mustStop = true;
}

QString MainWindow::doGenericSimulation(int timeout)
{
	RemoteProcessSsh ssh;
	RemoteProcessSsh sshCore;

	if (!ssh.connect(getClientHostname(0))) {
		emit logError(ui->txtBatch, "Connect to client failed");
		return QString();
	}

	if (!sshCore.connect(getCoreHostname())) {
		emit logError(ui->txtBatch, "Connect to core failed");
		return QString();
	}

	QString testId = getGraphName() + QDateTime::currentDateTime().toString(".yyyy.MM.dd.hh.mm.ss");

#ifndef DEGUB_EMULATOR
	QString emulatorCmd = QString("line-router %1.graph %2").arg(getGraphName()).arg(testId);
#else
	QString emulatorCmd = QString("sleep 1000000").arg(getGraphName()).arg(testId);
#endif
	QString emulatorKey = sshCore.startProcess(emulatorCmd);

	// start servers
	emit logInformation(ui->txtBatch, QString("Starting servers..."));
	int tcpPort = 8000;
	int udpPort = 4000;
	foreach (NetGraphConnection c, netGraph.connections) {
		if (c.type == "TCP") {
			QString serverCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; tcptransfer server 0.0.0.0 %2").arg(netGraph.nodes[c.dest].ip()).arg(tcpPort);
			QString key = ssh.startProcess(serverCmd);

			netGraph.connections[c.index].port = tcpPort;
			netGraph.connections[c.index].serverKey = key;

			tcpPort++;
		} else if (c.type == "UDP") {
			QString serverCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; udpping -l -p %3 --sink-only").arg(netGraph.nodes[c.dest].ip()).arg(udpPort);
			emit logInformation(ui->txtBatch, serverCmd);
			QString key = ssh.startProcess(serverCmd);

			netGraph.connections[c.index].port = udpPort;
			netGraph.connections[c.index].serverKey = key;

			udpPort++;
		} else if (c.type.startsWith("TCPx")) {
			QString s = c.type.split('x').at(1);
			QStringList options = s.split(' ', QString::SkipEmptyParts);
			int multiplier = options.takeFirst().toInt();

			for (;multiplier > 0; multiplier--) {
				QString serverCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; tcptransfer server 0.0.0.0 %3").arg(netGraph.nodes[c.dest].ip()).arg(tcpPort);
				QString key = ssh.startProcess(serverCmd);

				netGraph.connections[c.index].ports << tcpPort;
				netGraph.connections[c.index].serverKeys << key;
				netGraph.connections[c.index].options = options;

				tcpPort++;
			}
		} else if (c.type.startsWith("cmd")) {
			QString processCmd = c.serverCmd;
			QString serverCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; %2").arg(netGraph.nodes[c.dest].ip()).arg(processCmd);
			emit logInformation(ui->txtBatch, serverCmd);
			QString key = ssh.startProcess(serverCmd);

			netGraph.connections[c.index].port = 0;
			netGraph.connections[c.index].serverKey = key;
		}
	}
	sleep(2);
	emit logInformation(ui->txtBatch, QString("Starting clients..."));
	foreach (NetGraphConnection c, netGraph.connections) {
		if (c.type == "TCP") {
			QString clientCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; tcptransfer client %2 %3 %4").arg(netGraph.nodes[c.source].ip()).arg(netGraph.nodes[c.dest].ip()).arg(c.port).arg(0);
			QString key = ssh.startProcess(clientCmd);

			netGraph.connections[c.index].clientKey = key;
		} else if (c.type == "UDP") {
			QString clientCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; udpping -a %2 -p %3 -i 1000 -c 0 -s 1458 -b 1 -v 0 --source-only --randomize-interval").arg(netGraph.nodes[c.source].ip()).arg(netGraph.nodes[c.dest].ip()).arg(c.port);
			emit logInformation(ui->txtBatch, clientCmd);
			QString key = ssh.startProcess(clientCmd);

			netGraph.connections[c.index].clientKey = key;
		} else if (c.type.startsWith("TCPx")) {
			foreach (int port, c.ports) {
				bool startStop = c.options.contains("start-stop");

				QString clientCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; tcptransfer client %2 %3 %4 %5").arg(netGraph.nodes[c.source].ip()).arg(netGraph.nodes[c.dest].ip()).arg(port).arg(0).arg(startStop ? "--start-stop" : "");
				QString key = ssh.startProcess(clientCmd);

				netGraph.connections[c.index].clientKeys << key;
			}
		} else if (c.type.startsWith("cmd")) {
			QString processCmd = c.clientCmd;
			QString clientCmd = QString("export MN_DEBUG=" MN_DEBUG "; export LD_PRELOAD=/usr/lib/libipaddr.so; export SRCIP=%1; %2").arg(netGraph.nodes[c.source].ip()).arg(processCmd);
			emit logInformation(ui->txtBatch, clientCmd);
			QString key = ssh.startProcess(clientCmd);

			netGraph.connections[c.index].clientKey = key;
		}
	}

	if (timeout) {
		emit logInformation(ui->txtBatch, QString("Let it run for %1s...").arg(timeout));
		for (int i = 0; i < timeout; i++) {
			sleep(1);
			if (mustStop)
				break;
		}
	} else {
		// wait for clients
		emit logInformation(ui->txtBatch, QString("Waiting for client apps to finish..."));

		while (1) {
			for (int i = 0; i < 5; i++) {
				sleep(1);
				if (mustStop)
					break;
			}
			if (mustStop)
				break;
			int running = 0;
			foreach (NetGraphConnection c, netGraph.connections) {
				// clientKeys -> multiple apps; clientKey -> one app
				if (!c.clientKeys.isEmpty()) {
					foreach (QString key, c.clientKeys) {
						running += (!ssh.waitForFinished(key, 1)) ? 1 : 0;
					}
				} else {
					running += (!ssh.waitForFinished(c.clientKey, 1)) ? 1 : 0;
				}
			}
			if (running == 0)
				break;
			emit logInformation(ui->txtBatch, QString("Apps running: %1").arg(running));
		}
	}

    // stop emulator
    emit logInformation(ui->txtBatch, QString("Stopping emulator (%1)").arg(emulatorKey));
    if (!sshCore.signalProcess(emulatorKey, 2)) {
        emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(emulatorKey));
    }
    if (!sshCore.waitForFinished(emulatorKey, -1)) {
        emit logError(ui->txtBatch, QString("Error: %1 wait failed").arg(emulatorKey));
    }
    emit logInformation(ui->txtBatch, QString("Emulator output:"));
    emit logOutput(ui->txtBatch, sshCore.readAllStdout(emulatorKey));
    emit logOutput(ui->txtBatch, sshCore.readAllStderr(emulatorKey));

    // save emulator output
    {
        QString key = sshCore.startProcess("cp", QStringList() << QString("%1.out").arg(emulatorKey) << QString("%1/emulator.out").arg(testId));
        if (!sshCore.waitForFinished(key, -1))
            emit logError(ui->txtBatch, QString("Error: could not copy emulator stdout"));
        key = sshCore.startProcess("cp", QStringList() << QString("%1.err").arg(emulatorKey) << QString("%1/emulator.err").arg(testId));
        if (!sshCore.waitForFinished(key, -1))
            emit logError(ui->txtBatch, QString("Error: could not copy emulator stderr"));
    }

	// we need to kill client apps if (1) we are running with a timeout or (2) if the stop button was pressed
	// we need to kill server apps in any case
	// but there is no big problem if we try to kill them all anyways so...
	{
		// stop all apps
		emit logInformation(ui->txtBatch, QString("Kill apps..."));
		foreach (NetGraphConnection c, netGraph.connections) {
			if (!c.clientKeys.isEmpty()) {
				foreach (QString key, c.clientKeys) {
					if (!ssh.signalProcess(key, 2)) {
						emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(key));
					}

                    if (!ssh.waitForFinished(key, 0)) {
						emit logError(ui->txtBatch, QString("Error: %1 wait failed after SIGINT; trying SIGKILL").arg(key));
					}

					if (!ssh.signalProcess(key, 9)) {
						emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(key));
					}

                    if (!ssh.waitForFinished(key, 0)) {
						emit logError(ui->txtBatch, QString("Error: %1 wait failed").arg(key));
					}
				}
			} else {
				if (!ssh.signalProcess(c.clientKey, 2)) {
					emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(c.clientKey));
				}

                if (!ssh.waitForFinished(c.clientKey, 1)) {
					emit logError(ui->txtBatch, QString("Error: %1 wait failed after SIGINT; trying SIGKILL").arg(c.clientKey));
				}

				if (!ssh.signalProcess(c.clientKey, 9)) {
					emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(c.clientKey));
				}

                if (!ssh.waitForFinished(c.clientKey, 1)) {
					emit logError(ui->txtBatch, QString("Error: %1 wait failed").arg(c.clientKey));
				}
			}

			if (!c.serverKeys.isEmpty()) {
				foreach (QString key, c.serverKeys) {
					if (!ssh.signalProcess(key, 2)) {
						emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(key));
					}

                    if (!ssh.waitForFinished(key, 0)) {
						emit logError(ui->txtBatch, QString("Error: %1 wait failed after SIGINT; trying SIGKILL").arg(key));
					}

					if (!ssh.signalProcess(key, 9)) {
						emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(key));
					}

                    if (!ssh.waitForFinished(key, 0)) {
						emit logError(ui->txtBatch, QString("Error: %1 wait failed").arg(key));
					}
				}
			} else {
				if (!ssh.signalProcess(c.serverKey, 2)) {
					emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(c.serverKey));
				}

                if (!ssh.waitForFinished(c.serverKey, 0)) {
					emit logError(ui->txtBatch, QString("Error: %1 wait failed after SIGINT; trying SIGKILL").arg(c.serverKey));
				}

				if (!ssh.signalProcess(c.serverKey, 9)) {
					emit logError(ui->txtBatch, QString("Error: %1 signal failed").arg(c.serverKey));
				}

                if (!ssh.waitForFinished(c.serverKey, 0)) {
					emit logError(ui->txtBatch, QString("Error: %1 wait failed").arg(c.serverKey));
				}
			}
		}
	}

	// show output from apps
	emit logInformation(ui->txtBatch, QString("App output"));
	{
		QDir dir(".");
		dir.mkdir(testId);
	}
	foreach (NetGraphConnection c, netGraph.connections) {
		if (c.type == "UDP") {
			//TODO is this still used somewhere?
			//Parse CBR output
			{
				QString output = ssh.readAllStdout(c.clientKey);
				QStringList lines = output.split('\n', QString::SkipEmptyParts);

				double packetsSent = 0;
				foreach (QString line, lines) {
					if (line.startsWith("Packets sent:")) {
						packetsSent = line.split(' ', QString::SkipEmptyParts).at(2).toDouble();
					}
				}

				output = ssh.readAllStdout(c.serverKey);
				lines = output.split('\n', QString::SkipEmptyParts);

				double packetsRecv = 0;
				foreach (QString line, lines) {
					if (line.startsWith("Packets received:")) {
						packetsRecv = line.split(' ', QString::SkipEmptyParts).at(2).toDouble();
					}
				}
				double loss = 1.0 - packetsRecv / packetsSent;
				QString result = QString("probes sent = %1\nprobes received = %2\nsuccess rate = %3\n").arg((int)packetsSent).arg((int)packetsRecv).arg(1-loss);
				saveFile(QString("%1/probe_loss_path_%2_%3.txt").arg(testId).arg(c.source).arg(c.dest), result);
			}
		}
	}
	//show and save the output
	foreach (NetGraphConnection c, netGraph.connections) {
		QString fileNamePrefix = QString("%1/connection_%2-%3_%4").arg(testId).arg(c.source).arg(c.dest).arg(c.index);
		if (!c.clientKeys.isEmpty()) {
			int multiIndex = 0;
			foreach (QString key, c.clientKeys) {
				QString fileName = fileNamePrefix + QString("_%1_client").arg(multiIndex);
				QString result;
                // emit logInformation(ui->txtBatch, QString("STDOUT client %1:").arg(key));
				result = ssh.readAllStdout(key);
                // emit logOutput(ui->txtBatch, result);
				saveFile(fileName + ".out", result);
                // emit logInformation(ui->txtBatch, QString("STDERR client %1:").arg(key));
				result = ssh.readAllStderr(key);
                // emit logOutput(ui->txtBatch, result);
				saveFile(fileName + ".err", result);
				multiIndex++;
			}
		} else {
			QString fileName = fileNamePrefix + QString("_0_client");
			QString result;
//			emit logInformation(ui->txtBatch, QString("STDOUT client %1:").arg(c.clientKey));
			result = ssh.readAllStdout(c.clientKey);
//			emit logOutput(ui->txtBatch, result);
			saveFile(fileName + ".out", result);
//			emit logInformation(ui->txtBatch, QString("STDERR client %1:").arg(c.clientKey));
			result = ssh.readAllStderr(c.clientKey);
//			emit logOutput(ui->txtBatch, result);
			saveFile(fileName + ".err", result);
		}

		if (!c.serverKeys.isEmpty()) {
			int multiIndex = 0;
			foreach (QString key, c.serverKeys) {
				QString fileName = fileNamePrefix + QString("_%1_server").arg(multiIndex);
				QString result;
//				emit logInformation(ui->txtBatch, QString("STDOUT server %1:").arg(key));
				result = ssh.readAllStdout(key);
//				emit logOutput(ui->txtBatch, result);
				saveFile(fileName + ".out", result);
//				emit logInformation(ui->txtBatch, QString("STDERR server %1:").arg(key));
				result = ssh.readAllStderr(key);
//				emit logOutput(ui->txtBatch, result);
				saveFile(fileName + ".err", result);
				multiIndex++;
			}
		} else {
			QString fileName = fileNamePrefix + QString("_0_server");
			QString result;
//			emit logInformation(ui->txtBatch, QString("STDOUT server %1:").arg(c.serverKey));
			result = ssh.readAllStdout(c.serverKey);
//			emit logOutput(ui->txtBatch, result);
			saveFile(fileName + ".out", result);
//			emit logInformation(ui->txtBatch, QString("STDERR server %1:").arg(c.serverKey));
			result = ssh.readAllStderr(c.serverKey);
//			emit logOutput(ui->txtBatch, result);
			saveFile(fileName + ".err", result);
		}
	}

	// zip everything
	{
		QString key = sshCore.startProcess("zip", QStringList() << QString("-r") << QString("%1.zip").arg(testId) << QString("%1").arg(testId));
		if (!sshCore.waitForFinished(key, -1))
			emit logError(ui->txtBatch, QString("Error: could not zip emulator output folder"));
	}

	// download it
	{
		QString description = "Downloading the emulator output";
		QString command = "scp";
		QStringList args = QStringList() << QString("root@%1:~/%2.zip").arg(getCoreHostname()).arg(testId) << QString(".");
		if (!runCommand(ui->txtBatch, command, args, description)) {
			emit logError(ui->txtBatch, "Aborted.");
		}

		description = "Extract the emulator output";
		command = "unzip";
		args = QStringList() << QString("%1.zip").arg(testId);
		if (!runCommand(ui->txtBatch, command, args, description)) {
			emit logError(ui->txtBatch, "Aborted.");
		}
	}

	emit saveImage(testId + "/" + testId + ".png");

	emit reloadSimulationList();

	emit logInformation(ui->txtBatch, QString("Test done."));
	return testId;
}

void MainWindow::generateSimpleTopology(int pairs)
{
	// generate topology
	emit logInformation(ui->txtBatch, QString("Generating topology with 2 hosts"));

	double bw_KBps = simple_bw_KBps;
	int delay_ms = simple_delay_ms;
	int queueSize = simple_bufferbloat_factor * NetGraph::optimalQueueLength(bw_KBps, delay_ms);
	simple_queueSize = queueSize;

	netGraph.clear();
	int gateway = netGraph.addNode(NETGRAPH_NODE_GATEWAY, QPointF(0, 0));
	const double radiusMin = 300;
	double radius = pairs/2.0 * 3.0 * NETGRAPH_NODE_RADIUS/M_PI * 3.0;
	if (radius < radiusMin)
		radius = radiusMin;
	// minimun 2.0 * NETGRAPH_NODE_RADIUS / radius, anything higher adds spacing
	double alpha = 4.0 * NETGRAPH_NODE_RADIUS / radius;
	for (int pairIndex = 0; pairIndex < pairs; pairIndex++) {
		double angle = M_PI - (pairs / 2) * alpha + pairIndex * alpha + (1 - pairs % 2) * alpha / 2.0;
		int first = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(radius * cos(angle), -radius * sin(angle)));
		angle = M_PI - angle;
		int second = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(radius * cos(angle), -radius * sin(angle)));
		netGraph.addEdge(first, gateway, bw_KBps, delay_ms, 0, queueSize);
		netGraph.addEdge(gateway, first, bw_KBps, delay_ms, 0, queueSize);
		netGraph.addEdge(gateway, second, bw_KBps, delay_ms, 0, queueSize);
		netGraph.addEdge(second, gateway, bw_KBps, delay_ms, 0, queueSize);
	}
	netGraph.setFileName(QString("simple_%1KBps_%2ms_q%3.graph").arg(bw_KBps).arg(delay_ms).arg(queueSize));
	netGraph.saveToFile();
	emit reloadScene();
	emit saveGraph();
	emit logSuccess(ui->txtBatch, "OK.");
}

void MainWindow::on_spinBW_valueChanged(int arg1)
{
	simple_bw_KBps = arg1;
}

void MainWindow::on_spinPathDelay_valueChanged(int arg1)
{
	simple_delay_ms = arg1 / 2;
}

void MainWindow::on_checkJitter_toggled(bool checked)
{
	simple_jitter = checked;
}

void MainWindow::on_doubleSpinBox_valueChanged(double arg1)
{
	simple_bufferbloat_factor = arg1;
}

//#define DEGUB_EMULATOR

void MainWindow::on_btn4x10_clicked()
{
	netGraph.connections.clear();
	emit logInformation(ui->txtBatch, "Adding connections");
	// Generate connections
	netGraph.addConnection(NetGraphConnection(11, 22, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(27, 24, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(4, 8, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(26, 24, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(15, 16, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(26, 19, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(6, 11, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(6, 8, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(12, 30, "TCP", ""));
	netGraph.addConnection(NetGraphConnection(20, 6, "TCP", ""));
}

void MainWindow::on_btnY_clicked()
{
	// generate Y topology
	// generate topology
	emit logInformation(ui->txtBatch, QString("Generating Y topology"));

	double bw_KBps = simple_bw_KBps;
	int delay_ms = simple_delay_ms;
	int queueSize = simple_bufferbloat_factor * NetGraph::optimalQueueLength(bw_KBps, delay_ms);
	simple_queueSize = queueSize;

	netGraph.clear();
	int gateway = netGraph.addNode(NETGRAPH_NODE_GATEWAY, QPointF(0, 0));
	int left = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(-300, 0));
	int right1 = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(300, 100));
	int right2 = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(300, -100));

	netGraph.addEdge(left, gateway, bw_KBps, delay_ms, 0, queueSize);
	netGraph.addEdge(gateway, left, bw_KBps, delay_ms, 0, queueSize);
	netGraph.addEdge(gateway, right1, bw_KBps, delay_ms, 0, queueSize);
	netGraph.addEdge(right1, gateway, bw_KBps, delay_ms, 0, queueSize);
	netGraph.addEdge(gateway, right2, bw_KBps, delay_ms, 0, queueSize);
	netGraph.addEdge(right2, gateway, bw_KBps, delay_ms, 0, queueSize);

	for (int e = 0; e < netGraph.edges.count(); e++) {
		netGraph.edges[e].recordFullTimeline = false;
		netGraph.edges[e].recordSampledTimeline = true;
		netGraph.edges[e].timelineSamplingPeriod = 100 * USEC_TO_NSEC;
	}

	netGraph.setFileName(QString("Y_%1KBps_%2ms_q%3.graph").arg(bw_KBps).arg(delay_ms).arg(queueSize));

	netGraph.addConnection(NetGraphConnection(left, right1, "TCPx10", ""));
	netGraph.addConnection(NetGraphConnection(left, right2, "TCPx10", ""));

	netGraph.saveToFile();
	emit reloadScene();
	emit saveGraph();
	emit logSuccess(ui->txtBatch, "OK.");
}

void MainWindow::on_btnAdd10TCP_clicked()
{
	for (int i = 0; i < 10; i++) {
		int n1, n2;
		n1 = n2 = -1;
		while (true) {
			n1 = rand() % netGraph.nodes.count();
			if (netGraph.nodes[n1].nodeType != NETGRAPH_NODE_HOST)
				continue;
			n2 = rand() % netGraph.nodes.count();
			if (n2 == n1)
				continue;
			if (netGraph.nodes[n2].nodeType != NETGRAPH_NODE_HOST)
				continue;
			break;
		}
		netGraph.addConnection(NetGraphConnection(n1, n2, "TCP", ""));
	}
}
