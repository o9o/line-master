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

#include "netgraphscene.h"

#include "qgraphicstooltip.h"

NetGraphScene::NetGraphScene(QObject *parent) :
	QGraphicsScene(parent)
{
	netGraph = 0;
	newEdge = 0;
	newConnection = 0;
	editMode = MoveNode;
	fastMode = false;
	unusedHidden = false;
	defaultASNumber = 0;
	connectionType = "";
	hideConnections = false;
	hideFlows = false;
	hideEdges = false;
	pixmapHost.load(":/icons/extra-resources/rgtaylor_csc_net_computer_32.png");
	pixmapRouter.load(":/icons/extra-resources/juanjo_Router_32.png");
	pixmapBorderRouter.load(":/icons/extra-resources/juanjo_Router_border_32.png");
	pixmapGateway.load(":/icons/extra-resources/juanjo_Router_gateway_32.png");
	tooltipTarget = 0;
	initTooltip();
	reload();
}

void NetGraphScene::setMode(EditMode mode)
{
	if (editMode == EditEdge && mode != EditEdge && selectedEdge) {
		selectedEdge->setSelected(false);
		selectedEdge = 0;
		emit edgeSelected(defaultEdge);
	}
	if (editMode == InsertEdgeEnd && mode != InsertEdgeEnd && startNode) {
		startNode->setSel(false);
		startNode->ungrabMouse();
		newEdge->setVisible(false);
		startNode = 0;
	}
	if (editMode == InsertConnectionEnd && mode != InsertConnectionEnd && startNode) {
		startNode->setSel(false);
		startNode->ungrabMouse();
		newConnection->setVisible(false);
		startNode = 0;
	}

	if (mode == Drag && editMode != Drag ) {
		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
			if (edge && !edge->flowEdge) {
				edge->setAcceptedMouseButtons(Qt::NoButton);
			}
			NetGraphSceneNode *node = dynamic_cast<NetGraphSceneNode*>(item);
			if (node) {
				node->setAcceptedMouseButtons(Qt::NoButton);
			}
		}
	} else if (editMode == Drag && mode != Drag) {
		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
			if (edge && !edge->flowEdge) {
				edge->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
			}
			NetGraphSceneNode *node = dynamic_cast<NetGraphSceneNode*>(item);
			if (node) {
				node->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
			}
		}
	}

	if (editMode == ShowRoutes && mode != ShowRoutes) {
		if (tooltipTarget) {
			tooltip->setVisible(false);
			tooltipTarget = 0;
		}
	}

	editMode = mode;
}

void NetGraphScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	if (editMode == Drag || mouseEvent->button() != Qt::LeftButton) {
		mouseEvent->ignore();
		return;
	}

	if (editMode == InsertHost) {
		addNode(NETGRAPH_NODE_HOST, mouseEvent->scenePos());
	} else if (editMode == InsertGateway) {
		addNode(NETGRAPH_NODE_GATEWAY, mouseEvent->scenePos());
	} else if (editMode == InsertRouter) {
		addNode(NETGRAPH_NODE_ROUTER, mouseEvent->scenePos());
	} else if (editMode == InsertBorderRouter) {
		addNode(NETGRAPH_NODE_BORDER, mouseEvent->scenePos());
	}
	QGraphicsScene::mousePressEvent(mouseEvent);
}

NetGraphSceneNode *NetGraphScene::addNode(int type, QPointF pos)
{
	int nodeIndex = netGraph->addNode(type, pos, defaultASNumber);
	// update the AS hull
	netGraph->computeASHulls(defaultASNumber);

	// update the AS graphics item
	bool foundAS = false;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneAS *domain = dynamic_cast<NetGraphSceneAS*>(item);
		if (domain) {
			if (domain->ASNumber == defaultASNumber) {
				foundAS = true;
				foreach (NetGraphAS d, netGraph->domains) {
					if (d.ASNumber == defaultASNumber) {
						domain->setHull(d.hull);
						break;
					}
				}
				break;
			}
		}
	}
	if (!foundAS) {
		foreach (NetGraphAS d, netGraph->domains) {
			if (d.ASNumber == defaultASNumber) {
				addAS(d.index);
				break;
			}
		}
	}

	return addNode(nodeIndex);
}

NetGraphSceneNode *NetGraphScene::addNode(int index)
{
	QPixmap pixmap;
	int nodeType = netGraph->nodes[index].nodeType;
	if (nodeType == NETGRAPH_NODE_HOST) {
		pixmap = pixmapHost;
	} else if (nodeType == NETGRAPH_NODE_GATEWAY) {
		pixmap = pixmapGateway;
	} else if (nodeType == NETGRAPH_NODE_ROUTER) {
		pixmap = pixmapRouter;
	} else if (nodeType == NETGRAPH_NODE_BORDER) {
		pixmap = pixmapBorderRouter;
	}

	NetGraphSceneNode *item = new NetGraphSceneNode(index, nodeType, pixmap, 0, this);
	item->setFastMode(fastMode);
	item->setPos(QPointF(netGraph->nodes[index].x, netGraph->nodes[index].y));
	item->setUsed(netGraph->nodes[index].used);
	item->setUnusedHidden(unusedHidden);
	if (!fastMode) {
		connect(item, SIGNAL(mousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)), SLOT(nodeMousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)));
		connect(item, SIGNAL(mouseMoved(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)), SLOT(nodeMouseMoved(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)));
		connect(item, SIGNAL(mouseReleased(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)), SLOT(nodeMouseReleased(QGraphicsSceneMouseEvent*,NetGraphSceneNode*)));
		connect(item, SIGNAL(hoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)), SLOT(nodeHoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)));
		connect(item, SIGNAL(hoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)), SLOT(nodeHoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneNode*)));
	}
	return item;
}

NetGraphSceneAS *NetGraphScene::addAS(int index)
{
	NetGraphSceneAS *domain = new NetGraphSceneAS(index, netGraph->domains[index].ASNumber, netGraph->domains[index].hull, 0, this);
	domain->setUsed(netGraph->domains[index].used);
	domain->setUnusedHidden(unusedHidden);
	domain->setZValue(-2);
	return domain;
}

NetGraphSceneEdge *NetGraphScene::addEdge(int startIndex, int endIndex, double bandwidth, int delay, double loss, int queueLength,
								  NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	int edgeIndex = netGraph->addEdge(startIndex, endIndex, bandwidth, delay, loss, queueLength);
	return addEdge(edgeIndex, start, end);
}

NetGraphSceneEdge *NetGraphScene::addEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	NetGraphSceneEdge *edge = new NetGraphSceneEdge(netGraph->edges[index].source, netGraph->edges[index].dest, index, 0, this);
	edge->setFastMode(fastMode);
	edge->setStartPoint(start->pos());
	edge->setEndPoint(end->pos());
	edge->setZValue(-1);
	edge->setText(netGraph->edges[edge->edgeIndex].tooltip());
	edge->setUsed(netGraph->edges[index].used);
	edge->setUnusedHidden(unusedHidden);
	edge->setVisible(!hideEdges);
	if (!fastMode) {
		connect(start, SIGNAL(positionChanged(QPointF)), edge, SLOT(setStartPoint(QPointF)));
		connect(end, SIGNAL(positionChanged(QPointF)), edge, SLOT(setEndPoint(QPointF)));
		connect(edge, SIGNAL(mousePressed(QGraphicsSceneMouseEvent*, NetGraphSceneEdge*)), SLOT(edgeMousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneEdge*)));
		connect(edge, SIGNAL(hoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)), SLOT(edgeHoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)));
		connect(edge, SIGNAL(hoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)), SLOT(edgeHoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneEdge*)));
	}
	return edge;
}

NetGraphSceneEdge *NetGraphScene::addFlowEdge(int index, NetGraphSceneNode *start, NetGraphSceneNode *end, int extraOffset, QColor color)
{
	NetGraphSceneEdge *edge = new NetGraphSceneEdge(netGraph->edges[index].source, netGraph->edges[index].dest, index, 0, this);
	edge->setFastMode(fastMode);
	edge->setStartPoint(start->pos());
	edge->setEndPoint(end->pos());
	edge->setZValue(-2);
	edge->setText(netGraph->edges[edge->edgeIndex].tooltip());
	edge->setUsed(netGraph->edges[index].used);
	edge->setUnusedHidden(unusedHidden);
	edge->setFlowEdge(extraOffset, color);
	if (!fastMode) {
		connect(start, SIGNAL(positionChanged(QPointF)), edge, SLOT(setStartPoint(QPointF)));
		connect(end, SIGNAL(positionChanged(QPointF)), edge, SLOT(setEndPoint(QPointF)));
	}
	return edge;
}

NetGraphSceneConnection *NetGraphScene::addConnection(int startIndex, int endIndex,
								  NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	int connectionIndex = netGraph->addConnection(NetGraphConnection(startIndex, endIndex, connectionType, ""));
	return addConnection(connectionIndex, start, end);
}

NetGraphSceneConnection *NetGraphScene::addConnection(int index, NetGraphSceneNode *start, NetGraphSceneNode *end)
{
	NetGraphSceneConnection *c = new NetGraphSceneConnection(netGraph->connections[index].source, netGraph->connections[index].dest, index, 0, this);
	c->setFastMode(fastMode);
	c->setStartPoint(start->pos());
	c->setEndPoint(end->pos());
	c->setZValue(-1);
	c->setText(netGraph->connections[c->connectionIndex].type);
	c->setVisible(!hideConnections);
	if (!fastMode) {
		connect(start, SIGNAL(positionChanged(QPointF)), c, SLOT(setStartPoint(QPointF)));
		connect(end, SIGNAL(positionChanged(QPointF)), c, SLOT(setEndPoint(QPointF)));
		connect(c, SIGNAL(mousePressed(QGraphicsSceneMouseEvent*, NetGraphSceneConnection*)), SLOT(connectionMousePressed(QGraphicsSceneMouseEvent*,NetGraphSceneConnection*)));
		connect(c, SIGNAL(hoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)), SLOT(connectionHoverEnter(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)));
		connect(c, SIGNAL(hoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)), SLOT(connectionHoverLeave(QGraphicsSceneHoverEvent*,NetGraphSceneConnection*)));
	}
	return c;
}


void NetGraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	QGraphicsScene::mouseMoveEvent(mouseEvent);
}

void NetGraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	QGraphicsScene::mouseReleaseEvent(mouseEvent);
}

void NetGraphScene::nodeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node)
{
	if (editMode == MoveNode && mouseEvent->buttons() & Qt::LeftButton) {
		oldMousePos = mouseEvent->scenePos();
		oldNodePos = node->pos();
	} else if (editMode == DelNode && mouseEvent->buttons() & Qt::LeftButton) {
		int nodeIndex = node->nodeIndex;
		int ASNumber = netGraph->nodes[nodeIndex].ASNumber;
		netGraph->deleteNode(nodeIndex);
		delete node;

		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
			if (edge && !edge->flowEdge) {
				if (edge->startIndex == nodeIndex || edge->endIndex == nodeIndex) {
					delete edge;
					continue;
				}
				if (edge->startIndex > nodeIndex)
					edge->startIndex--;
				if (edge->endIndex > nodeIndex)
					edge->endIndex--;
			}

			NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
			if (n) {
				if (n->nodeIndex > nodeIndex)
					n->nodeIndex--;
			}

			NetGraphSceneAS *domain = dynamic_cast<NetGraphSceneAS*>(item);
			if (domain) {
				if (domain->ASNumber == ASNumber) {
					netGraph->computeASHulls(ASNumber);
					foreach (NetGraphAS d, netGraph->domains) {
						if (d.ASNumber == ASNumber) {
							domain->setHull(d.hull);
							break;
						}
					}
					break;
				}
			}
		}

	} else if (editMode == InsertEdgeStart && mouseEvent->buttons() & Qt::LeftButton) {
		getNewEdge()->setVisible(true);
		getNewEdge()->startIndex = getNewEdge()->endIndex = node->nodeIndex;
		getNewEdge()->setStartPoint(node->pos());
		startNode = node;
		editMode = InsertEdgeEnd;
		node->ungrabMouse();
		node->setSel(true);
	} else if (editMode == InsertEdgeEnd && mouseEvent->buttons() & Qt::LeftButton) {
		getNewEdge()->endIndex = node->nodeIndex;
		editMode = InsertEdgeStart;
		startNode->setSel(false);
		node->ungrabMouse();
		getNewEdge()->setVisible(false);
		if (netGraph->canAddEdge(newEdge->startIndex, newEdge->endIndex)) {
			addEdge(newEdge->startIndex, getNewEdge()->endIndex,
				   defaultEdge.bandwidth, defaultEdge.delay_ms,
				   defaultEdge.lossBernoulli, defaultEdge.queueLength,
				   startNode, node);
		}
		destroyNewEdge();
		startNode = 0;
	} else if (editMode == InsertConnectionStart && mouseEvent->buttons() & Qt::LeftButton && node->nodeType == NETGRAPH_NODE_HOST) {
		getNewConnection()->setVisible(true);
		getNewConnection()->startIndex = getNewConnection()->endIndex = node->nodeIndex;
		getNewConnection()->setStartPoint(node->pos());
		startNode = node;
		editMode = InsertConnectionEnd;
		node->ungrabMouse();
		node->setSel(true);
	} else if (editMode == InsertConnectionEnd && mouseEvent->buttons() & Qt::LeftButton && node->nodeType == NETGRAPH_NODE_HOST && getNewConnection()->startIndex != node->nodeIndex) {
		getNewConnection()->endIndex = node->nodeIndex;
		editMode = InsertConnectionStart;
		startNode->setSel(false);
		node->ungrabMouse();
		getNewEdge()->setVisible(false);
		addConnection(getNewConnection()->startIndex, getNewConnection()->endIndex, startNode, node);
		destroyNewConnection();
		startNode = 0;
	}
}

void NetGraphScene::nodeMouseMoved(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node)
{
	if (editMode == MoveNode && mouseEvent->buttons() & Qt::LeftButton) {
		QPointF newMousePos = mouseEvent->scenePos();
		QPointF newPos = oldNodePos + newMousePos - oldMousePos;
		node->setPos(newPos);
		netGraph->nodes[node->nodeIndex].x = node->pos().x();
		netGraph->nodes[node->nodeIndex].y = node->pos().y();
	}
}

void NetGraphScene::nodeMouseReleased(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node)
{
	Q_UNUSED(mouseEvent);
	Q_UNUSED(node);
	if (editMode == MoveNode) {
		QRectF newRect = itemsBoundingRect().adjusted(-100, -100, 100, 100);
		foreach (QGraphicsView *view, views()) {
			if (view) {
				QWidget *viewport = view->viewport();
				if (viewport) {
					if (viewport->width() > newRect.width()) {
						newRect.setWidth(viewport->width());
					}
					if (viewport->height() > newRect.height()) {
						newRect.setHeight(viewport->height());
					}
				}
			}
		}

		// recompute AS hulls
		netGraph->computeASHulls(netGraph->nodes[node->nodeIndex].ASNumber);
		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneAS *domain = dynamic_cast<NetGraphSceneAS*>(item);
			if (domain) {
				if (domain->ASNumber == netGraph->nodes[node->nodeIndex].ASNumber) {
					foreach (NetGraphAS d, netGraph->domains) {
						if (d.ASNumber == netGraph->nodes[node->nodeIndex].ASNumber) {
							domain->setHull(d.hull);
							break;
						}
					}
					break;
				}
			}
		}

		setSceneRect(newRect);
	}
}

void NetGraphScene::nodeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node)
{
	Q_UNUSED(hoverEvent);
	if (editMode == InsertEdgeEnd) {
		getNewEdge()->endIndex = node->nodeIndex;
		if (netGraph->canAddEdge(getNewEdge()->startIndex, getNewEdge()->endIndex)) {
			getNewEdge()->setEndPoint(node->pos());
		}
	} else if (editMode == InsertConnectionEnd && node->nodeType == NETGRAPH_NODE_HOST && getNewConnection()->startIndex != node->nodeIndex) {
		getNewConnection()->endIndex = node->nodeIndex;
		getNewConnection()->setEndPoint(node->pos());
	} else if (editMode == ShowRoutes) {
		if (tooltipTarget != node) {
			tooltip->setHtml(netGraph->nodes[node->nodeIndex].routeTooltip());
			tooltip->setVisible(true);
			tooltipTarget = node;
		}
		tooltip->setPos(hoverEvent->scenePos());
	}
}

void NetGraphScene::nodeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node)
{
	Q_UNUSED(hoverEvent);
	Q_UNUSED(node);
	if (editMode == InsertEdgeEnd) {
		getNewEdge()->endIndex = getNewEdge()->startIndex;
		getNewEdge()->setEndPoint(getNewEdge()->startPoint);
		getNewEdge()->updateShape();
	} else if (editMode == InsertConnectionEnd && node->nodeType == NETGRAPH_NODE_HOST && getNewConnection()->startIndex != node->nodeIndex) {
		getNewConnection()->endIndex = getNewConnection()->startIndex;
		getNewConnection()->setEndPoint(getNewConnection()->startPoint);
		getNewConnection()->updateShape();
	} else if (editMode == ShowRoutes) {
		if (tooltipTarget == node) {
			tooltip->setVisible(false);
			tooltipTarget = 0;
		}
	}
}

void NetGraphScene::edgeMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneEdge *edge)
{
	Q_UNUSED(mouseEvent);
	if (editMode == EditEdge) {
		if (selectedEdge == edge) {
			edge->setSelected(false);
			selectedEdge = 0;
			emit edgeSelected(defaultEdge);
		} else {
			if (selectedEdge)
				selectedEdge->setSelected(false);
			edge->setSelected(true);
			selectedEdge = edge;
			emit edgeSelected(netGraph->edges[selectedEdge->edgeIndex]);
		}
	} else if (editMode == DelEdge) {
		int edgeIndex = edge->edgeIndex;
		netGraph->deleteEdge(edgeIndex);
		delete edge;

		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneEdge *e = dynamic_cast<NetGraphSceneEdge*>(item);
			if (e && !e->flowEdge) {
				if (e->edgeIndex > edgeIndex)
					e->edgeIndex--;
			}
		}
	}
}

void NetGraphScene::edgeHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge)
{
	Q_UNUSED(hoverEvent);
	if (editMode == EditEdge || editMode == DelEdge) {
		edge->setHovered(true);
	}
}

void NetGraphScene::edgeHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneEdge *edge)
{
	Q_UNUSED(hoverEvent);
	if (editMode == EditEdge || editMode == DelEdge) {
		edge->setHovered(false);
	}
}

void NetGraphScene::connectionMousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneConnection *c)
{
	Q_UNUSED(mouseEvent);
	if (editMode == DelConnection) {
		int connectionIndex = c->connectionIndex;
		netGraph->deleteConnection(connectionIndex);
		delete c;

		foreach (QGraphicsItem *item, items()) {
			NetGraphSceneConnection *c2 = dynamic_cast<NetGraphSceneConnection*>(item);
			if (c2) {
				if (c2->connectionIndex > connectionIndex)
					c2->connectionIndex--;
			}
		}
	}
}

void NetGraphScene::connectionHoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c)
{
	Q_UNUSED(hoverEvent);
	if (editMode == DelConnection) {
		c->setHovered(true);
	}
}

void NetGraphScene::connectionHoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneConnection *c)
{
	Q_UNUSED(hoverEvent);
	if (editMode == DelConnection) {
		c->setHovered(false);
	}
}

void NetGraphScene::delayChanged(int val)
{
	if (editMode == EditEdge && selectedEdge) {
		netGraph->edges[selectedEdge->edgeIndex].delay_ms = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
	} else {
		defaultEdge.delay_ms = val;
	}
}

void NetGraphScene::bandwidthChanged(double val)
{
	if (editMode == EditEdge && selectedEdge) {
		netGraph->edges[selectedEdge->edgeIndex].bandwidth = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
	} else {
		defaultEdge.bandwidth = val;
	}
}

void NetGraphScene::lossRateChanged(double val)
{
	if (editMode == EditEdge && selectedEdge) {
		netGraph->edges[selectedEdge->edgeIndex].lossBernoulli = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
	} else {
		defaultEdge.lossBernoulli = val;
	}
}

void NetGraphScene::queueLenghtChanged(int val)
{
	if (editMode == EditEdge && selectedEdge) {
		netGraph->edges[selectedEdge->edgeIndex].queueLength = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
	} else {
		defaultEdge.queueLength = val;
	}
}

void NetGraphScene::samplingPeriodChanged(quint64 val)
{
	if (editMode == EditEdge && selectedEdge) {
		netGraph->edges[selectedEdge->edgeIndex].timelineSamplingPeriod = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
	} else {
		defaultEdge.timelineSamplingPeriod = val;
	}
}

void NetGraphScene::samplingChanged(bool val)
{
	if (editMode == EditEdge && selectedEdge) {
		netGraph->edges[selectedEdge->edgeIndex].recordSampledTimeline = val;
		selectedEdge->setText(netGraph->edges[selectedEdge->edgeIndex].tooltip());
	} else {
		defaultEdge.recordSampledTimeline = val;
	}
}

void NetGraphScene::ASNumberChanged(int val)
{
	defaultASNumber = val;
}

void NetGraphScene::connectionTypeChanged(QString val)
{
	connectionType = val;
}

void NetGraphScene::reload()
{
	clear();
	initTooltip();

	selectedEdge = 0;
	startNode = 0;

	if (!netGraph)
		return;

	fastMode = (netGraph->nodes.count() > SHOW_MAX_NODES);

	QList<NetGraphSceneNode *> nodes;
	foreach (NetGraphNode node, netGraph->nodes) {
		nodes << addNode(node.index);
	}

	foreach (NetGraphEdge edge, netGraph->edges) {
		addEdge(edge.index, nodes[edge.source], nodes[edge.dest]);
	}

	foreach (NetGraphConnection c, netGraph->connections) {
		addConnection(c.index, nodes[c.source], nodes[c.dest]);
	}

	foreach (NetGraphAS domain, netGraph->domains) {
		addAS(domain.index);
	}

	setSceneRect(itemsBoundingRect().adjusted(-100, -100, 100, 100));
}

void NetGraphScene::initTooltip()
{
	tooltip = new QGraphicsTooltip();
	addItem(tooltip);
	tooltip->setZValue(100000);
	tooltip->setVisible(false);
	tooltip->setAcceptHoverEvents(false);
}

void NetGraphScene::setHideConnections(bool value)
{
	hideConnections = value;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneConnection *c = dynamic_cast<NetGraphSceneConnection*>(item);
		if (c) {
			c->setVisible(!hideConnections);
		}
	}
}

void NetGraphScene::setHideFlows(bool value)
{
	hideFlows = value;
	showFlows();
}

void NetGraphScene::setHideEdges(bool value)
{
	hideEdges = value;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *e = dynamic_cast<NetGraphSceneEdge*>(item);
		if (e && !e->flowEdge) {
			e->setVisible(!hideEdges);
		}
	}
}

void NetGraphScene::routingChanged()
{
	showFlows();
}

void NetGraphScene::showFlows()
{
	QHash<int, NetGraphSceneNode*> sceneNodes;

	// remove previous flow edges
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && edge->flowEdge) {
			delete edge;
			continue;
		}

		NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
		if (n) {
			sceneNodes[n->nodeIndex] = n;
		}
	}

	if (hideFlows)
		return;

	QHash<int, int> extraOffsets; // edge index -> offset
	foreach (NetGraphConnection c, netGraph->connections) {
		NetGraphPath p12 = netGraph->pathByNodeIndex(c.source, c.dest);
		foreach (NetGraphEdge e, p12.edgeSet) {
			NetGraphSceneNode *start = sceneNodes.value(e.source, NULL);
			NetGraphSceneNode *end = sceneNodes.value(e.dest, NULL);

			if (start && end) {
				if (!extraOffsets.contains(e.index))
					extraOffsets[e.index] = 1;
				addFlowEdge(e.index, start, end, extraOffsets[e.index], NetGraphSceneConnection::getColorByIndex(c.index));
				extraOffsets[e.index]++;
			}
		}

		NetGraphPath p21 = netGraph->pathByNodeIndex(c.dest, c.source);
		foreach (NetGraphEdge e, p21.edgeSet) {
			NetGraphSceneNode *start = sceneNodes.value(e.source, NULL);
			NetGraphSceneNode *end = sceneNodes.value(e.dest, NULL);

			if (start && end) {
				if (!extraOffsets.contains(e.index))
					extraOffsets[e.index] = 1;
				addFlowEdge(e.index, start, end, extraOffsets[e.index], NetGraphSceneConnection::getColorByIndex(c.index));
				extraOffsets[e.index]++;
			}
		}
	}
}

void NetGraphScene::usedChanged()
{
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && !edge->flowEdge && edge->edgeIndex >= 0) {
			edge->setUsed(netGraph->edges[edge->edgeIndex].used);
		}

		NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
		if (n) {
			n->setUsed(netGraph->nodes[n->nodeIndex].used);
		}
	}
}

void NetGraphScene::setUnusedHidden(bool unusedHidden)
{
	this->unusedHidden = unusedHidden;
	foreach (QGraphicsItem *item, items()) {
		NetGraphSceneEdge *edge = dynamic_cast<NetGraphSceneEdge*>(item);
		if (edge && !edge->flowEdge && edge->edgeIndex >= 0) {
			edge->setUnusedHidden(unusedHidden);
		}

		NetGraphSceneNode *n = dynamic_cast<NetGraphSceneNode*>(item);
		if (n) {
			n->setUnusedHidden(unusedHidden);
		}
	}
}

NetGraphSceneEdge *NetGraphScene::getNewEdge()
{
	if (!newEdge) {
		newEdge = new NetGraphSceneEdge(-1, -1, -1, 0, this);
		newEdge->setZValue(-1);
		newEdge->setPos(0, 0);
	}
	return newEdge;
}

void NetGraphScene::destroyNewEdge()
{
	delete newEdge;
	newEdge = 0;
}

NetGraphSceneConnection *NetGraphScene::getNewConnection()
{
	if (!newConnection) {
		newConnection = new NetGraphSceneConnection(-1, -1, -1, 0, this);
		newConnection->setZValue(-1);
		newConnection->setPos(0, 0);
	}
	return newConnection;
}

void NetGraphScene::destroyNewConnection()
{
	delete newConnection;
	newConnection = 0;
}

