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

void MainWindow::on_btnNew_clicked()
{
	on_actionNew_triggered();
}

void MainWindow::on_btnOpen_clicked()
{
	on_actionOpen_triggered();
}

void MainWindow::on_btnSave_clicked()
{
	on_actionSave_triggered();
}

void MainWindow::on_btnSaveAs_clicked()
{
	on_actionSave_as_triggered();
}

void MainWindow::on_actionNew_triggered()
{
	netGraph.clear();
	scene.reload();
	setWindowTitle(QString("line-gui - %1").arg(getGraphName()));
	doTabTopologyChanged();
}

void MainWindow::on_actionOpen_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this,
										   "Open network graph", netGraph.fileName, "Network graph file (*.graph)");

	if (!fileName.isEmpty()) {
		loadGraph(fileName);
	}
}

bool MainWindow::on_actionSave_triggered()
{
	if (!netGraph.fileName.isEmpty()) {
		// save data to file
		if (!netGraph.saveToFile()) {
			QMessageBox::critical(this, "Save error", QString() + "Could not save file " + netGraph.fileName, QMessageBox::Ok);
			return false;
		}
		doSaveImage(getGraphName() + ".png");
		return true;
	} else {
		return on_actionSave_as_triggered();
	}
}

bool MainWindow::on_actionSave_as_triggered()
{
	QFileDialog fileDialog(this, "Save network graph");
	fileDialog.setFilter("Network graph file (*.graph)");
	fileDialog.setDefaultSuffix("graph");
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);

	QString fileName;
	if (fileDialog.exec()) {
		QStringList fileNames = fileDialog.selectedFiles();
		if (!fileNames.isEmpty())
			fileName = fileNames.first();
	}

	if (!fileName.isEmpty()) {
		netGraph.setFileName(fileName);
		if (on_actionSave_triggered()) {
			setWindowTitle(QString("line-gui - %1").arg(getGraphName()));
			return true;
		}
	}
	return false;
}

bool MainWindow::loadGraph(QString fileName, bool precomputed)
{
	QString oldName = netGraph.fileName;
	netGraph.setFileName(fileName);
	if (!netGraph.loadFromFile()) {
		QMessageBox::critical(this, "Open error", QString() + "Could not load file " + netGraph.fileName, QMessageBox::Ok);
		netGraph.setFileName(oldName);
		return false;
	}
	if (!precomputed) {
		netGraph.computeASHulls();
		computeRoutes();
	}

	scene.reload();
	setWindowTitle(QString("line-gui - %1").arg(getGraphName()));
	doTabTopologyChanged();
	return true;
}

void MainWindow::doSaveImage(QString fileName)
{
	NetGraphScene::EditMode oldMode = scene.editMode;
	scene.setMode(NetGraphScene::MoveNode);
	// save image
	QImage image(scene.sceneRect().width(), scene.sceneRect().height(), QImage::Format_RGB32);
	{
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.fillRect(image.rect(), Qt::white);
		scene.render(&painter);
	}
	image.save(fileName);

	QRect tightRect = scene.itemsBoundingRect().toRect();
	tightRect.translate(-scene.sceneRect().left(), -scene.sceneRect().top());
	tightRect.adjust(-10, -10, 10, 10);
	QImage imageCropped(tightRect.width(), tightRect.height(), QImage::Format_RGB32);
	{
		QPainter painter(&imageCropped);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.fillRect(imageCropped.rect(), Qt::white);
		painter.drawImage(imageCropped.rect(), image, tightRect);
	}
	fileName.replace(".png", "-cropped.png");
	imageCropped.save(fileName);
	scene.setMode(oldMode);
}
