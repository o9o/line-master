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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui>
#include <QFutureWatcher>

#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <float.h>
#include <math.h>

#include "netgraphscene.h"
#include "briteimporter.h"
#include "util.h"
#include "remoteprocessssh.h"
#include "qoplot.h"
#include "qdisclosure.h"
#include "qaccordion.h"

#include "../remote_config.h"

#define UDPPING_OVERHEAD 42

#define SEC_TO_NSEC  1000000000
#define MSEC_TO_NSEC 1000000
#define USEC_TO_NSEC 1000

#define TAB_INDEX_TOPOLOGY   0
#define TAB_INDEX_BRITE      1
#define TAB_INDEX_DEPLOY     2
#define TAB_INDEX_VALIDATION 3
#define TAB_INDEX_RUN        4
#define TAB_INDEX_RESULTS    5

#define TAB_COLOR_CHANGED Qt::darkGreen
#define TAB_COLOR_DEFAULT Qt::black

namespace Ui {
	class MainWindow;
}

class PathDelayMeasurement {
public:
	double fwdDelay;
	double backDelay;
	int frameSize;
};

class PathCongestionMeasurement {
public:
	double trafficBandwidth;
	double loss;
	int frameSize;
};

class PathQueueMeasurement {
public:
	double burstSize;
	double loss;
	int frameSize;
};

class ScalabilityMeasurement {
public:
	int flows;
	double totalMeasuredRate_kBps;
	double totalTheoreticalRate_kBps;
	double loss;
	int frameSize;
};

class Simulation {
public:
	explicit Simulation();
	explicit Simulation(QString dir);
	QString dir;
	QString graphName;
};

QString timeToString(quint64 value);

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

private:
	Ui::MainWindow *ui;

	// *** Topology editor
	NetGraphScene scene;
	NetGraph netGraph;

	QFutureWatcher<void> voidWatcher;
	BriteImporter briteImporter;

	QList<Simulation> simulations;
	int currentSimulation;
	int currentTopology;

	double simple_bw_KBps;
	int simple_delay_ms;
	bool simple_jitter;
	int simple_queueSize;
	double simple_bufferbloat_factor; // 1 = no bloat

	bool mustStop;


private slots:


private slots:
	void tick();
	void initPlots();
	void on_actionStop_triggered();
	void blockingOperationStarting();
	void blockingOperationFinished();
	QString getGraphName();
	QString getCoreHostname();
	QString getClientHostname(int index = 0);
	QStringList getClientHostnames();
	void doLogInformation(QTextEdit *log, QString message);
	void doLogError(QTextEdit *log, QString message);
	void doLogSuccess(QTextEdit *log, QString message);
	void doLogText(QTextEdit *log, QString message);
	void doLogOutput(QTextEdit *log, QString message);
	void doLogClear(QTextEdit *log);
	void doLogBriteInfo(QString s);
	void doLogBriteError(QString s);
	void updateTimeBox(QLineEdit *txt, bool &accepted, quint64 &value);
	void on_tabWidget_currentChanged(int index);
	void doTabTopologyChanged();
	void doTabBriteChanged();
	void doTabDeployChanged();
	void doTabValidationChanged();
	void doTabRunChanged();
	void doTabResultsChanged();

	// *** Open/save
	void on_actionNew_triggered();
	void on_actionOpen_triggered();
	bool on_actionSave_triggered();
	bool on_actionSave_as_triggered();
	void doSaveImage(QString fileName);
	bool loadGraph(QString fileName, bool precomputed = false);
	void on_btnNew_clicked();
	void on_btnOpen_clicked();
	void on_btnSave_clicked();
	void on_btnSaveAs_clicked();

	// *** Topology editor
	void on_sliderZoom_valueChanged(int value);
	void on_checkHideEdges_toggled(bool checked);
	void updateZoom();
	void on_checkUnusedHidden_toggled(bool checked);
	void on_checkHideConnections_toggled(bool checked);
	void on_checkHideFlows_toggled(bool checked);
	void on_spinZoom_valueChanged(double );
	void on_txtSamplingPeriod_textChanged(const QString &arg1);
	void onSamplingPeriodChanged(quint64 value);
	void on_checkSampledTimeline_toggled(bool checked);
	void on_btnDelEdge_toggled(bool checked);
	void on_btnDelNode_toggled(bool checked);
	void on_spinQueueLength_valueChanged(int );
	void on_spinASNumber_valueChanged(int );
	void on_txtConnectionType_textChanged(const QString &arg1);
	void on_spinLoss_valueChanged(double );
	void on_spinBandwidth_valueChanged(double );
	void on_spinDelay_valueChanged(int );
	void on_checkAutoQueue_toggled(bool checked);
	void autoAdjustQueue();
	void on_btnEditEdge_toggled(bool checked);
	void on_btnMoveNode_toggled(bool checked);
	void on_btnAddEdge_toggled(bool checked);
	void on_btnAddConnection_toggled(bool checked);
	void on_btnAddRouter_toggled(bool checked);
	void on_btnAddBorderRouter_toggled(bool checked);
	void on_btnAddGateway_toggled(bool checked);
	void on_btnAddHost_toggled(bool checked);
	void on_btnDelConnection_toggled(bool checked);
	void edgeSelected(NetGraphEdge edge);
	void on_btnDrag_toggled(bool checked);
	void on_btnShowRoutes_toggled(bool checked);

	// *** Validation
#define ENABLE_VALIDATION
#ifdef ENABLE_VALIDATION
	void delayValidation();
	void doDelayValidationFinished(QList<QOPlot> *plots);
	void lossBernoulliValidation();
	void congestionValidation();
	void doCongestionValidationFinished(QList<QOPlot> *plots);
	void queueingLossValidation();
	void doQueuingLossValidationFinished(QList<QOPlot> *plots);
	void queueingDelayValidation();
	void doQueuingDelayValidationFinished(QList<QOPlot> *plots);
	void doSshStressTest();
	void on_btn_QueueingDelay_clicked();
	void on_btnStartQueueingLossValidation_clicked();
	void on_btnStartCongestionValidation_clicked();
	void on_btnStartLossBernValidation_clicked();
	void on_btnStartDelayValidation_clicked();
	void on_btnSshStressTest_clicked();
	void measureDelay(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
				   int frame_size, double rate_kBps, long long timeout_us,
				   QList<PathDelayMeasurement> &data);
	void measureBernoulliLoss(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
						 double ratekBps, int frame_size, double &loss);
	void measureCongestionLoss(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
						  double rate_kBps, int frame_size,
						  double &loss, double &measuredBitrate_kBps);
	void measureQueueingLoss(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
						int frame_size, int burst_size,
						long long interval_us, double &loss);
	void theoreticalQueueingLoss(NetGraphPath p, int burst_size, double &loss);
	void measureQueueingDelay(QTextEdit *log, NetGraphNode src, NetGraphNode dst,
								   int frame_size, int rate_kBps, int runTime_s,
								   QList<quint64> &delays_us);
#endif

#ifdef ENABLE_SCALABILITY
	// *** Scalability TODO
	void on_btnBatchScalability_clicked();
	void checkScalability();
	void plotScalability(QList<ScalabilityMeasurement> measurements);
#endif

	// *** Batch
	void on_btnGeneric_clicked();
	void on_btnGenericStop_clicked();
	void on_checkGenericTimeout_toggled(bool checked);
	void generateSimpleTopology(int pairs);
	void on_spinBW_valueChanged(int arg1);
	void on_spinPathDelay_valueChanged(int arg1);
	void on_checkJitter_toggled(bool checked);
	void on_doubleSpinBox_valueChanged(double arg1);
	QString doGenericSimulation(int timeout = 0);
	void on_btn4x10_clicked();
	void on_btnY_clicked();
	void on_btnAdd10TCP_clicked();
	void on_txtGenericTimeout_textChanged(const QString &arg1);

	// *** Deployment
	void on_btnDeploy_clicked();
	void on_btnComputeRoutes_clicked();
	void computeRoutes();
	void generateHostDeploymentScript();
	void deploy();

	// *** BRITE
	void on_btnBriteImport_clicked();
	void importBrite(QString fomeFileName, QString toFileName);
	void onImportFinished(QString fileName);
	void doLayoutGraph();
	void layoutGraph();
	void onLayoutFinished();
	void doRecomputeRoutes();
	void recomputeRoutes();
	void onRoutingFinished();

	// *** Remote
	bool runCommand(QTextEdit *log, QString command, QStringList args, QString description);
	bool runCommand(QTextEdit *log, QString command, QStringList args, QString description, bool showOutput, QString &output);
	bool startProcess(QTextEdit *log, QProcess &process, QString command, QStringList args, QString description, QString &output);
	bool stopProcess(QTextEdit *log, QProcess &process, QString description, QString stopCommand, QString &output);

	// *** Results
	void doReloadTopologyList();
	void doReloadSimulationList();
	void loadSimulation();
	void on_cmbSimulation_currentIndexChanged(int index);
	void on_cmbTopologies_currentIndexChanged(int index);

signals:
	void logInformation(QTextEdit *log, QString message);
	void logError(QTextEdit *log, QString message);
	void logSuccess(QTextEdit *log, QString message);
	void logText(QTextEdit *log, QString message);
	void logOutput(QTextEdit *log, QString message);
	void logClear(QTextEdit *log);
	void tabTopologyChanged();
	void tabBriteChanged();
	void tabDeployChanged();
	void tabValidationChanged();
	void tabRunChanged();
	void tabResultsChanged();
	void reloadScene();
	void saveGraph();
	void saveImage(QString fileName);
	void importFinished(QString fileName);
	void layoutFinished();
	void routingFinished();
	void routingChanged();
	void usedChanged();
	void reloadTopologyList();
	void reloadSimulationList();
	void delayValidationFinished(QList<QOPlot> *plots);
	void congestionValidationFinished(QList<QOPlot> *plots);
	void queuingLossValidationFinished(QList<QOPlot> *plots);
	void queuingDelayValidationFinished(QList<QOPlot> *plots);
};

#endif // MAINWINDOW_H
