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

#ifndef NETGRAPHSCENE_H
#define NETGRAPHSCENE_H

#include <QGraphicsScene>

#include "netgraph.h"
#include "netgraphscenenode.h"
#include "netgraphsceneedge.h"
#include "netgraphsceneas.h"
#include "netgraphsceneconnection.h"

#define SHOW_MAX_NODES 100

class NetGraphScene : public QGraphicsScene
{
	Q_OBJECT
public:
	enum EditMode { InsertHost,
				 InsertGateway,
				 InsertRouter,
				 InsertBorderRouter,
				 InsertEdgeStart,
				 InsertEdgeEnd,
				 InsertConnectionStart,
				 InsertConnectionEnd,
				 MoveNode,
				 EditEdge,
				 DelNode,
				 DelEdge,
				 DelConnection,
				 Drag,
				 ShowRoutes };

	explicit NetGraphScene(QObject *parent = 0);
	NetGraph *netGraph;
	EditMode editMode;

	// for moving nodes
	QPointF oldNodePos;
	QPointF oldMousePos;

	// for adding edges
	NetGraphSceneEdge *newEdge;
	NetGraphEdge defaultEdge;

	int defaultASNumber;
	QString connectionType;

	// the start node for a new edge/connection
	NetGraphSceneNode *startNode;

	NetGraphSceneConnection *newConnection;

	// for editing edges
	NetGraphSceneEdge *selectedEdge;

	// for large graphs, switch to read only
	bool fastMode;
	bool unusedHidden;
	bool hideConnections;
	bool hideFlows;
	bool hideEdges;

	QPixmap pixmapHost;
	QPixmap pixmapRouter;
	QPixmap pixmapGateway;
	QPixmap pixmapBorderRouter;

	QGraphicsTextItem *tooltip;

signals:
	void edgeSelected(NetGraphEdge edge);

public slots:
	void setMode(EditMode mode);

	// edge properties (selected/default)
	void delayChanged(int val);
	void bandwidthChanged(double val);
	void lossRateChanged(double val);
	void queueLenghtChanged(int val);
	void samplingPeriodChanged(quint64 val);
	void samplingChanged(bool val);

	// node properties
	void ASNumberChanged(int val);

	// connection properties
	void connectionTypeChanged(QString val);

	// graph events
	void reload();
	void usedChanged();
	void setUnusedHidden(bool unusedHidden);
	void setHideConnections(bool value);
	void setHideFlows(bool value);
	void setHideEdges(bool value);
	void routingChanged();
	void showFlows();

	void initTooltip();

	// new edge
	NetGraphSceneEdge *getNewEdge();
	void destroyNewEdge();

	// new connection
	NetGraphSceneConnection *getNewConnection();
	void destroyNewConnection();

	// node mouse events
	void nodeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void nodeMouseMoved(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void nodeMouseReleased(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void nodeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node);
	void nodeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node);

	// edge mouse events
	void edgeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneEdge *edge);
	void edgeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge);
	void edgeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge);

	// connection mouse events
	void connectionMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneConnection *c);
	void connectionHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c);
	void connectionHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c);

protected:
	void *tooltipTarget;

	void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);

	NetGraphSceneNode *addNode(int type, QPointF pos);
	NetGraphSceneNode *addNode(int index);
	NetGraphSceneEdge *addEdge(int startIndex, int endIndex, double bandwidth, int delay, double loss, int queueLength,
						  NetGraphSceneNode *start, NetGraphSceneNode *end);
	NetGraphSceneEdge *addEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end);
	NetGraphSceneEdge *addFlowEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end, int extraOffset, QColor color);
	NetGraphSceneAS *addAS(int index);
	NetGraphSceneConnection *addConnection(int startIndex, int endIndex,
							NetGraphSceneNode *start, NetGraphSceneNode *end);
	NetGraphSceneConnection *addConnection(int index, NetGraphSceneNode *start, NetGraphSceneNode *end);
};

#endif // NETGRAPHSCENE_H
