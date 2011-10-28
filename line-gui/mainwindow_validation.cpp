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

#ifdef ENABLE_VALIDATION

#include "util.h"

#define DEBUG_VALIDATION 0

/******************* Delay validation *********************/

void MainWindow::on_btnStartDelayValidation_clicked()
{
	ui->txtValidation->clear();

	// Make sure we saved
	if (!on_actionSave_triggered())
		return;

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::delayValidation);
	voidWatcher.setFuture(future);
}

void MainWindow::delayValidation()
{
	// for each path
	//     for frame size in 100, 200, ..., 1000 bytes
	//         send a probe, measure one-way delay
	//         don't go over 10% max path rate
	//         wait for 5*RTT
	// plot(frameSize, delay) measured and theoretical

	emit logInformation(ui->txtValidation, "Delay measurements");

	// one plot for each path
	QList<QOPlot> *plots = new QList<QOPlot>;
	foreach (NetGraphNode src, netGraph.getHostNodes()) {
		foreach (NetGraphNode dst, netGraph.getHostNodes()) {
			if (mustStop)
				break;
			if (src.index >= dst.index)
				continue;
			emit logInformation(ui->txtValidation, QString("Delay for path: %1 - %2").arg(src.index).arg(dst.index));

			// add a dummy connection so that we have a path and thus a route
			netGraph.addConnection(NetGraphConnection(src.index, dst.index, "", ""));
			computeRoutes();

			NetGraphPath p = netGraph.pathByNodeIndex(src.index, dst.index);
			emit logText(ui->txtValidation, QString("Forward path: %1").arg(p.toString()));

			QList<PathDelayMeasurement> pathMeasuredDelay;
			QList<PathDelayMeasurement> pathTheoreticalDelay;
			NetGraphPath pReverse = netGraph.pathByNodeIndex(p.dest, p.source);
			emit logText(ui->txtValidation, QString("Reverse path: %1").arg(pReverse.toString()));
			double ratekBps = 0.1 * p.bandwidth();

#if DEBUG_VALIDATION
			for (int frameSize = 100; frameSize <= 100; frameSize += 100) {
#else
			for (int frameSize = 100; frameSize <= 1000; frameSize += 100) {
#endif
				if (mustStop)
					break;
				QList<PathDelayMeasurement> runData;
				long long timeout_us = 5000LL * (p.computeFwdDelay(frameSize) + pReverse.computeFwdDelay(frameSize));

				measureDelay(ui->txtValidation, src, dst, frameSize, ratekBps, timeout_us, runData);

				QString result;
				result += "Measured forward delay: ";
				foreach (PathDelayMeasurement item, runData) {
					result += QString::number(item.fwdDelay) + " ";
				}
				emit logText(ui->txtValidation, result);

				emit logText(ui->txtValidation, "Theoretical forward delay: " + QString::number(p.computeFwdDelay(frameSize)));

				pathMeasuredDelay.append(runData);

				PathDelayMeasurement theoreticalDelay;
				theoreticalDelay.fwdDelay = p.computeFwdDelay(frameSize);
				theoreticalDelay.backDelay = pReverse.computeFwdDelay(frameSize);
				theoreticalDelay.frameSize = frameSize;
				pathTheoreticalDelay << theoreticalDelay;
			}

			// make delay plot for path
			QOPlot plotFwd;
			plotFwd.title = QString("Delay as a function of frame size for path %1 - %2").arg(p.source).arg(p.dest);
			plotFwd.xlabel = "Frame size (bytes)";
			plotFwd.ylabel = "Delay (ms)";

			QOPlotScatterData *scatterFwdTheoretical = new QOPlotScatterData;
			foreach (PathDelayMeasurement item, pathTheoreticalDelay) {
				scatterFwdTheoretical->x.append(item.frameSize);
				scatterFwdTheoretical->y.append(item.fwdDelay);
			}
			scatterFwdTheoretical->pen.setColor(Qt::blue);
			scatterFwdTheoretical->legendLabel = "Theoretical";
			plotFwd.data << QSharedPointer<QOPlotData>(scatterFwdTheoretical);

			QOPlotScatterData *scatterFwdMeasured = new QOPlotScatterData;
			foreach (PathDelayMeasurement item, pathMeasuredDelay) {
				scatterFwdMeasured->x.append(item.frameSize);
				scatterFwdMeasured->y.append(item.fwdDelay);
			}
			scatterFwdMeasured->pen.setColor(Qt::red);
			scatterFwdMeasured->legendLabel = "Measured";
			plotFwd.data << QSharedPointer<QOPlotData>(scatterFwdMeasured);

			(*plots) << plotFwd;

			// another plot for the reverse path
			QOPlot plotBack;
			plotBack.title = QString("Delay as a function of frame size for path %1 - %2").arg(pReverse.source).arg(pReverse.dest);
			plotBack.xlabel = "Frame size (bytes)";
			plotBack.ylabel = "Delay (ms)";

			QOPlotScatterData *scatterBackTheoretical = new QOPlotScatterData;
			foreach (PathDelayMeasurement item, pathTheoreticalDelay) {
				scatterBackTheoretical->x.append(item.frameSize);
				scatterBackTheoretical->y.append(item.backDelay);
			}
			scatterBackTheoretical->pen.setColor(Qt::blue);
			scatterBackTheoretical->legendLabel = "Theoretical";
			scatterBackTheoretical->pointSymbol = "+";
			plotBack.data << QSharedPointer<QOPlotData>(scatterBackTheoretical);

			QOPlotScatterData *scatterBackMeasured = new QOPlotScatterData;
			foreach (PathDelayMeasurement item, pathMeasuredDelay) {
				scatterBackMeasured->x.append(item.frameSize);
				scatterBackMeasured->y.append(item.backDelay);
			}
			scatterBackMeasured->pen.setColor(Qt::red);
			scatterBackMeasured->legendLabel = "Measured";
			scatterBackMeasured->pointSymbol = "x";
			plotBack.data << QSharedPointer<QOPlotData>(scatterBackMeasured);

			(*plots) << plotBack;
		}
	}

	emit delayValidationFinished(plots);
	emit logSuccess(ui->txtValidation, "Finished");
	emit logText(ui->txtValidation, "Compare the theoretical and the measured data plots to decide whether the accuracy is acceptable.");
}

void MainWindow::doDelayValidationFinished(QList<QOPlot> *plots)
{
	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);
	accordion->addLabel("Delay validation plots");

	foreach (QOPlot plot, *plots) {
		QOPlotWidget *plotWidget = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		plotWidget->setPlot(plot);
		accordion->addWidget(plot.title, plotWidget);
	}

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
	emit tabResultsChanged();
}

void MainWindow::measureDelay(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
						int frame_size, double rate_kBps, long long timeout_us,
						QList<PathDelayMeasurement> &data)
{
	data.clear();
	long long interval_us = (long long)(0.5 + 1000.0 * frame_size / rate_kBps);
	QString serverCmd = QString("udpping -l");
	QString clientCmd = QString("udpping -a %1 -s %2 -i %3 -t %4").
						arg(dst.ip(), QString::number(frame_size - UDPPING_OVERHEAD), QString::number(interval_us), QString::number(timeout_us));

	netGraph.connections.clear();
	netGraph.addConnection(NetGraphConnection::cmdConnection(src.index, dst.index, serverCmd, clientCmd));
	deploy();

	QString testId = doGenericSimulation();
	QString clientOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_client.out").arg(testId).arg(src.index).arg(dst.index).arg(0), clientOutput);

	// Parse client output
	QStringList lines = clientOutput.split('\n', QString::SkipEmptyParts);
	PathDelayMeasurement item;
	item.frameSize = frame_size;
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Recv from")) {
			QString remoteIP = line.split(' ', QString::SkipEmptyParts).at(2);
			QString localIP = line.split(' ', QString::SkipEmptyParts).at(6);
			localIP = localIP.replace(";", "");
			int bytesReceived = line.split(' ', QString::SkipEmptyParts).at(9).toInt();
			if (remoteIP != dst.ip()) {
				emit logOutput(log, clientOutput);
				emit logError(log, "Bad server IP address");
				return;
			}
			if (localIP != src.ip()) {
				emit logOutput(log, clientOutput);
				emit logError(log, "Bad local IP address");
				return;
			}
			if (bytesReceived != frame_size - UDPPING_OVERHEAD) {
				emit logOutput(log, clientOutput);
				emit logError(log, "Bad frame size");
				return;
			}
		} else if (line.startsWith("FORW delay:")) {
			item.fwdDelay = line.split(' ', QString::SkipEmptyParts).at(2).toDouble();
		} else if (line.startsWith("BACK delay:")) {
			item.backDelay = line.split(' ', QString::SkipEmptyParts).at(2).toDouble();
			data << item;
		}
	}
}

/******************* Bernoulli loss validation *********************/

void MainWindow::on_btnStartLossBernValidation_clicked()
{
	ui->txtValidation->clear();

	// Make sure we saved
	if (!on_actionSave_triggered())
		return;

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::lossBernoulliValidation);
	voidWatcher.setFuture(future);
}

void MainWindow::lossBernoulliValidation()
{
	// for each path send 1,000 probes at 50% link rate (take the bottleneck)
	// show how many time out (in %)

	emit logInformation(ui->txtValidation, "Bernoulli loss measurements");
	foreach (NetGraphNode src, netGraph.getHostNodes()) {
		foreach (NetGraphNode dst, netGraph.getHostNodes()) {
			if (mustStop)
				break;
			if (src.index == dst.index)
				continue;
			emit logInformation(ui->txtValidation, QString("Bernoulli loss test for path: %1 - %2").arg(src.index).arg(dst.index));

			// add a dummy connection so that we have a path and thus a route
			netGraph.addConnection(NetGraphConnection(src.index, dst.index, "", ""));
			computeRoutes();
			NetGraphPath p = netGraph.pathByNodeIndex(src.index, dst.index);

			double loss;
			double ratekBps = 0.5 * p.bandwidth();
			int frame_size = 100;
			measureBernoulliLoss(ui->txtValidation, src, dst, ratekBps, frame_size, loss);

			QString line = QString("Result: %1% loss").arg(loss * 100.0);
			emit logText(ui->txtValidation, line);
			line = QString("Theoretical loss: %1%").arg(p.lossBernoulli() *  100.0);
			emit logText(ui->txtValidation, line);
		}
	}

	emit logSuccess(ui->txtValidation, "Finished");
	emit logText(ui->txtValidation, "Compare the values given above to decide whether the accuracy is acceptable.");
}

void MainWindow::measureBernoulliLoss(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
							   double ratekBps, int frame_size, double &loss)
{
	const int probeCount = 1000;
	long long interval_us = (long long)(0.5 + 1000.0 * frame_size / ratekBps);
	QString serverCmd = QString("udpping -l --sink-only");
	QString clientCmd = QString("udpping -a %1 -c %2 -i %3 -s %4 --source-only").
						arg(dst.ip()).arg(probeCount).arg(interval_us).arg(frame_size);

	netGraph.connections.clear();
	netGraph.addConnection(NetGraphConnection::cmdConnection(src.index, dst.index, serverCmd, clientCmd));
	deploy();

	QString testId = doGenericSimulation();
	QString serverOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_server.out").arg(testId).arg(src.index).arg(dst.index).arg(0), serverOutput);
	QString clientOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_client.out").arg(testId).arg(src.index).arg(dst.index).arg(0), clientOutput);

	loss = 0;

	// Parse server output
	double packetsReceived = 0;
	QStringList lines = serverOutput.split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Packets received")) {
			QString s = line.split(' ', QString::SkipEmptyParts).at(2);
			packetsReceived = s.toDouble();
		}
	}
	emit logText(log, QString("Total packets received by server: %1").arg(packetsReceived));

	// Parse client output
	double packetsSent = 0;
	lines = clientOutput.split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Packets sent")) {
			QString s = line.split(' ', QString::SkipEmptyParts).at(2);
			packetsSent = s.toDouble();
		}
	}
	emit logText(log, QString("Total packets sent by client: %1").arg(packetsSent));

	loss = (1 - packetsReceived / packetsSent);
	emit logText(log, QString("Measured loss: %1%").arg(loss * 100.0));
}

/******************* Congestion validation *********************/

void MainWindow::on_btnStartCongestionValidation_clicked()
{
	ui->txtValidation->clear();

	// Make sure we saved
	if (!on_actionSave_triggered())
		return;

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::congestionValidation);
	voidWatcher.setFuture(future);
}

void MainWindow::congestionValidation()
{
	// for each path send f = 10%, 20%, ..., 100%, 110%, ..., 200% (of the bw of the path bottleneck)
	// constant bitrate traffic for a few seconds
	// measure loss rate (count sent and received packets)
	// compute theoretical congestion loss rate as max(0, 1-1/f)
	// plot (bitrate, loss rate)

	emit logInformation(ui->txtValidation, "Loss measurements under congestion");
	QList<QOPlot> *plots = new QList<QOPlot>;
	foreach (NetGraphNode src, netGraph.getHostNodes()) {
		foreach (NetGraphNode dst, netGraph.getHostNodes()) {
			if (mustStop)
				break;
			if (src.index == dst.index)
				continue;
			emit logInformation(ui->txtValidation, "Path:");

			// add a dummy connection so that we have a path and thus a route
			netGraph.addConnection(NetGraphConnection(src.index, dst.index, "", ""));
			computeRoutes();
			NetGraphPath p = netGraph.pathByNodeIndex(src.index, dst.index);
			emit logText(ui->txtValidation, p.toString());

			QList<PathCongestionMeasurement> dataMeasured;
			QList<PathCongestionMeasurement> dataTheoretical;

			const int frame_size = 500;
#if DEBUG_VALIDATION
			for (int fraction = 75; fraction <= 150; fraction += 25) {
#else
			for (int fraction = 30; fraction <= 200; fraction += 10) {
#endif
				if (mustStop)
					break;
				double loss;
				double cbrRate_kBps = fraction / 100.0 * p.bandwidth();
				double measuredBitrate_kBps;

				QString line = QString("Sending traffic: %1 KB/s (%2% of link bandwidth)").arg(cbrRate_kBps).arg(fraction);
				emit logText(ui->txtValidation, line);

				emit logInformation(ui->txtValidation, "Method 2");
				measureCongestionLoss(ui->txtValidation, src, dst, cbrRate_kBps, frame_size, loss, measuredBitrate_kBps);

				line = QString("Result: %1% loss").arg(loss * 100.0);
				emit logText(ui->txtValidation, line);

				double theorLoss = (measuredBitrate_kBps - p.bandwidth()) / measuredBitrate_kBps;
				if (theorLoss < 0)
					theorLoss = 0;
				line = QString("Theoretical loss: %1%").arg(theorLoss * 100.0);
				emit logText(ui->txtValidation, line);

				double plannedLoss = 1 - 100.0 / fraction;
				if (plannedLoss < 0)
					plannedLoss = 0;
				line = QString("Planned loss: %1%").arg(plannedLoss * 100.0);
				emit logText(ui->txtValidation, line);

				PathCongestionMeasurement item;
				item.frameSize = frame_size;
				item.trafficBandwidth = measuredBitrate_kBps;
				item.loss = loss;
				dataMeasured << item;

				item.frameSize = frame_size;
				item.trafficBandwidth = measuredBitrate_kBps;
				item.loss = theorLoss;
				dataTheoretical << item;
			}

			// make plot for path
			QOPlot plot;
			plot.title = QString("Packet loss as a function of traffic rate on path %1 - %2. Path bottleneck: %3 KB/s; Frame size %4 B").arg(p.source).arg(p.dest).arg(p.bandwidth()).arg(frame_size);
			plot.xlabel = "Traffic rate (KB/s)";
			plot.ylabel = "Packet loss (%)";

			QOPlotCurveData *curveTheoretical = new QOPlotCurveData;
			foreach (PathCongestionMeasurement item, dataTheoretical) {
				curveTheoretical->x.append(item.trafficBandwidth);
				curveTheoretical->y.append(item.loss * 100.0);
			}
			curveTheoretical->pen.setColor(Qt::blue);
			curveTheoretical->legendLabel = "Theoretical";
			curveTheoretical->pointSymbol = "+";
			plot.data << QSharedPointer<QOPlotData>(curveTheoretical);

			QOPlotCurveData *curveMeasured = new QOPlotCurveData;
			foreach (PathCongestionMeasurement item, dataMeasured) {
				curveMeasured->x.append(item.trafficBandwidth);
				curveMeasured->y.append(item.loss * 100.0);
			}
			curveMeasured->pen.setColor(Qt::red);
			curveMeasured->legendLabel = "Measured";
			curveMeasured->pointSymbol = "x";
			plot.data << QSharedPointer<QOPlotData>(curveMeasured);

			(*plots) << plot;
		}
	}
	emit congestionValidationFinished(plots);
	emit logSuccess(ui->txtValidation, "Finished");
	emit logText(ui->txtValidation, "Compare the theoretical and the measured data plots to decide whether the accuracy is acceptable.");
}

void MainWindow::doCongestionValidationFinished(QList<QOPlot> *plots)
{
	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);
	accordion->addLabel("Congestion validation plots");

	foreach (QOPlot plot, *plots) {
		QOPlotWidget *plotWidget = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		plotWidget->setPlot(plot);
		accordion->addWidget(plot.title, plotWidget);
	}

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
	emit tabResultsChanged();
}

void MainWindow::measureCongestionLoss(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
							   double rate_kBps, int frame_size,
							   double &loss, double &measuredBitrate_kBps)
{
	long long probeInterval_us = (long long)(0.5 + 1000.0 * frame_size / rate_kBps);
	long durationSec = 20;

	QString serverCmd = QString("udpping -l --sink-only");
	QString clientCmd = QString("udpping -a %1 -c 0 -i %2 -s %3 --source-only -v 0 --busywaiting").
						arg(dst.ip()).arg(probeInterval_us).arg(frame_size - UDPPING_OVERHEAD);

	netGraph.connections.clear();
	netGraph.addConnection(NetGraphConnection::cmdConnection(src.index, dst.index, serverCmd, clientCmd));
	deploy();

	QString testId = doGenericSimulation(durationSec);
	QString serverOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_server.out").arg(testId).arg(src.index).arg(dst.index).arg(0), serverOutput);
	QString clientOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_client.out").arg(testId).arg(src.index).arg(dst.index).arg(0), clientOutput);

	loss = 0;
	measuredBitrate_kBps = 0;

	// Parse server output
	double packetsReceived = 0;
	QStringList lines = serverOutput.split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Packets received")) {
			QString s = line.split(' ', QString::SkipEmptyParts).at(2);
			packetsReceived = s.toDouble();
		}
	}
	emit logText(log, QString("Total packets received by server: %1").arg(packetsReceived));

	// Parse client output
	measuredBitrate_kBps = 0;
	double probeCount = 0;
	lines = clientOutput.split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Average bitrate")) {
			QString s = line.split(' ', QString::SkipEmptyParts).at(2);
			measuredBitrate_kBps = s.toDouble() / 8.0;
		} else if (line.startsWith("Packets sent")) {
			QString s = line.split(' ', QString::SkipEmptyParts).at(2);
			probeCount = s.toDouble();
		}
	}
	emit logText(log, QString("Total packets sent by client: %1").arg(probeCount));
	emit logText(log, QString("Probe average bitrate (measured): %1 KB/s").arg(measuredBitrate_kBps));

	loss = (1 - packetsReceived / probeCount);
	emit logText(log, QString("Measured loss: %1%").arg(loss * 100.0));
}

/******************* Queuing loss validation *********************/

void MainWindow::on_btnStartQueueingLossValidation_clicked()
{
	ui->txtValidation->clear();

	// Make sure we saved
	if (!on_actionSave_triggered())
		return;

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::queueingLossValidation);
	voidWatcher.setFuture(future);
}

void MainWindow::queueingLossValidation()
{
	// for frame size in 100, 200, 500
	//     for burst = 1 to 100
	//         for 10 (a few) times
	//             send burst, measure loss
	//         compute average loss
	//         if average loss > 70%, break

	emit logInformation(ui->txtValidation, "Queue measurements");
	QList<QOPlot> *plots = new QList<QOPlot>;
	foreach (NetGraphNode src, netGraph.getHostNodes()) {
		foreach (NetGraphNode dst, netGraph.getHostNodes()) {
			if (mustStop)
				break;
			if (src.index == dst.index)
				continue;

			// add a dummy connection so that we have a path and thus a route
			netGraph.addConnection(NetGraphConnection(src.index, dst.index, "", ""));
			computeRoutes();
			NetGraphPath p = netGraph.pathByNodeIndex(src.index, dst.index);

			emit logInformation(ui->txtValidation, "Path:");
			emit logText(ui->txtValidation, p.toString());

			QList<int> frame_sizes = QList<int>() << 1500;
			foreach (int frame_size, frame_sizes) {
				if (mustStop)
					break;
				QList<PathQueueMeasurement> dataMeasured;
				QList<PathQueueMeasurement> dataTheoretical;
				double loss = 0;
				for (int burst = 1; loss < 0.50; burst++) {
					long long interval_us = qMax(3000000LL, (long long)(5 * 1000 * burst * p.computeFwdDelay(frame_size)));

					emit logText(ui->txtValidation, QString("Sending traffic, burst size: %1").arg(burst));

					measureQueueingLoss(ui->txtValidation, src, dst, frame_size, burst, interval_us, loss);

					emit logText(ui->txtValidation, QString("Result: %1% loss").arg(loss * 100.0));

					PathQueueMeasurement item;
					item.frameSize = frame_size;
					item.burstSize = burst;
					item.loss = loss;
					dataMeasured << item;

					theoreticalQueueingLoss(p, burst, item.loss);
					dataTheoretical << item;

					emit logText(ui->txtValidation, QString("Theoretical: %1% loss").arg(item.loss * 100.0));

					if (loss > burst / 3.0)
						break;
				}
				// plotQueueingLoss(p, dataMeasured, dataTheoretical);
				// make plot for path
				QOPlot plot;
				plot.title = QString("Packet loss as a function of traffic burst size for path %1-%2. Frame size %3 B").arg(p.source).arg(p.dest).arg(frame_size);
				plot.xlabel = "Burst size (packet count)";
				plot.ylabel = "Packet loss (%)";

				QOPlotCurveData *curveTheoretical = new QOPlotCurveData;
				foreach (PathQueueMeasurement item, dataTheoretical) {
					curveTheoretical->x.append(item.burstSize);
					curveTheoretical->y.append(item.loss * 100.0);
				}
				curveTheoretical->pen.setColor(Qt::blue);
				curveTheoretical->legendLabel = "Theoretical";
				curveTheoretical->pointSymbol = "+";
				plot.data << QSharedPointer<QOPlotData>(curveTheoretical);

				QOPlotCurveData *curveMeasured = new QOPlotCurveData;
				foreach (PathQueueMeasurement item, dataMeasured) {
					curveMeasured->x.append(item.burstSize);
					curveMeasured->y.append(item.loss * 100.0);
				}
				curveMeasured->pen.setColor(Qt::red);
				curveMeasured->legendLabel = "Measured";
				curveMeasured->pointSymbol = "x";
				plot.data << QSharedPointer<QOPlotData>(curveMeasured);

				(*plots) << plot;
			}
		}
	}
	emit queuingLossValidationFinished(plots);
	emit logSuccess(ui->txtValidation, "Finished");
	emit logText(ui->txtValidation, "Compare the theoretical and the measured data plots to decide whether the accuracy is acceptable.");
}

void MainWindow::doQueuingLossValidationFinished(QList<QOPlot> *plots)
{
	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);
	accordion->addLabel("Queuing loss validation plots");

	foreach (QOPlot plot, *plots) {
		QOPlotWidget *plotWidget = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		plotWidget->setPlot(plot);
		accordion->addWidget(plot.title, plotWidget);
	}

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
	emit tabResultsChanged();
}

void MainWindow::measureQueueingLoss(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
							   int frame_size, int burst_size,
							   long long interval_us, double &loss)
{
	// we send probeCount bursts of burst_size probes each
	const long probeCount = 10;

	QString serverCmd = QString("udpping -l --sink-only");
	QString clientCmd = QString("udpping -a %1 -c %2 -i %3 -s %4 --source-only -v 0 -b %5")
			.arg(dst.ip()).arg(probeCount).arg(interval_us).arg(frame_size - UDPPING_OVERHEAD).arg(burst_size);

	netGraph.connections.clear();
	netGraph.addConnection(NetGraphConnection::cmdConnection(src.index, dst.index, serverCmd, clientCmd));
	deploy();

	QString testId = doGenericSimulation(qRound(interval_us / 1.0e6));
	QString serverOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_server.out").arg(testId).arg(src.index).arg(dst.index).arg(0), serverOutput);
	QString serverErr;
	readFile(QString("%1/connection_%2-%3_%4_0_server.err").arg(testId).arg(src.index).arg(dst.index).arg(0), serverErr);
	serverOutput += "\n" + serverErr;
	QString clientOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_client.out").arg(testId).arg(src.index).arg(dst.index).arg(0), clientOutput);
	QString clientErr;
	readFile(QString("%1/connection_%2-%3_%4_0_client.err").arg(testId).arg(src.index).arg(dst.index).arg(0), clientErr);
	clientOutput += "\n" + clientErr;

	loss = 0;

	emit logInformation(ui->txtValidation, "Client output:");
	emit logOutput(ui->txtValidation, clientOutput);
	emit logInformation(ui->txtValidation, "Server output:");
	emit logOutput(ui->txtValidation, serverOutput);

	// Parse server output
	double packetsReceived = 0;
	QStringList lines = serverOutput.split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Packets received")) {
			QString s = line.split(' ', QString::SkipEmptyParts).at(2);
			packetsReceived = s.toDouble();
		}
	}
	emit logText(log, QString("Total packets received by server: %1").arg(packetsReceived));

	// Parse client output
	double packetsSent = 0;
	lines = clientOutput.split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Packets sent")) {
			QString s = line.split(' ', QString::SkipEmptyParts).at(2);
			packetsSent= s.toDouble();
		}
	}
	emit logText(log, QString("Total packets sent by client: %1").arg(packetsSent));

	loss = (1 - packetsReceived / packetsSent);
	emit logText(log, QString("Measured loss: %1%").arg(loss * 100.0));
}

void MainWindow::theoreticalQueueingLoss(NetGraphPath p, int burst_size, double &loss)
{
	double arrivalRate_kBps = DBL_MAX;
	double c = burst_size;

	foreach (NetGraphEdge e, p.edgeList) {
		double serviceRate_kBps = e.bandwidth;
		double q = e.queueLength;
		double qloss = 1 - serviceRate_kBps/arrivalRate_kBps - q/c;
		if (qloss < 0)
			qloss = 0;
		c *= (1 - qloss);
		arrivalRate_kBps = serviceRate_kBps;
	}
	loss = 1.0 - c/burst_size;
}

/******************* Queuing delay validation *********************/

void MainWindow::on_btn_QueueingDelay_clicked()
{
	ui->txtValidation->clear();

	// Make sure we saved
	if (!on_actionSave_triggered())
		return;

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::queueingDelayValidation);
	voidWatcher.setFuture(future);
}

void MainWindow::queueingDelayValidation()
{
	// pick first link in path
	// send bursts of 200% qsize for a few seconds
	// check delay

	emit logInformation(ui->txtValidation, "Queue measurements");
	QList<QOPlot> *plots = new QList<QOPlot>;
	foreach (NetGraphNode src, netGraph.getHostNodes()) {
		foreach (NetGraphNode dst, netGraph.getHostNodes()) {
			if (mustStop)
				break;
			if (src.index == dst.index)
				continue;

			// add a dummy connection so that we have a path and thus a route
			netGraph.addConnection(NetGraphConnection(src.index, dst.index, "", ""));
			computeRoutes();
			NetGraphPath p = netGraph.pathByNodeIndex(src.index, dst.index);

			emit logInformation(ui->txtValidation, "Path:");
			emit logText(ui->txtValidation, p.toString());

			qreal bneck = p.edgeList.first().bandwidth;
			bool ok = true;
			for (int i = 1; i < p.edgeList.count(); i++) {
				if (p.edgeList.at(i).bandwidth < bneck) {
					emit logText(ui->txtValidation, QString("Cannot use path because the bottleneck is not the first link (link %1-%2 has smaller rate)").arg(p.edgeList.at(i).source).arg(p.edgeList.at(i).dest));
					ok = false;
					break;
				}
			}
			if (!ok)
				continue;

			QList<int> frame_sizes = QList<int>() << 500;
			foreach (int frame_size, frame_sizes) {
				if (mustStop)
					break;
				double cbrRate_kBps = 200.0 / 100.0 * p.bandwidth();
				const double theorDelay_ms = p.computeFwdDelay(frame_size) + (p.edgeList.first().queueLength * ETH_FRAME_LEN - frame_size) / p.edgeList.first().bandwidth;
				const int fillTime_s = qRound((p.edgeList.first().queueLength * ETH_FRAME_LEN) / p.edgeList.first().bandwidth);
				int runTime_s = qMax(20, 2 * fillTime_s);

				//emit logText(ui->txtValidation, QString("Sending traffic, burst size: %1").arg(burst));
				emit logText(ui->txtValidation, QString("Sending traffic, rate: %1").arg(cbrRate_kBps));

				QList<quint64> delays_us;
				measureQueueingDelay(ui->txtValidation, src, dst, frame_size, cbrRate_kBps, runTime_s, delays_us);

				emit logText(ui->txtValidation, QString("Theoretical maximum delay: %1 ms").arg(theorDelay_ms));
				emit logText(ui->txtValidation, QString("Measured delay (ms): %1-%2").arg(qMinimum(delays_us, 0) * 1.0e-3).arg(qMaximum(delays_us, 0) * 1.0e-3));

				// make plot for path
				QOPlot plot;
				plot.title = QString("Packet delay as queue gets filled for path %1-%2. Frame size %3 B").arg(p.source).arg(p.dest).arg(frame_size);
				plot.xlabel = "Arrival events at destination";
				plot.ylabel = "Packet delay (ms)";

				QOPlotCurveData *curveMeasured = new QOPlotCurveData;
				for (int i = 0; i < delays_us.count(); i++) {
					curveMeasured->x.append(i);
					curveMeasured->y.append(delays_us[i] * 1.0e-3);
				}
				curveMeasured->pen.setColor(Qt::red);
				curveMeasured->legendLabel = "Measured";
				curveMeasured->pointSymbol = "x";
				plot.data << QSharedPointer<QOPlotData>(curveMeasured);

				QOPlotCurveData *curveTheoretical = new QOPlotCurveData;
				curveTheoretical->x.append(0);
				curveTheoretical->y.append(theorDelay_ms);
				curveTheoretical->x.append(delays_us.count() - 1);
				curveTheoretical->y.append(theorDelay_ms);
				curveTheoretical->pen.setColor(Qt::blue);
				curveTheoretical->pen.setStyle(Qt::DashLine);
				curveTheoretical->legendLabel = "Theoretical maximum";
				plot.data << QSharedPointer<QOPlotData>(curveTheoretical);

				(*plots) << plot;
			}
		}
	}
	emit queuingDelayValidationFinished(plots);
	emit logSuccess(ui->txtValidation, "Finished");
}

void MainWindow::doQueuingDelayValidationFinished(QList<QOPlot> *plots)
{
	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);
	accordion->addLabel("Queuing delay validation plots");

	foreach (QOPlot plot, *plots) {
		QOPlotWidget *plotWidget = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		plotWidget->setPlot(plot);
		accordion->addWidget(plot.title, plotWidget);
	}

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
	emit tabResultsChanged();
}

void MainWindow::measureQueueingDelay(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
							   int frame_size, int rate_kBps, int runTime_s,
							   QList<quint64> &delays_us)
{
	long long probeInterval_us = (long long)(0.5 + 1000.0 * frame_size / rate_kBps);
	QString serverCmd = QString("udpping -l -v 1 --sink-only");
	QString clientCmd = QString("udpping -a %1 -c 0 -i %2 -s %3 --source-only -v 0 --busywaiting").
						arg(dst.ip()).arg(probeInterval_us).arg(frame_size - UDPPING_OVERHEAD);

	netGraph.connections.clear();
	netGraph.addConnection(NetGraphConnection::cmdConnection(src.index, dst.index, serverCmd, clientCmd));
	deploy();

	QString testId = doGenericSimulation(runTime_s);
	QString serverOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_server.out").arg(testId).arg(src.index).arg(dst.index).arg(0), serverOutput);
	QString serverErr;
	readFile(QString("%1/connection_%2-%3_%4_0_server.err").arg(testId).arg(src.index).arg(dst.index).arg(0), serverErr);
	serverOutput += "\n" + serverErr;
	QString clientOutput;
	readFile(QString("%1/connection_%2-%3_%4_0_client.out").arg(testId).arg(src.index).arg(dst.index).arg(0), clientOutput);
	QString clientErr;
	readFile(QString("%1/connection_%2-%3_%4_0_client.err").arg(testId).arg(src.index).arg(dst.index).arg(0), clientErr);
	clientOutput += "\n" + clientErr;

	delays_us.clear();

	emit logInformation(log, "Simulation finished.");

	// Parse server output
	QStringList lines = serverOutput.split('\n', QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		QString line = lines[i];
		if (line.startsWith("Client connected:")) {
			QString s = line.split(' ', QString::SkipEmptyParts).last();
			bool ok;
			quint64 value;
			value = s.toULongLong(&ok);
			if (ok)
				delays_us << value;
		}
	}
}

/******************* Ssh stress test *********************/

void MainWindow::on_btnSshStressTest_clicked()
{
	ui->txtBatch->clear();

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::doSshStressTest);
	voidWatcher.setFuture(future);
}

void MainWindow::doSshStressTest()
{
	RemoteProcessSsh ssh;
	if (!ssh.connect(getClientHostname(0))) {
		emit logError(ui->txtValidation, "Connect failed");
		return;
	}

	QStringList keys;
	int count = 500;
	int errorCount = 0;

	emit logInformation(ui->txtValidation, QString("Starting %1 ping 127.0.0.1 instances..").arg(count));
	for (int i = 0; i < count; i++) {
		keys << ssh.startProcess("ping 127.0.0.1");
	}
	emit logInformation(ui->txtValidation, QString("Done."));

	emit logInformation(ui->txtValidation, QString("Wait 5 seconds..."));
	sleep(5);
	emit logInformation(ui->txtValidation, QString("Done."));

	emit logInformation(ui->txtValidation, QString("Checking running state..."));
	for (int i = 0; i < count; i++) {
		if (!ssh.isProcessRunning(keys[i])) {
			emit logError(ui->txtValidation, QString("Error: %1 is not running").arg(keys[i]));
			errorCount++;
		}
	}
	emit logInformation(ui->txtValidation, QString("Done."));

	emit logInformation(ui->txtValidation, QString("Kill all..."));
	for (int i = 0; i < count; i++) {
		if (!ssh.signalProcess(keys[i], 9)) {
			emit logError(ui->txtValidation, QString("Error: %1 signal failed").arg(keys[i]));
			errorCount++;
		}

		if (!ssh.waitForFinished(keys[i], 5)) {
			emit logError(ui->txtValidation, QString("Error: %1 wait failed").arg(keys[i]));
			errorCount++;
		}
	}

	for (int i = 0; i < count; i++) {
		if (ssh.isProcessRunning(keys[i])) {
			emit logError(ui->txtValidation, QString("Error: %1 is still running").arg(keys[i]));
			errorCount++;
		}
	}

	for (int i = 0; i < count; i++) {
		emit logInformation(ui->txtValidation, QString("STDOUT %1:").arg(i));
		emit logOutput(ui->txtValidation, ssh.readAllStdout(keys[i]));
		emit logInformation(ui->txtValidation, QString("STDERR %1:").arg(i));
		emit logOutput(ui->txtValidation, ssh.readAllStderr(keys[i]));
	}
	emit logInformation(ui->txtValidation, QString("Test done. Errors: %1").arg(errorCount));
}

#endif
