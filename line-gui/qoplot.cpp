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

#include "qoplot.h"

#include <QtOpenGL>
#include <QSvgGenerator>

#include "nicelabel.h"

QOPlot::QOPlot()
{
	xGridVisible = true;
	yGridVisible = true;
	legendVisible = true;
	legendPosition = TopRight;
	legendAlpha = 200;

	backgroundColor = Qt::white;
	foregroundColor = Qt::black;
	legendBackground = QColor(240, 240, 240);

	drag_x_enabled = drag_y_enabled = zoom_x_enabled = zoom_y_enabled = true;
	autoAdjusted = true;
	autoAdjustedFixedAspect = fixedAxes = false;
	includeOriginX = includeOriginY = true;
}

QOPlot::~QOPlot()
{
	data.clear();
}

QOPlotData::QOPlotData()
{
	legendVisible = true;
	legendLabel = "";
	legendLabelColor = Qt::black;
}

QOPlotCurveData::QOPlotCurveData() : QOPlotData()
{
	dataType = "line";
	pen = QPen(Qt::blue, 1.0, Qt::SolidLine);
	pen.setCosmetic(true);
	pointSymbol = QString();
	symbolSize = 6.0;
	x = QVector<qreal>();
	y = QVector<qreal>();
	worldWidthResampled = -1;
	xResampled = QVector<qreal>();
	yResampled = QVector<qreal>();
}

void QOPlotCurveData::boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax)
{
	if (x.count() > 0) {
		xmin = xmax = x[0];
		ymin = ymax = y[0];
	} else {
		xmin = 0; xmax = 1;
		ymin = 0; ymax = 1;
	}
	for (int i = 0; i < x.count(); i++) {
		if (x[i] < xmin)
			xmin = x[i];
		if (x[i] > xmax)
			xmax = x[i];
		if (y[i] < ymin)
			ymin = y[i];
		if (y[i] > ymax)
			ymax = y[i];
	}
}

QOPlotStemData::QOPlotStemData() : QOPlotCurveData()
{
	dataType = "stem";
	pointSymbol = "o";
}

QOPlotScatterData::QOPlotScatterData() : QOPlotCurveData()
{
	dataType = "scatter";
	pointSymbol = "x";
}

class QGraphicsDummyItem : public QGraphicsItem
{
public:
	explicit QGraphicsDummyItem(QGraphicsItem *parent = 0) : QGraphicsItem(parent) {
		setFlag(QGraphicsItem::ItemHasNoContents);
	}

	QRectF bbox;
	QRectF boundingRect() const
	{
		return bbox;
	}

	void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *)
	{
	}
};

class QGraphicsAATextItem : public QGraphicsTextItem
{
public:
	explicit QGraphicsAATextItem(QGraphicsItem *parent = 0) : QGraphicsTextItem(parent) { }
	explicit QGraphicsAATextItem(const QString &text, QGraphicsItem *parent = 0) : QGraphicsTextItem(text, parent) { }

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		QPainter::RenderHints hints = painter->renderHints();
		painter->setRenderHint(QPainter::Antialiasing, true);
		QGraphicsTextItem::paint(painter, option, widget);
		painter->setRenderHints(hints, false);
		painter->setRenderHints(hints, true);
	}
};

QOPlotWidget::QOPlotWidget(QWidget *parent, int minWidth, int minHeight,
			QSizePolicy policy) :
	QWidget(parent), minWidth(minWidth), minHeight(minHeight)
{
	setStyle(NULL);
	setStyleSheet("QWidget { border: 0px; }");
	setMinSize(minWidth, minHeight);
	setSizePolicy(policy);
	setFocusPolicy(Qt::StrongFocus);
	scenePlot = new QGraphicsScene();
	viewPlot = new QOGraphicsView(scenePlot, this);

	scenePlot->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
//	scenePlot->setItemIndexMethod(QGraphicsScene::NoIndex);
	viewPlot->setGeometry(0, 0, width(), height());
	viewPlot->setStyle(NULL);
	viewPlot->setAlignment(0);
	viewPlot->setRenderHint(QPainter::Antialiasing, false);
	viewPlot->setDragMode(QGraphicsView::NoDrag);
	viewPlot->setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
	viewPlot->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);

	connect(viewPlot, SIGNAL(mouseMoved(QMouseEvent*)), SLOT(mouseMoveEvent(QMouseEvent*)));
	connect(viewPlot, SIGNAL(mousePressed(QMouseEvent*)), SLOT(mousePressEvent(QMouseEvent*)));
	connect(viewPlot, SIGNAL(mouseReleased(QMouseEvent*)), SLOT(mouseReleaseEvent(QMouseEvent*)));
	connect(viewPlot, SIGNAL(wheelMoved(QWheelEvent*)), SLOT(wheelEvent(QWheelEvent*)));

	marginLeft = marginRight = marginTop = marginBottom = 50;
	world_x_off = 0;
	world_y_off = 0;
	world_press_x = NAN;
	world_press_y = NAN;
	zoomX = zoomY = 1.0;

	resampleAllowed = false;
	cullingAllowed = false;

	itemPlot = new QGraphicsDummyItem();
	scenePlot->addItem(itemPlot);
	itemPlot->setZValue(1);

	itemLegend = new QGraphicsDummyItem();
	scenePlot->addItem(itemLegend);
	itemLegend->setZValue(1000);

	bg = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	bg->setZValue(-1000);

	axesBgLeft = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgLeft->setZValue(100);
	axesBgRight = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgRight->setZValue(100);
	axesBgTop = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgTop->setZValue(100);
	axesBgBottom = scenePlot->addRect(0, 0, 1, 1, QPen(Qt::NoPen), QBrush(plot.backgroundColor));
	axesBgBottom->setZValue(100);
	plotAreaBorder = scenePlot->addRect(0, 0, 1, 1, QPen(plot.foregroundColor), QBrush(Qt::NoBrush));
	plotAreaBorder->setZValue(200);

	xLabel = scenePlot->addText("X Axis");
	xLabel->setZValue(300);
	yLabel = scenePlot->addText("Y Axis");
	yLabel->setZValue(300);
	titleLabel = scenePlot->addText("Title");
	titleLabel->setZValue(300);

	drawPlot();
}

void QOPlotWidget::setPlot(QOPlot plot)
{
	this->plot = plot;
	if (plot.autoAdjusted) {
		autoAdjustAxes();
	} else if (plot.autoAdjustedFixedAspect) {
		autoAdjustAxes(plot.autoAspectRatio);
	} else if (plot.fixedAxes) {
		fixAxes(plot.fixedXmin, plot.fixedXmax, plot.fixedYmin, plot.fixedYmax);
	}
	drawPlot();
}

void QOPlotWidget::setAntiAliasing(bool enable)
{
	if (enable) {
		viewPlot->setRenderHint(QPainter::Antialiasing, true);
		viewPlot->setOptimizationFlags(QGraphicsView::DontSavePainterState);
	} else {
		viewPlot->setRenderHint(QPainter::Antialiasing, false);
		viewPlot->setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
	}
}

void QOPlotWidget::test()
{
	plot.xlabel = "Teh X axis";
	plot.ylabel = "Teh Y axis";
	plot.title = "Awesome example";

	QOPlotCurveData *curve1 = new QOPlotCurveData;
	curve1->x.append(-10);
	curve1->y.append(-10);
	curve1->x.append(0);
	curve1->y.append(0);
	curve1->x.append(10);
	curve1->y.append(10);
	curve1->pen.setColor(Qt::blue);
	curve1->pen.setStyle(Qt::SolidLine);
	curve1->pointSymbol = "*";
	curve1->legendLabel = "Blah";
	plot.data << QSharedPointer<QOPlotData>(curve1);

	QOPlotCurveData *curve2 = new QOPlotCurveData;
	curve2->x.append(-5);
	curve2->y.append(-10);
	curve2->x.append(5);
	curve2->y.append(10);
	curve2->pen.setColor(Qt::green);
	curve2->pen.setStyle(Qt::DotLine);
	curve2->pointSymbol = "o";
	curve2->legendLabel = "Boo";
	plot.data << QSharedPointer<QOPlotData>(curve2);

	QOPlotStemData *stem1 = new QOPlotStemData;
	for (double x = -10; x  <= 10.001; x += 2) {
		stem1->x.append(x);
		stem1->y.append(0.1 + sqrt(fabs(x)));
	}
	stem1->pen.setColor(Qt::red);
	stem1->legendLabel = "Teh stem";
	plot.data << QSharedPointer<QOPlotData>(stem1);

	QOPlotScatterData *scatter1 = new QOPlotScatterData;
	for (double x = -10; x  <= 10.001; x += 2) {
		scatter1->x.append(x);
		scatter1->y.append(0.5 + 3 * sqrt(fabs(x)));
	}
	scatter1->pen.setColor(Qt::gray);
	scatter1->legendLabel = "Teh scatter";
	plot.data << QSharedPointer<QOPlotData>(scatter1);

	drawPlot();
}

void QOPlotWidget::testHuge()
{
	plot.xlabel = "Teh X axis";
	plot.ylabel = "Teh Y axis";
	plot.title = "Awesome example (huge)";

	qreal xmin, xmax, step;
	xmin = 0;
	xmax = 1000;
	step = 0.1;

	QOPlotCurveData *curve1 = new QOPlotCurveData;

	for (double x = xmin;  x <= xmax; x += step) {
		curve1->x.append(x);
		curve1->y.append(sin(x));
	}
	curve1->pen.setColor(Qt::blue);
	curve1->legendLabel = "sin(x)";
	plot.data << QSharedPointer<QOPlotData>(curve1);

	drawPlot();
}

void drawPlus(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	QGraphicsLineItem *line = new QGraphicsLineItem(size/2.0, 0, size/2.0, 0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	line = new QGraphicsLineItem(0, -size/2.0, 0, size/2.0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

void drawCross(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	QGraphicsLineItem *line = new QGraphicsLineItem(-size/2.0, -size/2.0, size/2.0, size/2.0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
	line = new QGraphicsLineItem(-size/2.0, size/2.0, size/2.0, -size/2.0, parent);
	line->setPen(pen);
	line->setZValue(z);
	line->setPos(x, y);
	line->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

void drawStar(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	drawPlus(size, parent, x, y, z, pen);
	drawCross(size, parent, x, y, z, pen);
}

void drawCircle(qreal size, QGraphicsItem *parent, qreal x, qreal y, qreal z, QPen pen)
{
	QGraphicsEllipseItem *circle = new QGraphicsEllipseItem(-size/2.0, -size/2.0, size, size, parent);
	circle->setPen(pen);
	circle->setBrush(pen.color());
	circle->setZValue(z);
	circle->setPos(x, y);
	circle->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

QPen changeAlpha(QPen pen, int alpha)
{
	QPen result (pen);
	result.setColor(QColor(pen.color().red(), pen.color().green(), pen.color().blue(), alpha));
	return result;
}

QBrush changeAlpha(QBrush brush, int alpha)
{
	QBrush result (brush);
	result.setColor(QColor(brush.color().red(), brush.color().green(), brush.color().blue(), alpha));
	return result;
}

QColor changeAlpha(QColor c, int alpha)
{
	return QColor(c.red(), c.green(), c.blue(), alpha);
}

void QOPlotWidget::drawPlot()
{
	//qDebug() << "DRAW";

	xLabel->setPlainText(plot.xlabel);
	yLabel->setPlainText(plot.ylabel);
	titleLabel->setPlainText(plot.title);

	bg->setBrush(QBrush(plot.backgroundColor));
	axesBgLeft->setBrush(QBrush(plot.backgroundColor));
	axesBgRight->setBrush(QBrush(plot.backgroundColor));
	axesBgTop->setBrush(QBrush(plot.backgroundColor));
	axesBgBottom->setBrush(QBrush(plot.backgroundColor));
	plotAreaBorder->setPen(QPen(plot.foregroundColor));

	if (cullingAllowed) {
		foreach (QSharedPointer<QOPlotData> dataItem, plot.data) {
			foreach (QGraphicsItem *group, dataItem->cullingGroups) {
				if (group->scene())
					scenePlot->removeItem(group);
				delete group;
			}
			dataItem->cullingGroups.clear();
		}
	}
	scenePlot->removeItem(itemPlot);
	delete itemPlot;
	itemPlot = new QGraphicsDummyItem();
	scenePlot->addItem(itemPlot);
	itemPlot->setZValue(1);

	if (itemLegend->scene())
		scenePlot->removeItem(itemLegend);
	delete itemLegend;
	itemLegend = new QGraphicsDummyItem();
	scenePlot->addItem(itemLegend);
	itemLegend->setZValue(1000);

	const qreal legendLineLength = 30.0;
	const int legendLineSymbolsCount = 3;
	const qreal legendPadding = 10.0;
	const qreal legendSpacing = 5.0;
	const qreal legendTextIndent = 40.0; // = x_text - padding; must be larger than legendLineLength & friends
	qreal legendCurrentItemY = legendPadding;
	qreal legendCurrentWidth = 2 * legendPadding;
	bool haveLegend = false;

	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	foreach (QSharedPointer<QOPlotData> dataItem, plot.data) {
		if (dataItem->getDataType() == "line") {
			QSharedPointer<QOPlotCurveData> item = qSharedPointerCast<QOPlotCurveData>(dataItem);
			if (!item)
				continue;
			QList<QGraphicsItem*> newItems;
			if (item->x.count() > 1000 && resampleAllowed) {
				//
				if (item->worldWidthResampled != deviceW) {
					qreal factor = 1.0 * deviceW / worldW; // 2.0 for subsampling
					int oldXdev = 0;
					qreal oldX, oldY;
					qreal oldYMin, oldYMax;
					oldX = oldY = 0;
					for (int i = 0; i < item->x.count(); i++) {
						qreal newX = item->x[i];
						qreal newY = item->y[i];
						int newXdev = (int)(newX * factor + 0.5);
						if (newXdev == oldXdev && i != 0) {
							oldYMin = qMin(oldYMin, item->y[i]);
							oldYMax = qMax(oldYMax, item->y[i]);
						} else if (newXdev != oldXdev && i != 0) {
							// add vertical line for oldX
							QGraphicsLineItem *line = new QGraphicsLineItem(oldX, oldYMin, oldX, oldYMax, itemPlot, scenePlot);
							if (cullingAllowed)
								newItems << line;
							line->setPen(item->pen);
							line->setZValue(1);
							// connect old line with new line
							line = new QGraphicsLineItem(oldX, oldY, newX, newY, itemPlot, scenePlot);
							if (cullingAllowed)
								newItems << line;
							line->setPen(item->pen);
							line->setZValue(1);
							oldYMin = oldYMax = newY;
						} else if (i == 0) {
							oldYMin = oldYMax = newY;
						}
						oldXdev = newXdev;
						oldX = newX;
						oldY = newY;
					}
					if (item->x.count()) {
						// add vertical line for oldX
						item->xResampled.append(oldX);
						item->yResampled.append(oldYMin);
						item->xResampled.append(oldX);
						item->yResampled.append(oldYMax);
						QGraphicsLineItem *line = new QGraphicsLineItem(oldX, oldYMin, oldX, oldYMax, itemPlot, scenePlot);
						if (cullingAllowed)
							newItems << line;
						line->setPen(item->pen);
						line->setZValue(1);
					}
				}
//				for (int i = 0; i < item->xResampled.count() - 1; i++) {
//					if (item->xResampled[i] == item->xResampled[i+1] && item->yResampled[i] != item->yResampled[i+1])
//						continue;
//					QGraphicsLineItem *line = new QGraphicsLineItem(item->xResampled[i], item->yResampled[i], item->xResampled[i+1], item->yResampled[i+1], itemPlot, scenePlot);
//					line->setPen(item->pen);
//					line->setZValue(1);
//				}
			} else {
				for (int i = 0; i < item->x.count() - 1; i++) {
					if (item->x[i] == item->x[i+1] && item->y[i] != item->y[i+1])
						continue;
					QGraphicsLineItem *line = new QGraphicsLineItem(item->x[i], item->y[i], item->x[i+1], item->y[i+1], itemPlot, scenePlot);
					if (cullingAllowed)
						newItems << line;
					line->setPen(item->pen);
					line->setZValue(1);
				}
			}
			if (cullingAllowed) {
				QGraphicsDummyItem *group = new QGraphicsDummyItem(itemPlot);
				int childCount = 0;
				while (!newItems.isEmpty()) {
					if (childCount <= 3000) {
						QGraphicsItem *newItem = newItems.takeLast();
						newItem->setParentItem(group);
						childCount++;
					} else {
						group->bbox = group->childrenBoundingRect();
						dataItem->cullingGroups << group;
						group = new QGraphicsDummyItem(itemPlot);
						childCount = 0;
					}
				}
				group->bbox = group->childrenBoundingRect();
				dataItem->cullingGroups << group;
			}
			if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
				for (int i = 0; i < item->x.count(); i++) {
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					}
				}
			}
			// line legend
			if (item->legendVisible && !item->legendLabel.isEmpty()) {
				QGraphicsDummyItem *itemLegendLabel = new QGraphicsDummyItem(itemLegend);
				itemLegendLabel->setZValue(1);

				// draw line
				QGraphicsLineItem *line = new QGraphicsLineItem(0, 0, legendLineLength, 0, itemLegendLabel);
				line->setPen(changeAlpha(item->pen, plot.legendAlpha));
				line->setPos(0, 0);
				line->setZValue(1);

				// draw symbols
				if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
					for (int i = 0; i < legendLineSymbolsCount; i++) {
						double x = item->symbolSize / 2.0 + i / (double)(legendLineSymbolsCount - 1) * (legendLineLength - item->symbolSize);
						if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
							drawPlus(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
							drawCross(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
							drawStar(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
							drawCircle(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen, plot.legendAlpha));
						}
					}
				}

				// draw label
				QGraphicsTextItem *label;
				if (!item->legendLabel.isEmpty()) {
					label = new QGraphicsTextItem(item->legendLabel, itemLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(item->pen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());
				}

				// set bbox manually
				itemLegendLabel->bbox = itemLegendLabel->childrenBoundingRect();

				// align vertically (everything is centered locally around y=0)
				itemLegendLabel->setPos(legendPadding, legendCurrentItemY + itemLegendLabel->boundingRect().height() / 2.0);

				legendCurrentItemY += itemLegendLabel->boundingRect().height() + legendSpacing;
				haveLegend = true;
			}
		} else if (dataItem->getDataType() == "stem") {
			QSharedPointer<QOPlotStemData> item = qSharedPointerDynamicCast<QOPlotStemData>(dataItem);
			if (!item)
				continue;
			for (int i = 0; i < item->x.count(); i++) {
				if (item->y[i] == 0)
					continue;
				QGraphicsLineItem *line = new QGraphicsLineItem(item->x[i], 0.0, item->x[i], item->y[i], itemPlot, scenePlot);
				line->setPen(item->pen);
				line->setZValue(1);
			}
			if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
				for (int i = 0; i < item->x.count(); i++) {
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					}
				}
			}
			// stem legend
			if (item->legendVisible && !item->legendLabel.isEmpty()) {
				QGraphicsDummyItem *itemLegendLabel = new QGraphicsDummyItem(itemLegend);
				itemLegendLabel->setZValue(1);

				// draw line
				QGraphicsLineItem *line = new QGraphicsLineItem(0, 0, legendLineLength, 0, itemLegendLabel);
				line->setPen(changeAlpha(item->pen.color(), plot.legendAlpha));
				line->setPos(0, 0);
				line->setZValue(1);

				// draw symbols
				if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
					double x = -item->symbolSize / 2.0 + legendLineLength;
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					}
				}

				// draw label
				QGraphicsTextItem *label;
				if (!item->legendLabel.isEmpty()) {
					label = new QGraphicsTextItem(item->legendLabel, itemLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(item->pen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());
				}

				// set bbox manually
				itemLegendLabel->bbox = itemLegendLabel->childrenBoundingRect();

				// align vertically (everything is centered locally around y=0)
				itemLegendLabel->setPos(legendPadding, legendCurrentItemY + itemLegendLabel->boundingRect().height() / 2.0);

				legendCurrentItemY += itemLegendLabel->boundingRect().height() + legendSpacing;
				haveLegend = true;
			}
		} else if (dataItem->getDataType() == "scatter") {
			QSharedPointer<QOPlotScatterData> item = qSharedPointerDynamicCast<QOPlotScatterData>(dataItem);
			if (!item)
				continue;
			if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
				for (int i = 0; i < item->x.count(); i++) {
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemPlot, item->x[i], item->y[i], 1, item->pen);
					}
				}
			}
			// scatter legend
			if (item->legendVisible && !item->legendLabel.isEmpty()) {
				QGraphicsDummyItem *itemLegendLabel = new QGraphicsDummyItem(itemLegend);
				itemLegendLabel->setZValue(1);

				// draw symbols
				if (!item->pointSymbol.isEmpty() && item->symbolSize > 0) {
					double x = -item->symbolSize / 2.0 + legendLineLength;
					if (item->pointSymbol == "+" || item->pointSymbol == "plus") {
						drawPlus(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "x" || item->pointSymbol == "cross") {
						drawCross(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "*" || item->pointSymbol == "star") {
						drawStar(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					} else if (item->pointSymbol == "o" || item->pointSymbol == "circle") {
						drawCircle(item->symbolSize, itemLegendLabel, x, 0, 1, changeAlpha(item->pen.color(), plot.legendAlpha));
					}
				}

				// draw label
				QGraphicsTextItem *label;
				if (!item->legendLabel.isEmpty()) {
					label = new QGraphicsTextItem(item->legendLabel, itemLegendLabel);
					label->setPos(legendTextIndent, - label->boundingRect().height()/2.0);
					label->setZValue(1);
					label->setDefaultTextColor(changeAlpha(item->pen.color(), plot.legendAlpha));
					legendCurrentWidth = qMax(legendCurrentWidth, 2 * legendPadding + legendTextIndent + label->boundingRect().width());
				}

				// set bbox manually
				itemLegendLabel->bbox = itemLegendLabel->childrenBoundingRect();

				// align vertically (everything is centered locally around y=0)
				itemLegendLabel->setPos(legendPadding, legendCurrentItemY + itemLegendLabel->boundingRect().height() / 2.0);

				legendCurrentItemY += itemLegendLabel->boundingRect().height() + legendSpacing;
				haveLegend = true;
			}
		}
	}

	// legend background
	legendCurrentItemY -= legendSpacing;
	QGraphicsRectItem *legendBg = new QGraphicsRectItem(0, 0, legendCurrentWidth, legendCurrentItemY + legendPadding, itemLegend);
	legendBg->setPen(changeAlpha(plot.foregroundColor, plot.legendAlpha));
	legendBg->setBrush(changeAlpha(plot.legendBackground, plot.legendAlpha));
	legendBg->setZValue(0);
	itemLegend->bbox = itemLegend->childrenBoundingRect();
	if (!haveLegend) {
		scenePlot->removeItem(itemLegend);
	}

	// update axes and other stuff
	updateGeometry();
}

void QOPlotWidget::saveImage(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as image");
		fileDialog.setFilter("PNG (*.png)");
		fileDialog.setDefaultSuffix("png");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		QImage image(width(), height(), QImage::Format_RGB32);
		{
			QPainter painter(&image);
			painter.setRenderHint(QPainter::Antialiasing);
			painter.fillRect(image.rect(), Qt::white);
			scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		}
		image.save(fileName);
	}
}

void QOPlotWidget::saveSvgImage(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as scalable vectorial image");
		fileDialog.setFilter("SVG (*.svg)");
		fileDialog.setDefaultSuffix("svg");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		QSvgGenerator generator;
		generator.setFileName(fileName);
		generator.setSize(QSize(width(), height()));
		generator.setViewBox(QRect(0, 0, width(), height()));
		generator.setTitle(plot.title);
		generator.setDescription("Created by the LINE emulator.");
		QPainter painter;
		painter.begin(&generator);
		painter.setRenderHint(QPainter::Antialiasing);
		scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		painter.end();
	}
}

void QOPlotWidget::savePdf(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as PDF");
		fileDialog.setFilter("PDF (*.pdf)");
		fileDialog.setDefaultSuffix("pdf");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		// Setup printer
		QPrinter printer(QPrinter::HighResolution);
		printer.setOutputFormat(QPrinter::PdfFormat);
		printer.setOutputFileName(fileName);
		printer.setFullPage(true);
		printer.setPageSize(QPrinter::Custom);
		printer.setPaperSize(QSizeF(width(), height()), QPrinter::Point);
		printer.setPageMargins(0, 0, 0, 0, QPrinter::Point);

		QPainter painter;
		painter.begin(&printer);
		painter.setRenderHint(QPainter::Antialiasing);
		scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		painter.end();
	}
}

void QOPlotWidget::saveEps(QString fileName)
{
	if (fileName.isEmpty()) {
		QFileDialog fileDialog(0, "Save plot as EPS");
		fileDialog.setFilter("EPS (*.eps)");
		fileDialog.setDefaultSuffix("eps");
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);

		if (fileDialog.exec()) {
			QStringList fileNames = fileDialog.selectedFiles();
			if (!fileNames.isEmpty())
				fileName = fileNames.first();
		}
	}

	if (!fileName.isEmpty()) {
		// Setup printer
		QPrinter printer(QPrinter::HighResolution);
		printer.setOutputFormat(QPrinter::PostScriptFormat);
		printer.setOutputFileName(fileName);
		printer.setFullPage(true);
		printer.setPageSize(QPrinter::Custom);
		printer.setPaperSize(QSizeF(width(), height()), QPrinter::Point);
		printer.setPageMargins(0, 0, 0, 0, QPrinter::Point);

		QPainter painter;
		painter.begin(&printer);
		painter.setRenderHint(QPainter::Antialiasing);
		scenePlot->render(&painter, QRectF(), QRectF(0, 0, width(), height()));
		painter.end();
	}
}

void QOPlotWidget::cull()
{
	if (!cullingAllowed)
		return;
	qreal plotWidth, plotHeight, world_w, world_h;
	getPlotAreaGeometry(plotWidth, plotHeight, world_w, world_h);

	QRectF viewRect (world_x_off, world_y_off, world_w, world_h);

	// qDebug() << "CULL";
	int visible = 0;
	int invisible = 0;
	foreach (QSharedPointer<QOPlotData> dataItem, plot.data) {
		foreach (QGraphicsItem *group, dataItem->cullingGroups) {
			if (group->boundingRect().intersects(viewRect)) {
				visible++;
				if (!group->scene()) {
					group->setParentItem(itemPlot);
					//qDebug() << "add:" << "group rect:" << group->boundingRect() << "scene rect" << viewRect;
				}
			} else {
				if (group->scene()) {
					scenePlot->removeItem(group);
					//qDebug() << "remove:" << "group rect:" << group->boundingRect() << "scene rect" << viewRect;
				}
				invisible++;
			}
		}
	}
	// qDebug() << "visible:" << visible << "invisible:" << invisible << "total" << visible+invisible;
}

void QOPlotWidget::resizeEvent(QResizeEvent*)
{
	if (width() == 0 || height() == 0)
		return;
	if (plot.autoAdjusted) {
		autoAdjustAxes();
	} else if (plot.autoAdjustedFixedAspect) {
		autoAdjustAxes(plot.autoAspectRatio);
	} else if (plot.fixedAxes) {
		fixAxes(plot.fixedXmin, plot.fixedXmax, plot.fixedYmin, plot.fixedYmax);
	} else {
		updateGeometry();
	}
}

void QOPlotWidget::updateGeometry()
{
	bg->setRect(0, 0, width(), height());
	axesBgLeft->setRect(0, 0, marginLeft, height());
	axesBgRight->setRect(width() - marginRight, 0, marginRight, height());
	axesBgTop->setRect(0, 0, width(), marginTop);
	axesBgBottom->setRect(0, height() - marginBottom, width(), marginBottom);

	viewPlot->setGeometry(0, 0, width(), height());
	viewPlot->setSceneRect(0, 0, width(), height());

	qreal plotWidth, plotHeight, world_w, world_h;
	getPlotAreaGeometry(plotWidth, plotHeight, world_w, world_h);

	plotAreaBorder->setRect(marginLeft, marginTop, plotWidth, plotHeight);

	xLabel->setPos(width() / 2.0 - xLabel->boundingRect().width() / 2.0, marginTop + plotHeight + marginBottom * 2.0 / 3.0 - xLabel->boundingRect().height() / 2.0);
	yLabel->setPos(marginLeft / 3.0 - yLabel->boundingRect().height() / 2.0, height() / 2.0 + yLabel->boundingRect().width() / 2.0);
	yLabel->setRotation(-90);
	titleLabel->setPos(width() / 2.0 - titleLabel->boundingRect().width() / 2.0, marginTop / 2.0 - titleLabel->boundingRect().height() / 2.0);

	itemPlot->resetTransform();
	itemPlot->translate(marginLeft + plotWidth * (-world_x_off/world_w), marginBottom + plotHeight * (1 + world_y_off/world_h));
	itemPlot->scale(plotWidth/world_w, -plotHeight/world_h);

	// ticks
	foreach (QGraphicsItem* item, tickItems) {
		delete item;
	}
	tickItems.clear();

	qreal x_tick_start, x_tick_size;
	const int x_tick_count = 10;
	const qreal x_tick_length = 5; // line length
	nice_loose_label(world_x_off, world_x_off + world_w, x_tick_count, x_tick_start, x_tick_size);
	// X tick marks & grid
	for (int i = 0; i <= x_tick_count; i++) {
		qreal x_tick_world = x_tick_start + i * x_tick_size;
		if (world_x_off <= x_tick_world && x_tick_world <= world_x_off + world_w) {
			qreal x_tick_device = marginLeft + (x_tick_world - world_x_off)/world_w*plotWidth;
			QGraphicsTextItem *tick = scenePlot->addText(QString::number(x_tick_world));
			tick->setPos(x_tick_device - tick->boundingRect().width() / 2.0, marginTop + plotHeight);
			tick->setZValue(200);
			tickItems << tick;
			QGraphicsLineItem *line = scenePlot->addLine(x_tick_device, marginTop + plotHeight, x_tick_device, marginTop + plotHeight - x_tick_length, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			line = scenePlot->addLine(x_tick_device, marginTop, x_tick_device, marginTop + x_tick_length, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			if (plot.xGridVisible) {
				line = scenePlot->addLine(x_tick_device, marginTop + plotHeight, x_tick_device, marginTop + plotHeight - plotHeight, QPen(plot.foregroundColor, 1.0, Qt::DotLine));
				line->setZValue(0.4);
				tickItems << line;
			}
		}
	}
	// Y axis
	if (world_x_off <= 0 && 0 <= world_x_off + world_w) {
		qreal x_tick_device = marginLeft + (0.0 - world_x_off)/world_w*plotWidth;
		QGraphicsLineItem *line = scenePlot->addLine(x_tick_device, marginTop + plotHeight, x_tick_device, marginTop + plotHeight - plotHeight, QPen(plot.foregroundColor, 1.0, Qt::SolidLine));
		line->setZValue(0.4);
		tickItems << line;
	}

	qreal y_tick_start, y_tick_size;
	const int y_tick_count = 10;
	const qreal y_tick_length = 5; // line length
	nice_loose_label(world_y_off, world_y_off + world_h, y_tick_count, y_tick_start, y_tick_size);
	double y_tick_max_width = -1;
	// Y tick marks & grid
	for (int i = 0; i <= y_tick_count; i++) {
		qreal y_tick_world = y_tick_start + i * y_tick_size;
		if (world_y_off <= y_tick_world && y_tick_world <= world_y_off + world_h) {
			qreal y_tick_device = marginTop + (1 - (y_tick_world - world_y_off)/world_h)*plotHeight;
			QGraphicsTextItem *tick = scenePlot->addText(QString::number(y_tick_world));
			tick->setPos(marginLeft - tick->boundingRect().width(), y_tick_device - tick->boundingRect().height() / 2.0);
			tick->setZValue(200);
			tickItems << tick;
			y_tick_max_width = qMax(y_tick_max_width, tick->boundingRect().width());
			QGraphicsLineItem *line = scenePlot->addLine(marginLeft, y_tick_device, marginLeft + y_tick_length, y_tick_device, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			line = scenePlot->addLine(marginLeft + plotWidth, y_tick_device, marginLeft + plotWidth - y_tick_length, y_tick_device, QPen(plot.foregroundColor, 1.0));
			line->setZValue(0.5);
			tickItems << line;
			if (plot.yGridVisible) {
				line = scenePlot->addLine(marginLeft, y_tick_device, marginLeft + plotWidth, y_tick_device, QPen(plot.foregroundColor, 1.0, Qt::DotLine));
				line->setZValue(0.4);
				tickItems << line;
			}
		}
	}
	// X axis
	if (world_y_off <= 0 && 0 <= world_y_off + world_h) {
		qreal y_tick_device = marginTop + (1 - (0.0 - world_y_off)/world_h)*plotHeight;
		QGraphicsLineItem *line = scenePlot->addLine(marginLeft, y_tick_device, marginLeft + plotWidth, y_tick_device, QPen(plot.foregroundColor, 1.0, Qt::SolidLine));
		line->setZValue(0.4);
		tickItems << line;
	}

	// adjust marginLeft if it is too small
	if (y_tick_max_width > marginLeft * 2.0 / 3.0 - yLabel->boundingRect().height() / 2.0) {
		marginLeft = (y_tick_max_width + yLabel->boundingRect().height() / 2.0) * 3.0 / 2.0 * 1.1;
		updateGeometry();
	}

	// legend
	if (plot.legendPosition == QOPlot::TopLeft) {
		itemLegend->setPos(marginLeft, marginTop);
	} else if (plot.legendPosition == QOPlot::TopRight) {
		itemLegend->setPos(marginLeft + plotWidth - itemLegend->childrenBoundingRect().width(), marginTop);
	} else if (plot.legendPosition == QOPlot::BottomLeft) {
		itemLegend->setPos(marginLeft, marginTop + plotHeight - itemLegend->childrenBoundingRect().height());
	} else if (plot.legendPosition == QOPlot::BottomRight) {
		itemLegend->setPos(marginLeft + plotWidth - itemLegend->childrenBoundingRect().width(), marginTop + plotHeight - itemLegend->childrenBoundingRect().height());
	}

	cull();
}


void QOPlotWidget::getPlotAreaGeometry(qreal &deviceW, qreal &deviceH, qreal &worldW, qreal &worldH)
{
	deviceW = qMax(width() - marginLeft - marginRight, 1.0);
	deviceH = qMax(height() - marginTop - marginBottom, 1.0);

	worldW = deviceW / zoomX;
	worldH = deviceH / zoomY;
}

void QOPlotWidget::mouseMoveEvent(QMouseEvent* event)
{
	// world coords
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		//		qDebug() << "move:" << "screen" << event->x() << event->y() << "world" << world_x << world_y;

		bool mustUpdate = false;
		if (plot.drag_x_enabled && event->buttons() & Qt::LeftButton && !isnan(world_press_x)) {
			qreal delta_x = world_x - world_press_x;
			world_x_off -= delta_x;
			mustUpdate = true;
		}

		if (plot.drag_y_enabled && event->buttons() & Qt::LeftButton && !isnan(world_press_y)) {
			qreal delta_y = world_y - world_press_y;
			world_y_off -= delta_y;
			mustUpdate = true;
		}

		if (mustUpdate) {
			plot.autoAdjusted = false;
			plot.autoAdjustedFixedAspect = false;
			plot.fixedAxes = false;
			updateGeometry();
		}
	}
}

void QOPlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
	// world coords
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		//		qDebug() << "release:" << "screen" << event->x() << event->y() << "world" << world_x << world_y;
	}
}

void QOPlotWidget::mousePressEvent(QMouseEvent* event)
{
	// world coords
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		//		qDebug() << "press:" << "screen" << event->x() << event->y() << "world" << world_x << world_y;

		if (event->buttons() & Qt::LeftButton) {
			world_press_x = world_x;
			world_press_y = world_y;
		}
	}

	if (event->buttons() & Qt::MidButton) {
		autoAdjustAxes();
	}
}

void QOPlotWidget::wheelEvent(QWheelEvent *event)
{
	qreal world_x, world_y;
	if (widgetCoordsToWorldCoords(event->x(), event->y(), world_x, world_y)) {
		float steps = event->delta()/8.0/15.0;

		//		qDebug() << "wheel:" << "screen" << event->x() << event->y() << "delta" << event->delta() << "world" << world_x << world_y;

		if (plot.zoom_x_enabled) {
			qreal zoomX_old = zoomX;
			zoomX *= pow(1.25, steps);
			world_x_off = (world_x_off - world_x * (1.0 - zoomX / zoomX_old)) * zoomX_old / zoomX;
		}

		if (plot.zoom_y_enabled) {
			qreal zoomY_old = zoomY;
			zoomY *= pow(1.25, steps);
			world_y_off = (world_y_off - world_y * (1.0 - zoomY / zoomY_old)) * zoomY_old / zoomY;
		}

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		drawPlot();
		event->accept();
	}
}

void QOPlotWidget::keyPressEvent(QKeyEvent *event)
{
	event->ignore();
	if (event->key() == Qt::Key_Left && plot.drag_x_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dx = -1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dx = -50;
		} else {
			widget_dx = -10;
		}
		widget_dy = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_x_off += world_dx;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		event->accept();
	} else if (event->key() == Qt::Key_Right && plot.drag_x_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dx = 1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dx = 50;
		} else {
			widget_dx = 10;
		}
		widget_dy = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_x_off += world_dx;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		event->accept();
	} else if (event->key() == Qt::Key_Up && plot.drag_y_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dy = -1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dy = -50;
		} else {
			widget_dy = -10;
		}
		widget_dx = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_y_off += world_dy;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		event->accept();
	} else if (event->key() == Qt::Key_Down && plot.drag_y_enabled) {
		qreal widget_dx, widget_dy, world_dx, world_dy;
		if (event->modifiers() & Qt::ControlModifier) {
			widget_dy = 1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			widget_dy = 50;
		} else {
			widget_dy = 10;
		}
		widget_dx = 0;
		widgetDeltaToWorldDelta(widget_dx, widget_dy, world_dx, world_dy);

		world_y_off += world_dy;

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		event->accept();
	} else if (event->key() == Qt::Key_Minus) {
		float steps;
		if (event->modifiers() & Qt::ControlModifier) {
			steps = -0.1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			steps = -5;
		} else {
			steps = -1;
		}

		qreal deviceW, deviceH, worldW, worldH;
		getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);
		qreal world_x = world_x_off + worldW * 0.5;
		qreal world_y = world_y_off + worldH * 0.5;

		if (plot.zoom_x_enabled) {
			qreal zoomX_old = zoomX;
			zoomX *= pow(1.25, steps);
			world_x_off = (world_x_off - world_x * (1.0 - zoomX / zoomX_old)) * zoomX_old / zoomX;
		}

		if (plot.zoom_y_enabled) {
			qreal zoomY_old = zoomY;
			zoomY *= pow(1.25, steps);
			world_y_off = (world_y_off - world_y * (1.0 - zoomY / zoomY_old)) * zoomY_old / zoomY;
		}

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		event->accept();
	} else if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
		float steps;
		if (event->modifiers() & Qt::ControlModifier) {
			steps = 0.1;
		} else if (event->modifiers() & Qt::ShiftModifier) {
			steps = 5;
		} else {
			steps = 1;
		}

		qreal deviceW, deviceH, worldW, worldH;
		getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);
		qreal world_x = world_x_off + worldW * 0.5;
		qreal world_y = world_y_off + worldH * 0.5;

		if (plot.zoom_x_enabled) {
			qreal zoomX_old = zoomX;
			zoomX *= pow(1.25, steps);
			world_x_off = (world_x_off - world_x * (1.0 - zoomX / zoomX_old)) * zoomX_old / zoomX;
		}

		if (plot.zoom_y_enabled) {
			qreal zoomY_old = zoomY;
			zoomY *= pow(1.25, steps);
			world_y_off = (world_y_off - world_y * (1.0 - zoomY / zoomY_old)) * zoomY_old / zoomY;
		}

		plot.autoAdjusted = false;
		plot.autoAdjustedFixedAspect = false;
		plot.fixedAxes = false;
		updateGeometry();
		event->accept();
	} else if (event->key() == Qt::Key_A) {
		autoAdjustAxes();
	} else if (event->key() == Qt::Key_S) {
		saveImage();
	} else if (event->key() == Qt::Key_V) {
		saveSvgImage();
	} else if (event->key() == Qt::Key_P) {
		savePdf();
	} else if (event->key() == Qt::Key_E) {
		saveEps();
	} else if (event->key() == Qt::Key_Q) {
		if (viewPlot->renderHints() & QPainter::Antialiasing) {
			viewPlot->setRenderHint(QPainter::Antialiasing, false);
			viewPlot->setRenderHint(QPainter::HighQualityAntialiasing, false);
		} else {
			viewPlot->setRenderHint(QPainter::Antialiasing, true);
			viewPlot->setRenderHint(QPainter::HighQualityAntialiasing, true);
		}
	}
}

void QOPlotWidget::focusInEvent(QFocusEvent *event)
{
	event->accept();
}

void QOPlotWidget::focusOutEvent(QFocusEvent *event)
{
	event->accept();
}

void QOPlotWidget::enterEvent(QEvent *event)
{
	event->accept();
	setFocus(Qt::MouseFocusReason);
}

void QOPlotWidget::autoAdjustAxes()
{
	// compute bounding box
	qreal xmin, xmax, ymin, ymax;
	dataBoundingBox(xmin, xmax, ymin, ymax);

	if (plot.includeOriginX) {
		if (xmin > 0)
			xmin = 0;
		if (xmax < 0)
			xmax = 0;
	}
	if (plot.includeOriginY) {
		if (ymin > 0)
			ymin = 0;
		if (ymax < 0)
			ymax = 0;
	}
	xmax += (xmax - xmin) * 0.1;
	ymax += (ymax - ymin) * 0.1;

	// compute zoom/scroll
	recalcViewport(xmin, xmax, ymin, ymax);

	plot.autoAdjusted = true;
	plot.autoAdjustedFixedAspect = false;
	plot.fixedAxes = false;

	updateGeometry();
}

void QOPlotWidget::autoAdjustAxes(qreal aspectRatio)
{
	plot.autoAspectRatio = aspectRatio;

	// compute bounding box
	qreal xmin, xmax, ymin, ymax;
	dataBoundingBox(xmin, xmax, ymin, ymax);

	if (plot.includeOriginX) {
		if (xmin > 0)
			xmin = 0;
		if (xmax < 0)
			xmax = 0;
	}
	if (plot.includeOriginY) {
		if (ymin > 0)
			ymin = 0;
		if (ymax < 0)
			ymax = 0;
	}
	xmax += (xmax - xmin) * 0.1;
	ymax += (ymax - ymin) * 0.1;

	// compute zoom/scroll
	recalcViewport(xmin, xmax, ymin, ymax, aspectRatio);

	plot.autoAdjusted = false;
	plot.autoAdjustedFixedAspect = true;
	plot.fixedAxes = false;

	updateGeometry();
}

void QOPlotWidget::fixAxes(qreal xmin, qreal xmax, qreal ymin, qreal ymax)
{
	plot.fixedXmin = xmin;
	plot.fixedXmax = xmax;
	plot.fixedYmin = ymin;
	plot.fixedYmax = ymax;

	recalcViewport(xmin, xmax, ymin, ymax);

	plot.autoAdjusted = false;
	plot.autoAdjustedFixedAspect = false;
	plot.fixedAxes = true;

	updateGeometry();
}

void QOPlotWidget::recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax)
{
	qreal deviceW = qMax(width() - marginLeft - marginRight, 1.0);
	qreal deviceH = qMax(height() - marginTop - marginBottom, 1.0);

	world_x_off = xmin;
	world_y_off = ymin;
	zoomX = (xmax - xmin == 0) ? 1.0 : deviceW / (xmax - xmin);
	zoomY = (ymax - ymin == 0) ? 1.0 : deviceH / (ymax - ymin);
}

void QOPlotWidget::recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax, qreal aspectRatio)
{
	// Solve:
	// zoomX / zoomY = 1/aspectRatio
	// zoomX <= zoomXMin
	// zoomY <= zoomYMin
	// 1. Compute zoomXMin, zoomYMin
	recalcViewport(xmin, xmax, ymin, ymax);
	// 2. Enforce ratio
	qreal wph = 1;
	if (width() != 0 && height() != 0)
		wph = width() / (qreal) height();
	if (aspectRatio > 1) {
		zoomX = zoomY / aspectRatio;
	} else {
		zoomY = zoomX * aspectRatio;
	}
}


void QOPlotWidget::dataBoundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax)
{
	if (!plot.data.isEmpty()) {
		plot.data[0]->boundingBox(xmin, xmax, ymin, ymax);
	} else {
		xmin = 0; xmax = 1;
		ymin = 0; ymax = 1;
	}
	foreach (QSharedPointer<QOPlotData> item, plot.data) {
		qreal dxmin, dxmax, dymin, dymax;
		item->boundingBox(dxmin, dxmax, dymin, dymax);
		if (dxmin < xmin)
			xmin = dxmin;
		if (dxmax > xmax)
			xmax = dxmax;
		if (dymin < ymin)
			ymin = dymin;
		if (dymax > ymax)
			ymax = dymax;
	}
}

bool QOPlotWidget::widgetCoordsToWorldCoords(qreal widget_x, qreal widget_y, qreal &world_x, qreal &world_y)
{
	widget_x -= marginLeft;
	widget_y -= marginTop;

	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	if (widget_x < 0 || widget_x >= deviceW)
		return false;
	if (widget_y < 0 || widget_y >= deviceH)
		return false;

	widget_y = deviceH - widget_y;
	world_x = world_x_off + widget_x / deviceW * worldW;
	world_y = world_y_off + widget_y / deviceH * worldH;

	// success
	return true;
}

void QOPlotWidget::widgetDeltaToWorldDelta(qreal widget_dx, qreal widget_dy, qreal &world_dx, qreal &world_dy)
{
	qreal deviceW, deviceH, worldW, worldH;
	getPlotAreaGeometry(deviceW, deviceH, worldW, worldH);

	widget_dy = -widget_dy;
	world_dx = widget_dx / deviceW * worldW;
	world_dy = widget_dy / deviceH * worldH;
}

