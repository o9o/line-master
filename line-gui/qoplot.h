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

#ifndef QOPLOT_H
#define QOPLOT_H

#include <QWidget>
#include <QtGui>

class QOPlotData;

// Holds the plot configuration (no GUI elements)
class QOPlot
{
public:
	enum LegendPosition {
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	explicit QOPlot();
	~QOPlot();

	// the plot title
	QString title;
	// axis labels
	QString xlabel;
	QString ylabel;

	// the data to plot
	QList<QSharedPointer<QOPlotData> > data;

	// Adds d to the plot and takes ownership (so do not delete d somewhere else)
	void addData(QOPlotData *d) {
		data << QSharedPointer<QOPlotData>(d);
	}

	// bg color: used for the background...
	QColor backgroundColor;
	// fg color: used for axes, text
	QColor foregroundColor;

	// legend config
	bool legendVisible; // if true, show the legend
	LegendPosition legendPosition;
	QColor legendBackground; // RGB, A does not matter
	int legendAlpha; // opacity

	// show axis grid
	bool xGridVisible;
	bool yGridVisible;

	// There are 3 different ways of setting a predefined viewport.
	// In any case, the viewport is changed on user scroll/zoom.
	// 1. Fixed viewport: specify axes limits (xmin xmax ymin ymax)
	bool fixedAxes;
	qreal fixedXmin, fixedXmax, fixedYmin, fixedYmax;
	// 2. Auto adjusted: the plot is automatically resized to show all data
	// If includeOriginX is set, the viewport is changed to include X=0
	// If includeOriginY is set, the viewport is changed to include Y=0
	bool autoAdjusted;
	bool includeOriginX;
	bool includeOriginY;
	// 3. Auto adjusted with custom aspect ratio: the plot is automatically resized to show all data,
	// but a specific aspect ratio is preserved
	// If includeOriginX is set, the viewport is changed to include X=0
	// If includeOriginY is set, the viewport is changed to include Y=0
	bool autoAdjustedFixedAspect;
	qreal autoAspectRatio;

	// set this to enable/disable mouse actions
	bool drag_x_enabled;
	bool drag_y_enabled;
	bool zoom_x_enabled;
	bool zoom_y_enabled;
};

// Holds a data set to be plotted (e.g. a vector of points). Subclasses should specialize this.
class QOPlotData
{
public:
	explicit QOPlotData();
	virtual ~QOPlotData() {}
	virtual QString getDataType() { return dataType; }
	bool legendVisible;  // show a legend item for this line
	QString legendLabel; // text label for legend
	QColor legendLabelColor; // text color

	QList<QGraphicsItem*> cullingGroups;

	virtual void boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax) { xmin = 0; xmax = 1; ymin = 0; ymax = 1;}
	virtual QRectF boundingBox() {
		qreal xmin, xmax, ymin, ymax;
		boundingBox(xmin, xmax, ymin, ymax);
		return QRectF(xmin, xmax-xmin, ymin, ymax-ymin).normalized();
	}

protected:
	QString dataType; // can be: "line", "stem"
};

// Holds a data set to be plotted as a zig-zag line.
class QOPlotCurveData : public QOPlotData
{
public:
	explicit QOPlotCurveData();
	QVector<qreal> x; // x coords of the data points
	QVector<qreal> y; // y coords of the data points, x and y MUST have the same length
	QPen pen; // color, line style, width etc. Set cosmetic to true!
	QString pointSymbol; // use "+" etc, or "" for no symbol
	qreal symbolSize; // symbol size in pixels

	void boundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax);

	qreal worldWidthResampled;
	QVector<qreal> xResampled; // x coords of the data points
	QVector<qreal> yResampled; // y coords of the data points, x and y MUST have the same length
};

// Holds a data set to be plotted as a scatter plot.
class QOPlotScatterData : public QOPlotCurveData
{
public:
	explicit QOPlotScatterData();
};

class QOPlotStemData : public QOPlotCurveData
{
public:
	explicit QOPlotStemData();
};

class QGraphicsDummyItem;

class QOGraphicsView : public QGraphicsView
{
	Q_OBJECT
public:
	QOGraphicsView(QWidget *parent = 0) : QGraphicsView(parent) {}
	QOGraphicsView(QGraphicsScene *scene, QWidget *parent = 0) : QGraphicsView(scene, parent) {}

signals:
	void mousePressed(QMouseEvent *event);
	void mouseMoved(QMouseEvent *event);
	void mouseReleased(QMouseEvent *event);
	void wheelMoved(QWheelEvent *event);

protected:
	void mousePressEvent(QMouseEvent *event) { emit mousePressed(event); }
	void mouseMoveEvent(QMouseEvent *event) { emit mouseMoved(event); }
	void mouseReleaseEvent(QMouseEvent *event) { emit mouseReleased(event); }
	void wheelEvent(QWheelEvent *event) { emit wheelMoved(event); }
};

class QOPlotWidget : public QWidget
{
	Q_OBJECT
public:
	explicit QOPlotWidget(QWidget *parent = 0,
				 int minWidth = 0, int minHeight = 0,
				 QSizePolicy policy = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));

	QOPlot plot;

	// the size of the area between the widget border and the plot
	qreal marginLeft;
	qreal marginRight;
	qreal marginTop;
	qreal marginBottom;

	// used internally for keeping a zoom factor
	qreal zoomX, zoomY;

	// sets the data and redraws
	void setPlot(QOPlot plot);

	// sets the viewport such that the entire range of the data is visible
	// this is called automatically on middle click
	void autoAdjustAxes();

	// same as the other one, but forces an aspect ratio
	void autoAdjustAxes(qreal aspectRatio);

	// sets a fixed viewport
	void fixAxes(qreal xmin, qreal xmax, qreal ymin, qreal ymax);

	// this allows us to place the widget into Qt layouts
	void setMinSize(int w, int h) {
		minWidth = w;
		minHeight = h;
	}

	// Drawing optimizations
	bool resampleAllowed;
	bool cullingAllowed;

	void setAntiAliasing(bool enable);
signals:

public slots:
	// repopulates the scene (i.e. redraws everything); call this after changing data, labels, colors etc.
	void drawPlot();

	void saveImage(QString fileName = QString());
	void saveSvgImage(QString fileName = QString());
	void savePdf(QString fileName = QString());
	void saveEps(QString fileName = QString());

	void test();
	void testHuge();

protected:
	QGraphicsScene *scenePlot;
	QOGraphicsView *viewPlot;
	QGraphicsDummyItem *itemPlot;
	QGraphicsDummyItem *itemLegend;
	QGraphicsRectItem *bg;
	QGraphicsRectItem *axesBgLeft;
	QGraphicsRectItem *axesBgRight;
	QGraphicsRectItem *axesBgTop;
	QGraphicsRectItem *axesBgBottom;
	QGraphicsRectItem *plotAreaBorder;
	QGraphicsTextItem *xLabel;
	QGraphicsTextItem *yLabel;
	QGraphicsTextItem *titleLabel;
	QList<QGraphicsItem*> tickItems;

	// used internally for keeping the axes origin offset
	// call updateGeometry() after changing this
	qreal world_x_off;
	qreal world_y_off;

	// the last mouse press coords
	qreal world_press_x;
	qreal world_press_y;

	// compute the bbox of the data
	void dataBoundingBox(qreal &xmin, qreal &xmax, qreal &ymin, qreal &ymax);

	// compute zoom and offset to obtain a viewport
	void recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax);
	void recalcViewport(qreal xmin, qreal xmax, qreal ymin, qreal ymax, qreal aspectRatio);

	// returns the plot area in device and world (plot) coords
	void getPlotAreaGeometry(qreal &deviceW, qreal &deviceH, qreal &worldW, qreal &worldH);

	// resizes and repositions everything
	// call after changing the widget size, or plot viewport
	void updateGeometry();

	void resizeEvent(QResizeEvent*);

	// Tries to convert widget coords to world coords, and returns true in case of success.
	// Failure occurs when the position is outside the plot area.
	bool widgetCoordsToWorldCoords(qreal widget_x, qreal widget_y, qreal &world_x, qreal &world_y);
	void widgetDeltaToWorldDelta(qreal widget_dx, qreal widget_dy, qreal &world_dx, qreal &world_dy);

	// this allows us to place the widget into Qt layouts
	int minWidth;
	int minHeight;
	QSize minimumSizeHint() const {return QSize(minWidth, minHeight);}
	QSize sizeHint() const {return QSize(minWidth, minHeight);}

	void cull();

protected slots:
	// process mouse events for drag and zoom
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void focusInEvent(QFocusEvent *event);
	void focusOutEvent(QFocusEvent *event);
	void enterEvent(QEvent *event);
};

#endif // QOPLOT_H

