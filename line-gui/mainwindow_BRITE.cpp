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

void MainWindow::on_btnBriteImport_clicked()
{
	QDir briteDir ("../brite/BRITE/");
	QDir topoDir (".");
	QString fromFileName = QFileDialog::getOpenFileName(this, "Open BRITE topology", briteDir.absolutePath(), "BRITE topology file (*.brite)");
	if (fromFileName.isEmpty())
		return;

	QString toFileName;
	toFileName = fromFileName.split('/', QString::SkipEmptyParts).last().replace(".brite", ".graph", Qt::CaseInsensitive);
	toFileName = QFileDialog::getSaveFileName(this, "Save network graph", topoDir.absolutePath() + "/" + toFileName, "Network graph file (*.graph)");

	if (toFileName.isEmpty())
		return;

	ui->txtBrite->clear();
	// Start the load
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::importBrite, fromFileName, toFileName);
	voidWatcher.setFuture(future);
}

void MainWindow::importBrite(QString fromFileName, QString toFileName)
{
	if (!briteImporter.import(fromFileName, toFileName))
		return;

	emit importFinished(toFileName);
}

void MainWindow::layoutGraph()
{
	netGraph.layoutGephi();
	netGraph.computeASHulls();
	netGraph.saveToFile();
	emit layoutFinished();
}

void MainWindow::onImportFinished(QString fileName)
{
	doLogBriteInfo("Import finished.");
	netGraph.setFileName(fileName);
	if (!netGraph.loadFromFile()) {
		QMessageBox::critical(this, "Open error", QString() + "Could not load file " + netGraph.fileName, QMessageBox::Ok);
		return;
	}
	setWindowTitle(QString("line-gui - %1").arg(getGraphName()));

	doLogBriteInfo("Adding host nodes...");
	// Add host nodes
	foreach (NetGraphNode n, netGraph.nodes) {
		if (n.nodeType == NETGRAPH_NODE_GATEWAY) {
			int host = netGraph.addNode(NETGRAPH_NODE_HOST, QPointF(), n.ASNumber);
			netGraph.addEdgeSym(host, n.index, 300, 1, 0, 20);
		}
	}

	doLogBriteInfo("Adding connections...");
	// Generate connections
	QList<NetGraphNode> hostNodes = netGraph.getHostNodes();
	for (int i = 0; i < ui->spinConnections->value(); i++) {
		int n1 = random() % hostNodes.count();
		int n2 = random() % hostNodes.count();
		if (n1 != n2) {
			netGraph.addConnection(NetGraphConnection(hostNodes[n1].index, hostNodes[n2].index, "TCP", ""));
		} else {
			i--;
		}
	}

	doLogBriteInfo("Computing routes...");
	doRecomputeRoutes();
}

void MainWindow::doLayoutGraph()
{
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::layoutGraph);
	voidWatcher.setFuture(future);
}

void MainWindow::onLayoutFinished()
{
	doLogBriteInfo("Layout finished.");
	doLogBriteInfo("Loading scene and drawing.");
	scene.reload();
	scene.usedChanged();
}

void MainWindow::doRecomputeRoutes()
{
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::recomputeRoutes);
	voidWatcher.setFuture(future);
}

void MainWindow::recomputeRoutes()
{
	netGraph.computeRoutes();
	netGraph.updateUsed();
	emit routingFinished();
}

void MainWindow::onRoutingFinished()
{
	doLogBriteInfo("Routing finished.");
	doLogBriteInfo("Layout graph via GESHI...");
	// Layout
	doLayoutGraph();
}

