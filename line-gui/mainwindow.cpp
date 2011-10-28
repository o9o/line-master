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
#include "flowlayout.h"

#include <QtConcurrentRun>
#include <QtXml>

MainWindow::MainWindow(QWidget *parent) :
		QMainWindow(parent),
		ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	{
		QRegExp rx("[1-9][0-9]*(s|ms|us|ns)");
		QValidator *validator = new QRegExpValidator(rx, this);
		ui->txtSamplingPeriod->setValidator(validator);
		ui->txtGenericTimeout->setValidator(validator);
	}

	{
		delete ui->buttonBar->layout();
		FlowLayout *layout = new FlowLayout;
		foreach (QObject *c, ui->buttonBar->children()) {
			QWidget *w = dynamic_cast<QWidget*>(c);
			if (w)
				layout->addWidget(w);
		}
		ui->buttonBar->setLayout(layout);
	}

	connect(this, SIGNAL(logError(QTextEdit*,QString)), SLOT(doLogError(QTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logInformation(QTextEdit*,QString)), SLOT(doLogInformation(QTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logText(QTextEdit*,QString)), SLOT(doLogText(QTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logOutput(QTextEdit*,QString)), SLOT(doLogOutput(QTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logSuccess(QTextEdit*,QString)), SLOT(doLogSuccess(QTextEdit*,QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(logClear(QTextEdit*)), SLOT(doLogClear(QTextEdit*)), Qt::QueuedConnection);
	connect(this, SIGNAL(tabTopologyChanged()), SLOT(doTabTopologyChanged()), Qt::QueuedConnection);
	connect(this, SIGNAL(tabBriteChanged()), SLOT(doTabBriteChanged()), Qt::QueuedConnection);
	connect(this, SIGNAL(tabDeployChanged()), SLOT(doTabDeployChanged()), Qt::QueuedConnection);
	connect(this, SIGNAL(tabRunChanged()), SLOT(doTabRunChanged()), Qt::QueuedConnection);
	connect(this, SIGNAL(tabResultsChanged()), SLOT(doTabResultsChanged()), Qt::QueuedConnection);
	connect(this, SIGNAL(tabValidationChanged()), SLOT(doTabValidationChanged()), Qt::QueuedConnection);
	connect(this, SIGNAL(importFinished(QString)), SLOT(onImportFinished(QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(layoutFinished()), SLOT(onLayoutFinished()), Qt::QueuedConnection);
	connect(this, SIGNAL(routingFinished()), SLOT(onRoutingFinished()), Qt::QueuedConnection);
	connect(this, SIGNAL(saveImage(QString)), SLOT(doSaveImage(QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(reloadSimulationList()), SLOT(doReloadSimulationList()), Qt::QueuedConnection);
	connect(this, SIGNAL(reloadTopologyList()), SLOT(doReloadTopologyList()), Qt::QueuedConnection);
	connect(this, SIGNAL(delayValidationFinished(QList<QOPlot>*)), SLOT(doDelayValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	connect(this, SIGNAL(congestionValidationFinished(QList<QOPlot>*)), SLOT(doCongestionValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	connect(this, SIGNAL(queuingLossValidationFinished(QList<QOPlot>*)), SLOT(doQueuingLossValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	connect(this, SIGNAL(queuingDelayValidationFinished(QList<QOPlot>*)), SLOT(doQueuingDelayValidationFinished(QList<QOPlot>*)), Qt::QueuedConnection);
	connect(&voidWatcher, SIGNAL(finished()), SLOT(blockingOperationFinished()));
	connect(this, SIGNAL(reloadScene()), &scene, SLOT(reload()), Qt::QueuedConnection);
	connect(this, SIGNAL(saveGraph()), SLOT(on_actionSave_triggered()), Qt::QueuedConnection);
	connect(&briteImporter, SIGNAL(logInfo(QString)), SLOT(doLogBriteInfo(QString)), Qt::QueuedConnection);
	connect(&briteImporter, SIGNAL(logError(QString)), SLOT(doLogBriteError(QString)), Qt::QueuedConnection);
	connect(this, SIGNAL(routingChanged()), &scene, SLOT(routingChanged()), Qt::QueuedConnection);
	connect(this, SIGNAL(usedChanged()), &scene, SLOT(usedChanged()), Qt::QueuedConnection);

	connect(ui->txtBrite, SIGNAL(textChanged()), SLOT(doTabBriteChanged()));
	connect(ui->txtDeploy, SIGNAL(textChanged()), SLOT(doTabDeployChanged()));
	connect(ui->txtValidation, SIGNAL(textChanged()), SLOT(doTabValidationChanged()));
	connect(ui->txtBatch, SIGNAL(textChanged()), SLOT(doTabRunChanged()));

	{
		QDir d("..");
		d.mkdir("line-topologies");
	}
	QDir::setCurrent("../line-topologies");

	scene.netGraph = &netGraph;
	scene.setSceneRect(QRectF(0, 0, 500, 500));

	ui->graphicsView->setScene(&scene);

	QButtonGroup *btnGroup = new QButtonGroup(this);
	btnGroup->addButton(ui->btnAddHost);
	btnGroup->addButton(ui->btnAddGateway);
	btnGroup->addButton(ui->btnAddRouter);
	btnGroup->addButton(ui->btnAddBorderRouter);
	btnGroup->addButton(ui->btnAddEdge);
	btnGroup->addButton(ui->btnAddConnection);
	btnGroup->addButton(ui->btnDelConnection);
	btnGroup->addButton(ui->btnMoveNode);
	btnGroup->addButton(ui->btnEditEdge);
	btnGroup->addButton(ui->btnDelNode);
	btnGroup->addButton(ui->btnDelEdge);
	btnGroup->addButton(ui->btnDrag);
	btnGroup->addButton(ui->btnShowRoutes);
	ui->btnDrag->setChecked(true);

	this->addAction(ui->actionNew);
	this->addAction(ui->actionOpen);
	this->addAction(ui->actionSave);
	this->addAction(ui->actionSave_as);

	connect(&scene, SIGNAL(edgeSelected(NetGraphEdge)), SLOT(edgeSelected(NetGraphEdge)));
	on_spinQueueLength_valueChanged(ui->spinQueueLength->value());
	on_spinLoss_valueChanged(ui->spinLoss->value());
	on_spinBandwidth_valueChanged(ui->spinBandwidth->value());
	on_spinDelay_valueChanged(ui->spinDelay->value());

	on_checkGenericTimeout_toggled(ui->checkGenericTimeout->isChecked());

#ifdef ENABLE_BATCH
	on_spinBW_valueChanged(ui->spinBW->value());
	on_spinPathDelay_valueChanged(ui->spinPathDelay->value());
	on_checkJitter_toggled(ui->checkJitter->isChecked());
	on_doubleSpinBox_valueChanged(ui->doubleSpinBox->value());
#endif

	currentSimulation = -1;

	initPlots();

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
	timer->start(1000);

	emit reloadTopologyList();
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::initPlots()
{
#define DEBUG_PLOTS 0
#if DEBUG_PLOTS
	foreach (QObject *obj, ui->scrollPlotsWidgetContents->children()) {
		delete obj;
	}

	QVBoxLayout *verticalLayout = new QVBoxLayout(ui->scrollPlotsWidgetContents);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(11, 11, 11, 11);

	QAccordion *accordion = new QAccordion(ui->scrollPlotsWidgetContents);

	accordion->addLabel("Emulator");
	QOPlotWidget *plot = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
	plot->plot.title = "Zzzz";

	QOPlotStemData *stem = new QOPlotStemData();
	for (int i = 0; i < 10; i++) {
		stem->x.append(i);
		stem->y.append(i);
	}
	stem->pen = QPen(Qt::blue);
	stem->legendLabel = "Events (0 = forward, 1 = drop)";
	plot->plot.addData(stem);
	plot->plot.drag_y_enabled = false;
	plot->plot.zoom_y_enabled = false;
	plot->fixAxes(0, 1000, 0, 2);
	plot->drawPlot();
	accordion->addWidget("Zzzz", plot);

	plot = new QOPlotWidget(accordion, 0, 300, QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
	plot->plot.title = "Zzzz";
	QOPlotCurveData *curve = new QOPlotCurveData();
	for (int i = 0; i < 10; i++) {
		curve->x.append(i);
		curve->y.append(sqrt(i));
	}
	curve->pen = QPen(Qt::blue);
	curve->legendLabel = "Events (0 = forward, 1 = drop)";
	plot->plot.addData(curve);
	plot->fixAxes(0, 1000, 0, 2);
	plot->drawPlot();
	accordion->addWidget("Zzzz", plot);

	ui->scrollPlotsWidgetContents->layout()->addWidget(accordion);
#endif

//	QHBoxLayout *layout = (QHBoxLayout *)ui->tab_8->layout();
//	QDisclosure *a = new QDisclosure(ui->tab_8);
//	QLabel *aTxt = new QLabel("Zzzzzzzz...");
//	a->setWidget(aTxt);
//	if (layout)
//		layout->addWidget(a);
}

void MainWindow::tick()
{
//	plot1->appendData(random() % 100, random() % 100);
//	plot1->replot();
}

QString timeToString(quint64 value)
{
	if (value == 0)
		return "0ns";

	quint64 s, ms, us, ns;

	s = (value / 1000ULL / 1000ULL / 1000ULL) % 60ULL;
	ms = (value / 1000ULL / 1000ULL) % 1000ULL;
	us = (value / 1000ULL) % 1000ULL;
	ns = value % 1000ULL;

	if (ns) {
		return QString::number(value) + "ns";
	} else if (us) {
		return QString::number(value / 1000ULL) + "us";
	} else if (ms) {
		return QString::number(value / 1000ULL / 1000ULL) + "ms";
	} else {
		return QString::number(value / 1000ULL / 1000ULL / 1000ULL) + "s";
	}
}

void MainWindow::on_actionStop_triggered()
{
	mustStop = true;
}

QString MainWindow::getGraphName()
{
	QString graphName = netGraph.fileName.split('/', QString::SkipEmptyParts).last();
	graphName = graphName.replace(".graph", "");
	return graphName;
}

QString MainWindow::getCoreHostname()
{
	return REMOTE_HOST_ROUTER;
}

QString MainWindow::getClientHostname(int )
{
	return REMOTE_HOST_HOSTS;
}

QStringList MainWindow::getClientHostnames()
{
	return QStringList() << getClientHostname(0);
}

void MainWindow::doLogInformation(QTextEdit *log, QString message)
{
	log->setTextColor(Qt::darkCyan);
	log->append(QString("[%1] %2").arg(QTime::currentTime().toString("hh:mm:ss"), message));
}

void MainWindow::doLogError(QTextEdit *log, QString message)
{
	log->setTextColor(Qt::red);
	log->append(message);
}

void MainWindow::doLogSuccess(QTextEdit *log, QString message)
{
	log->setTextColor(Qt::darkGreen);
	log->append(message);
}

void MainWindow::doLogText(QTextEdit *log, QString message)
{
	log->setTextColor(Qt::black);
	log->append(message);
}

void MainWindow::doLogOutput(QTextEdit *log, QString message)
{
	log->setTextColor(Qt::darkBlue);
	QFont oldFont = log->font();
	QFont newFont = oldFont;
	newFont.setStyleHint(QFont::TypeWriter);
	newFont.setFamily("Liberation Mono");
	log->setCurrentFont(newFont);
	log->append(message);
	log->setCurrentFont(oldFont);
}

void MainWindow::doLogClear(QTextEdit *log)
{
	log->clear();
}

void MainWindow::doLogBriteInfo(QString s)
{
	ui->txtBrite->setTextColor(Qt::black);
	ui->txtBrite->append(s);
	emit tabBriteChanged();
}

void MainWindow::doLogBriteError(QString s)
{
	ui->txtBrite->setTextColor(Qt::red);
	ui->txtBrite->append(s);
	emit tabBriteChanged();
}

void MainWindow::blockingOperationStarting()
{
	// lock gui
	ui->btnDeploy->setEnabled(false);
	ui->btnStartDelayValidation->setEnabled(false);
	ui->btnStartCongestionValidation->setEnabled(false);
	ui->btnStartLossBernValidation->setEnabled(false);
	ui->btnStartQueueingLossValidation->setEnabled(false);
	ui->btn_QueueingDelay->setEnabled(false);
	ui->btnBatchScalability->setEnabled(false);
	ui->btnSshStressTest->setEnabled(false);
	mustStop = false;
}

void MainWindow::blockingOperationFinished()
{
	// unlock gui
	ui->btnDeploy->setEnabled(true);
	ui->btnStartDelayValidation->setEnabled(true);
	ui->btnStartCongestionValidation->setEnabled(true);
	ui->btnStartLossBernValidation->setEnabled(true);
	ui->btnStartQueueingLossValidation->setEnabled(true);
	ui->btn_QueueingDelay->setEnabled(true);
	ui->btnBatchScalability->setEnabled(true);
	ui->btnSshStressTest->setEnabled(true);
	mustStop = false;
}

void MainWindow::updateTimeBox(QLineEdit *txt, bool &accepted, quint64 &value)
{
	accepted = false;
	const QValidator *validator = txt->validator();

	if (validator) {
		QString text = txt->text();
		int pos = txt->cursorPosition();
		QValidator::State state = validator->validate(text, pos);
		if (state == QValidator::Acceptable) {
			txt->setStyleSheet("color:black");
			value = 0;
			quint64 multiplier = 1;
			if (text.endsWith("ns")) {
				text = text.replace("ns", "");
			} else if (text.endsWith("us")) {
				text = text.replace("us", "");
				multiplier = 1000;
			} else if (text.endsWith("ms")) {
				text = text.replace("ms", "");
				multiplier = 1000000;
			} else if (text.endsWith("s")) {
				text = text.replace("s", "");
				multiplier = 1000000000;
			}
			value = (quint64)text.toLongLong() * multiplier;
			accepted = true;
		} else if (state == QValidator::Intermediate) {
			txt->setStyleSheet("color:blue");
		} else if (state == QValidator::Invalid) {
			txt->setStyleSheet("color:red");
		}
	}
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
	ui->tabWidget->tabBar()->setTabTextColor(index, TAB_COLOR_DEFAULT);
}

void MainWindow::doTabTopologyChanged()
{
	if (ui->tabWidget->tabBar()->currentIndex() != TAB_INDEX_TOPOLOGY)
		ui->tabWidget->tabBar()->setTabTextColor(TAB_INDEX_TOPOLOGY, TAB_COLOR_CHANGED);
}

void MainWindow::doTabBriteChanged()
{
	if (ui->tabWidget->tabBar()->currentIndex() != TAB_INDEX_BRITE)
		ui->tabWidget->tabBar()->setTabTextColor(TAB_INDEX_BRITE, TAB_COLOR_CHANGED);
}

void MainWindow::doTabDeployChanged()
{
	if (ui->tabWidget->tabBar()->currentIndex() != TAB_INDEX_DEPLOY)
		ui->tabWidget->tabBar()->setTabTextColor(TAB_INDEX_DEPLOY, TAB_COLOR_CHANGED);
}

void MainWindow::doTabValidationChanged()
{
	if (ui->tabWidget->tabBar()->currentIndex() != TAB_INDEX_VALIDATION)
		ui->tabWidget->tabBar()->setTabTextColor(TAB_INDEX_VALIDATION, TAB_COLOR_CHANGED);
}

void MainWindow::doTabRunChanged()
{
	if (ui->tabWidget->tabBar()->currentIndex() != TAB_INDEX_RUN)
		ui->tabWidget->tabBar()->setTabTextColor(TAB_INDEX_RUN, TAB_COLOR_CHANGED);
}

void MainWindow::doTabResultsChanged()
{
	if (ui->tabWidget->tabBar()->currentIndex() != TAB_INDEX_RESULTS)
		ui->tabWidget->tabBar()->setTabTextColor(TAB_INDEX_RESULTS, TAB_COLOR_CHANGED);
}
