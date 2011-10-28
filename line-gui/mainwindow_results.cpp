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

#include "../tomo/tomodata.h"

Simulation::Simulation()
{
	dir = "(new)";
	graphName = "untitled";
}

Simulation::Simulation(QString dir) : dir(dir)
{
	// load data from simulation.txt
	QFile file(dir + "/" + "simulation.txt");
	if (file.open(QFile::ReadOnly)) {
		// file opened successfully
		QTextStream t(&file);
		while (true) {
			QString line = t.readLine().trimmed();
			if (line.isNull())
				break;
			if (line.contains('=')) {
				QStringList tokens = line.split('=');
				if (tokens.count() != 2)
					continue;

				QString key = tokens.at(0).trimmed();
				QString val = tokens.at(1).trimmed();

				if (key == "graph") {
					graphName = val;
				}
			}
		}
		file.close();
	}
}

void MainWindow::doReloadTopologyList()
{
	QString currentTopologyEntry;
	if (currentTopology >= 0) {
		currentTopologyEntry = ui->cmbTopologies->itemText(currentTopology);
	}
	ui->cmbTopologies->clear();

	ui->cmbTopologies->addItem("(untitled)");

	QDir dir;
	dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	dir.setSorting(QDir::Name);
	QFileInfoList list = dir.entryInfoList();
	for (int i = 0; i < list.size(); ++i) {
		QFileInfo fileInfo = list.at(i);

		if (fileInfo.fileName().startsWith("."))
			continue;

		if (!fileInfo.fileName().endsWith(".graph"))
			continue;

		ui->cmbTopologies->addItem(fileInfo.fileName().replace(QRegExp("\\.graph$"), ""));
	}

	for (int i = 0; i < ui->cmbTopologies->count(); i++) {
		if (ui->cmbTopologies->itemText(i) == currentTopologyEntry) {
			ui->cmbTopologies->setCurrentIndex(i);
			break;
		}
	}
}

void MainWindow::doReloadSimulationList()
{
	QString currentEntry;
	if (currentSimulation >= 0) {
		currentEntry = simulations[currentSimulation].dir;
	}

	simulations.clear();
	ui->cmbSimulation->clear();

	QString topology;
	if (currentTopology >= 0) {
		topology = ui->cmbTopologies->itemText(currentTopology);
	} else {
		topology = "(untitled)";
	}

	// dummy entry for new sim
	{
		Simulation s;
		s.dir = "(new)";
		s.graphName = topology;
		simulations << s;
		ui->cmbSimulation->addItem(s.dir);
	}

	QDir dir;
	dir.setFilter(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks);
	dir.setSorting(QDir::Name);
	QFileInfoList list = dir.entryInfoList();
	for (int i = 0; i < list.size(); ++i) {
		QFileInfo fileInfo = list.at(i);

		if (fileInfo.fileName().startsWith("."))
			continue;

		if (!QFile::exists(fileInfo.fileName() + "/" + "simulation.txt"))
			continue;

		Simulation s (fileInfo.fileName());
		if (s.graphName == topology) {
			simulations << s;
			ui->cmbSimulation->addItem(s.dir);
		}
	}

	for (int i = 0; i < simulations.count(); i++) {
		if (simulations[i].dir == currentEntry) {
			ui->cmbSimulation->setCurrentIndex(i);
			break;
		}
	}
}

void MainWindow::loadSimulation()
{
	if (currentSimulation <= 0)
		return;
	if (!loadGraph(simulations[currentSimulation].dir + "/" + simulations[currentSimulation].graphName + ".graph"))
		return;

	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);

	accordion->addLabel("Emulator");
	// emulator.out
	{
		QString content;
		QString fileName = simulations[currentSimulation].dir + "/" + "emulator.out";
		if (!readFile(fileName, content, true)) {
			QMessageBox::critical(this, "Open file error", QString("Failed to open file %1").arg(fileName));
		}
		QTextEdit *txt = new QTextEdit();
		txt->setPlainText(content);
		accordion->addWidget("Emulator output", txt);
	}
	// emulator.err
	{
		QString content;
		QString fileName = simulations[currentSimulation].dir + "/" + "emulator.err";
		if (!readFile(fileName, content, true)) {
			QMessageBox::critical(this, "Open file error", QString("Failed to open file %1").arg(fileName));
		}
		QTextEdit *txt = new QTextEdit();
		txt->setPlainText(content);
		accordion->addWidget("Emulator stderr", txt);
	}

	accordion->addLabel("Tomo records");
	{
		TomoData tomoData;
		QString fileName = simulations[currentSimulation].dir + "/" + "tomo-records.dat";
		if (!tomoData.load(fileName)) {
			QMessageBox::critical(this, "Open file error", QString("Failed to open file %1").arg(fileName));
		}

		QString content;
		content += QString("Number of paths: %1\n").arg(tomoData.m);
		content += QString("Number of edges: %1\n").arg(tomoData.n);
		content += QString("Simulation time (s): %1\n").arg((tomoData.tsMax - tomoData.tsMin) / 1.0e9);
		content += QString("Transmission rates for paths:");
		foreach (qreal v, tomoData.y) {
			content += QString(" %1").arg(v);
		}
		content += "\n";
		content += QString("Transmission rates for edges:");
		foreach (qreal v, tomoData.xmeasured) {
			content += QString(" %1").arg(v);
		}
		content += "\n";

		QTextEdit *txt = new QTextEdit();
		txt->setPlainText(content);
		accordion->addWidget("Data", txt);
	}

	accordion->addLabel("Packet events");
	{
		// for each link, array of bits: 0 = forward, 1 = drop
		for (int i = 0; i < netGraph.edges.count(); i++) {
			QString fileName = simulations[currentSimulation].dir + "/" + QString("packetevents-edge-%1.dat").arg(i);
			QFile file(fileName);
			if (!file.open(QIODevice::ReadOnly)) {
				QMessageBox::critical(this, "Open file error", QString("Failed to open file %1").arg(fileName));
			}
			QDataStream in(&file);
			in.setVersion(QDataStream::Qt_4_0);

			QVector<quint8> values;
			in >> values;
			qDebug() << __FILE__ << __LINE__ << "values.count() =" << values.count();
			QString title = QString("Packet events for edge %1 -> %2").arg(netGraph.edges[i].source).arg(netGraph.edges[i].dest);
			if (!values.isEmpty()) {
				QOPlotWidget *plot = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
				plot->plot.title = title;

				QOPlotStemData *stem = new QOPlotStemData();
				for (int i = 0; i < values.count(); i++) {
					stem->x.append(i);
					stem->y.append(values.at(i));
				}
				stem->pen = QPen(Qt::blue);
				stem->legendLabel = "Events (0 = forward, 1 = drop)";
				plot->plot.data << QSharedPointer<QOPlotData>(stem);
				plot->plot.drag_y_enabled = false;
				plot->plot.zoom_y_enabled = false;
				plot->fixAxes(0, 1000, 0, 2);
				plot->drawPlot();

				accordion->addWidget(title, plot);
			}
		}
	}

	accordion->addLabel("Link timelines");
	{
		for (int iEdge = 0; iEdge < netGraph.edges.count(); iEdge++) {
			QString fileName = simulations[currentSimulation].dir + "/" + QString("timelines-edge-%1.dat").arg(iEdge);
			QFile file(fileName);
			if (!file.open(QIODevice::ReadOnly)) {
				QMessageBox::critical(this, "Open file error", QString("Failed to open file %1").arg(fileName));
			}
			QDataStream in(&file);
			in.setVersion(QDataStream::Qt_4_0);

			QString title = QString("Timeline for edge %1 -> %2").arg(netGraph.edges[iEdge].source).arg(netGraph.edges[iEdge].dest);

			quint64 tsMin, tsMax, tsample;
			quint64 rate_Bps;
			qint32 delay_ms;
			quint64 qcapacity;

			QVector<quint64> vector_timestamp;
			QVector<quint64> vector_arrivals_p;
			QVector<quint64> vector_arrivals_B;
			QVector<quint64> vector_qdrops_p;
			QVector<quint64> vector_qdrops_B;
			QVector<quint64> vector_rdrops_p;
			QVector<quint64> vector_rdrops_B;
			QVector<quint64> vector_queue_sampled;
			QVector<quint64> vector_queue_avg;
			QVector<quint64> vector_queue_max;


			// edge timeline range
			in >> tsMin;
			in >> tsMax;

			// edge properties
			in >> tsample;
			in >> rate_Bps;
			in >> delay_ms;
			in >> qcapacity;

			in >> vector_timestamp;
			in >> vector_arrivals_p;
			in >> vector_arrivals_B;
			in >> vector_qdrops_p;
			in >> vector_qdrops_B;
			in >> vector_rdrops_p;
			in >> vector_rdrops_B;
			in >> vector_queue_sampled;
			in >> vector_queue_avg;
			in >> vector_queue_max;

			if (vector_timestamp.isEmpty())
				continue;

			quint64 nelem = (tsMax - tsMin)/tsample + (((tsMax - tsMin)%tsample) ? 1 : 0);

			QOPlotCurveData *arrivals_p = new QOPlotCurveData;
			QOPlotCurveData *arrivals_B = new QOPlotCurveData;
			QOPlotCurveData *qdrops_p = new QOPlotCurveData;
			QOPlotCurveData *qdrops_B = new QOPlotCurveData;
			QOPlotCurveData *rdrops_p = new QOPlotCurveData;
			QOPlotCurveData *rdrops_B = new QOPlotCurveData;
			QOPlotCurveData *queue_sampled = new QOPlotCurveData;
			QOPlotCurveData *queue_avg = new QOPlotCurveData;
			QOPlotCurveData *queue_max = new QOPlotCurveData;

			arrivals_p->x.reserve(nelem);
			arrivals_p->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				arrivals_p->x << vector_timestamp.at(i);
				arrivals_p->y << vector_arrivals_p.at(i);
			}

			QString subtitle = " - Arrivals (packets)";
			QOPlotWidget *plot_arrivals_p = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_arrivals_p->plot.title = title + subtitle;
			plot_arrivals_p->plot.data << QSharedPointer<QOPlotData>(arrivals_p);
			plot_arrivals_p->plot.drag_y_enabled = false;
			plot_arrivals_p->plot.zoom_y_enabled = false;
			plot_arrivals_p->fixAxes(0, 1000, 0, 2);
			plot_arrivals_p->drawPlot();
			accordion->addWidget(title + subtitle, plot_arrivals_p);

			arrivals_B->x = arrivals_p->x;
			arrivals_B->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				arrivals_B->y << vector_arrivals_B.at(i);
			}

			subtitle = " - Arrivals (bytes)";
			QOPlotWidget *plot_arrivals_B = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_arrivals_B->plot.title = title + subtitle;
			plot_arrivals_B->plot.data << QSharedPointer<QOPlotData>(arrivals_B);
			plot_arrivals_B->plot.drag_y_enabled = false;
			plot_arrivals_B->plot.zoom_y_enabled = false;
			plot_arrivals_B->fixAxes(0, 1000, 0, 2);
			plot_arrivals_B->drawPlot();
			accordion->addWidget(title + subtitle, plot_arrivals_B);

			qdrops_p->x = arrivals_p->x;
			qdrops_p->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				qdrops_p->y << vector_qdrops_p.at(i);
			}

			subtitle = " - Queue drops (packets)";
			QOPlotWidget *plot_qdrops_p = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_qdrops_p->plot.title = title + subtitle;
			plot_qdrops_p->plot.data << QSharedPointer<QOPlotData>(qdrops_p);
			plot_qdrops_p->plot.drag_y_enabled = false;
			plot_qdrops_p->plot.zoom_y_enabled = false;
			plot_qdrops_p->fixAxes(0, 1000, 0, 2);
			plot_qdrops_p->drawPlot();
			accordion->addWidget(title + subtitle, plot_qdrops_p);

			qdrops_B->x = arrivals_p->x;
			qdrops_B->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				qdrops_B->y << vector_qdrops_B.at(i);
			}

			subtitle = " - Queue drops (bytes)";
			QOPlotWidget *plot_qdrops_B = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_qdrops_B->plot.title = title + subtitle;
			plot_qdrops_B->plot.data << QSharedPointer<QOPlotData>(qdrops_B);
			plot_qdrops_B->plot.drag_y_enabled = false;
			plot_qdrops_B->plot.zoom_y_enabled = false;
			plot_qdrops_B->fixAxes(0, 1000, 0, 2);
			plot_qdrops_B->drawPlot();
			accordion->addWidget(title + subtitle, plot_qdrops_B);

			rdrops_p->x = arrivals_p->x;
			rdrops_p->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				rdrops_p->y << vector_rdrops_p.at(i);
			}

			subtitle = " - Random drops (packets)";
			QOPlotWidget *plot_rdrops_p = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_rdrops_p->plot.title = title + subtitle;
			plot_rdrops_p->plot.data << QSharedPointer<QOPlotData>(rdrops_p);
			plot_rdrops_p->plot.drag_y_enabled = false;
			plot_rdrops_p->plot.zoom_y_enabled = false;
			plot_rdrops_p->fixAxes(0, 1000, 0, 2);
			plot_rdrops_p->drawPlot();
			accordion->addWidget(title + subtitle, plot_rdrops_p);

			rdrops_B->x = arrivals_p->x;
			rdrops_B->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				rdrops_B->y << vector_rdrops_B.at(i);
			}

			subtitle = " - Random drops (bytes)";
			QOPlotWidget *plot_rdrops_B = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_rdrops_B->plot.title = title + subtitle;
			plot_rdrops_B->plot.data << QSharedPointer<QOPlotData>(rdrops_B);
			plot_rdrops_B->plot.drag_y_enabled = false;
			plot_rdrops_B->plot.zoom_y_enabled = false;
			plot_rdrops_B->fixAxes(0, 1000, 0, 2);
			plot_rdrops_B->drawPlot();
			accordion->addWidget(title + subtitle, plot_rdrops_B);

			queue_sampled->x = arrivals_p->x;
			queue_sampled->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				queue_sampled->y << vector_queue_sampled.at(i);
			}

			subtitle = " - Queue size (sampled)";
			QOPlotWidget *plot_queue_sampled = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_queue_sampled->plot.title = title + subtitle;
			plot_queue_sampled->plot.data << QSharedPointer<QOPlotData>(queue_sampled);
			plot_queue_sampled->plot.drag_y_enabled = false;
			plot_queue_sampled->plot.zoom_y_enabled = false;
			plot_queue_sampled->fixAxes(0, 1000, 0, 2);
			plot_queue_sampled->drawPlot();
			accordion->addWidget(title + subtitle, plot_queue_sampled);

			queue_max->x = arrivals_p->x;
			queue_max->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				queue_max->y << vector_queue_max.at(i);
			}

			subtitle = " - Queue size (interval maximums)";
			QOPlotWidget *plot_queue_max = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_queue_max->plot.title = title + subtitle;
			plot_queue_max->plot.data << QSharedPointer<QOPlotData>(queue_max);
			plot_queue_max->plot.drag_y_enabled = false;
			plot_queue_max->plot.zoom_y_enabled = false;
			plot_queue_max->fixAxes(0, 1000, 0, 2);
			plot_queue_max->drawPlot();
			accordion->addWidget(title + subtitle, plot_queue_max);

			queue_avg->x = arrivals_p->x;
			queue_avg->y.reserve(nelem);
			for (int i = 0; i < vector_timestamp.count(); i++) {
				queue_avg->y << vector_queue_avg.at(i);
			}

			subtitle = " - Queue size (interval mean)";
			QOPlotWidget *plot_queue_avg = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
			plot_queue_avg->plot.title = title + subtitle;
			plot_queue_avg->plot.data << QSharedPointer<QOPlotData>(queue_avg);
			plot_queue_avg->plot.drag_y_enabled = false;
			plot_queue_avg->plot.zoom_y_enabled = false;
			plot_queue_avg->fixAxes(0, 1000, 0, 2);
			plot_queue_avg->drawPlot();
			accordion->addWidget(title + subtitle, plot_queue_avg);
		}
	}

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
	emit tabResultsChanged();
}

void MainWindow::on_cmbSimulation_currentIndexChanged(int index)
{
	currentSimulation = index;

	// 0 is (new)
	if (currentSimulation > 0) {
		loadSimulation();
	}
}

void MainWindow::on_cmbTopologies_currentIndexChanged(int index)
{
	currentTopology = index;
	doReloadSimulationList();

	// 0 is (untitled)
	if (currentTopology > 0) {
		loadGraph(ui->cmbTopologies->itemText(currentTopology) + ".graph");
	}
}
