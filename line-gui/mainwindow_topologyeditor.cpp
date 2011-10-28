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

void MainWindow::on_sliderZoom_valueChanged(int )
{
	qreal scaleExp = ui->sliderZoom->value() / 100.0;
	qreal scaleFactor = pow(10.0, scaleExp);
	qreal zoomPercent = scaleFactor * 100;
	ui->spinZoom->setValue(zoomPercent);

	updateZoom();
}

void MainWindow::on_checkUnusedHidden_toggled(bool checked)
{
	netGraph.updateUsed();
	scene.usedChanged();
	scene.setUnusedHidden(checked);
}

void MainWindow::on_checkHideConnections_toggled(bool checked)
{
	scene.setHideConnections(checked);
}

void MainWindow::on_checkHideFlows_toggled(bool checked)
{
	scene.setHideFlows(checked);
}

void MainWindow::on_checkHideEdges_toggled(bool checked)
{
	scene.setHideEdges(checked);
}


void MainWindow::on_spinZoom_valueChanged(double )
{
	qreal targetScale = ui->spinZoom->value() / 100.0;
	qreal scaleExp = log10(targetScale);
	ui->sliderZoom->setValue((int)(scaleExp * 100));
}

void MainWindow::updateZoom()
{
	qreal targetScale = ui->spinZoom->value() / 100.0;
	qreal scaleFactor = targetScale / ui->graphicsView->transform().m11();

	ui->graphicsView->scale(scaleFactor, scaleFactor);
}

void MainWindow::on_txtSamplingPeriod_textChanged(const QString &)
{
	bool accepted;
	quint64 value;
	updateTimeBox(ui->txtSamplingPeriod, accepted, value);
	if (accepted)
		onSamplingPeriodChanged(value);
}

void MainWindow::on_btnAddConnection_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::InsertConnectionStart);
		ui->stackedWidget->setCurrentWidget(ui->pageAddConnection);
	}
}

void MainWindow::on_btnDelConnection_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::DelConnection);
		ui->stackedWidget->setCurrentWidget(ui->pageDeleteConnection);
	}
}

void MainWindow::on_btnMoveNode_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::MoveNode);
		ui->stackedWidget->setCurrentWidget(ui->pageMoveNode);
	}
}

void MainWindow::on_btnEditEdge_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::EditEdge);
		ui->stackedWidget->setCurrentWidget(ui->pageAddEditLink);
	}
}

void MainWindow::on_btnDelNode_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::DelNode);
		ui->stackedWidget->setCurrentWidget(ui->pageDeleteNode);
	}
}

void MainWindow::on_btnDelEdge_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::DelEdge);
		ui->stackedWidget->setCurrentWidget(ui->pageDeleteLink);
	}
}

void MainWindow::on_btnDrag_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::Drag);
		ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
		ui->stackedWidget->setCurrentWidget(ui->pageDrag);
	} else {
		ui->graphicsView->setDragMode(QGraphicsView::NoDrag);
	}
}

void MainWindow::on_spinDelay_valueChanged(int val)
{
	scene.delayChanged(val);
	if (ui->checkAutoQueue->isChecked())
		autoAdjustQueue();
}

void MainWindow::on_spinBandwidth_valueChanged(double val)
{
	scene.bandwidthChanged(val);
	if (ui->checkAutoQueue->isChecked())
		autoAdjustQueue();
}

void MainWindow::on_spinLoss_valueChanged(double val)
{
	scene.lossRateChanged(val);
}

void MainWindow::on_spinQueueLength_valueChanged(int val)
{
	scene.queueLenghtChanged(val);
}

void MainWindow::on_spinASNumber_valueChanged(int val)
{
	scene.ASNumberChanged(val);
}

void MainWindow::on_txtConnectionType_textChanged(const QString &arg1)
{
	scene.connectionTypeChanged(arg1);
}

void MainWindow::onSamplingPeriodChanged(quint64 value)
{
	scene.samplingPeriodChanged(value);
}

void MainWindow::on_checkSampledTimeline_toggled(bool checked)
{
	scene.samplingChanged(checked);
}

void MainWindow::on_btnAddHost_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::InsertHost);
		ui->stackedWidget->setCurrentWidget(ui->pageAddNode);
	}
}

void MainWindow::on_btnAddGateway_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::InsertGateway);
		ui->stackedWidget->setCurrentWidget(ui->pageAddNode);
	}
}

void MainWindow::on_btnAddRouter_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::InsertRouter);
		ui->stackedWidget->setCurrentWidget(ui->pageAddNode);
	}
}

void MainWindow::on_btnAddBorderRouter_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::InsertBorderRouter);
		ui->stackedWidget->setCurrentWidget(ui->pageAddNode);
	}
}

void MainWindow::on_btnAddEdge_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::InsertEdgeStart);
		ui->stackedWidget->setCurrentWidget(ui->pageAddEditLink);
	}
}

void MainWindow::autoAdjustQueue()
{
	ui->spinQueueLength->setValue(NetGraph::optimalQueueLength(ui->spinBandwidth->value(), ui->spinDelay->value()));
}

void MainWindow::on_checkAutoQueue_toggled(bool checked)
{
	if (checked) {
		autoAdjustQueue();
	}
}

void MainWindow::edgeSelected(NetGraphEdge edge)
{
	ui->spinDelay->setValue(edge.delay_ms);
	ui->spinBandwidth->setValue(edge.bandwidth);
	ui->spinLoss->setValue(edge.lossBernoulli);
	ui->spinQueueLength->setValue(edge.queueLength);
	ui->checkSampledTimeline->setChecked(edge.recordSampledTimeline);
	ui->txtSamplingPeriod->setText(timeToString(edge.timelineSamplingPeriod));
}

void MainWindow::on_btnShowRoutes_toggled(bool checked)
{
	if (checked) {
		scene.setMode(NetGraphScene::ShowRoutes);
		ui->stackedWidget->setCurrentWidget(ui->pageShowRoutes);
	}
}
