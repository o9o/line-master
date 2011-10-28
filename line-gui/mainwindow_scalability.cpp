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

#ifdef ENABLE_SCALABILITY

void MainWindow::on_btnBatchScalability_clicked()
{
	ui->txtBatch->clear();

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::checkScalability);
	voidWatcher.setFuture(future);
}

void MainWindow::checkScalability()
{
	// TODO switch to doGenericSimulation


	// for pairs = 1 to maxPairs
	//     generate topology:
	//         all links have the same predefined bitrate
	//         make i source nodes, connect each one to the gateway
	//         make i destination nodes, connect gateway to each one
	//     save, deploy
	//     for fraction = 30% ... 200% of the link rate
	//         start i CBR sinks on the targets
	//         start i CBR sources, with the fraction of the bitrate
	//         wait a number of seconds
	//         stop everything and compute the loss
	//     plot(traffic rate, loss)
	// see for what n the plots look different

//	const int maxPairs = 2;
//	const int stepPairs = 2;

//	QList<ScalabilityMeasurement> measurements;
//	for (int pairs = stepPairs; pairs <= maxPairs; pairs += stepPairs) {
//		if (mustStop)
//			break;
//		// generate topology
//		emit logInformation(ui->txtBatch, QString("Generating topology with %2 pairs of hosts").arg(pairs));

//		netGraph.clear();
//		int gateway = netGraph.addNode(NETGRAPH_NODE_GATEWAY, QPointF(0, 0));
//		const double radiusMin = 300;
//		double radius = pairs/2.0 * 3.0 * NETGRAPH_NODE_RADIUS/M_PI * 3.0;
//		if (radius < radiusMin)
//			radius = radiusMin;
//		// minimun 2.0 * NETGRAPH_NODE_RADIUS / radius, anything higher adds spacing
//		double alpha = 4.0 * NETGRAPH_NODE_RADIUS / radius;
//		for (int pairIndex = 0; pairIndex < pairs; pairIndex++) {
//			double angle = M_PI - (pairs / 2) * alpha + pairIndex * alpha + (1 - pairs % 2) * alpha / 2.0;
//			int first = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(radius * cos(angle), -radius * sin(angle)));
//			angle = M_PI - angle;
//			int second = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(radius * cos(angle), -radius * sin(angle)));
//			netGraph.addEdge(first, gateway, 128, 3, 0, 10);
//			netGraph.addEdge(gateway, first, 128, 3, 0, 10);
//			netGraph.addEdge(gateway, second, 128, 3, 0, 10);
//			netGraph.addEdge(second, gateway, 128, 3, 0, 10);
//		}
//		netGraph.setFileName(QString("manypairs_%1.graph").arg(pairs));
//		netGraph.saveToFile();
//		emit reloadScene();
//		emit saveGraph();
//		emit logSuccess(ui->txtBatch, "OK.");

//		// deploy topology
//		emit logInformation(ui->txtBatch, QString("Deploying topology..."));
//		emit logClear(ui->txtDeploy);
//		/*deploy();
//		if (!checkDeployed(ui->txtBatch)) {
//			emit logError(ui->txtBatch, "Deployment failed, aborting.");
//			return;
//		}*/

//		// prepare traffic sources/sinks
//		QList<QPair<NetGraphHostNode, NetGraphHostNode> > connections;
//		for (int ipair = 0; ipair < pairs; ipair++) {
//			connections << QPair<NetGraphHostNode, NetGraphHostNode>(netGraph.hostNodeByVNIndex(2 * ipair), netGraph.hostNodeByVNIndex(2 * ipair + 1));
//		}

//		// run measurements
//		double rate_kBps = 20;
//		int frame_size = 300;
//		double loss;
//		double measuredBitrate_kBps;
//		//TODO

//		ScalabilityMeasurement item;
//		item.flows = pairs;
//		item.frameSize = frame_size;
//		item.loss = loss;
//		item.totalMeasuredRate_kBps =  measuredBitrate_kBps;
//		item.totalTheoreticalRate_kBps = rate_kBps * pairs;
//		measurements << item;

//		plotScalability(measurements);
//	}
//	emit logSuccess(ui->txtBatch, "All done.");
}

void MainWindow::plotScalability(QList<ScalabilityMeasurement> measurements)
{
	QString graphName = getGraphName();

	QStringList script = QStringList() << "% plot scalability measurements";
	int frame_size = 0;
	int maxFlows = 0;

	QString flows = "flows = [";
	foreach (ScalabilityMeasurement item, measurements) {
		flows += QString::number(item.flows) + " ";
		frame_size = item.frameSize;
		if (item.flows > maxFlows)
			maxFlows = item.flows;
	}
	flows += "];";
	script << flows;

	QString measuredRate = "measuredRate = [";
	foreach (ScalabilityMeasurement item, measurements) {
		measuredRate += QString::number(item.totalMeasuredRate_kBps) + " ";
		frame_size = item.frameSize;
	}
	measuredRate += "];";
	script << measuredRate;

	QString theoreticalRate = "theoreticalRate = [";
	foreach (ScalabilityMeasurement item, measurements) {
		theoreticalRate += QString::number(item.totalTheoreticalRate_kBps) + " ";
		frame_size = item.frameSize;
	}
	theoreticalRate += "];";
	script << theoreticalRate;

	QString loss = "loss = [";
	foreach (ScalabilityMeasurement item, measurements) {
		loss += QString::number(item.loss * 100.0) + " ";
	}
	loss += "];";
	script << loss;

	QDir dir("");
	dir.mkdir(graphName);
	dir.cd(graphName);

	QString suffix = "_flows_" + QString::number(maxFlows) + "_framesize_" + QString::number(frame_size);

	script << "figure;";
	script << "plot(flows, measuredRate, 'r+');";
	script << "hold on;";
	script << "plot(flows, theoreticalRate, 'bx');";
	script << "xlim([0 max(flows)+1]);";
	script << "ylim([0 max([measuredRate theoreticalRate])*1.1]);";
	script << "xlabel('Flow count');";
	script << "ylabel('Total throughput (KB/s)');";
	script << "legend('Measured', 'Theoretical');";
	script << QString("title('Throughput for different flow count. Frame size %1 B.');").
			arg(QString::number(frame_size));
	script << "print('" + graphName + "/scalability_throughput" + suffix + ".png','-dpng');";

	script << "figure;";
	script << "plot(flows, loss, 'r*');";
	script << "xlim([0 max(flows)+1]);";
	script << "ylim([0 100]);";
	script << "xlabel('Flow count');";
	script << "ylabel('Total packet loss (%)');";
	script << QString("title('Packet loss for different flow count. Frame size %1 B.');").
			arg(QString::number(frame_size));
	script << "print('" + graphName + "/scalability_loss" + suffix + ".png','-dpng');";

	saveFile(graphName + "/plotscalability" + suffix + ".m", script.join("\n"));
	runProcess("octave", QStringList() << graphName + "/plotscalability" + suffix + ".m");

	dir.cdUp();
}

#endif
