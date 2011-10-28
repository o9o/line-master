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

#include "netgraphscenenode.h"

NetGraphSceneNode::NetGraphSceneNode(int nodeIndex, int nodeType,
									 QPixmap pixmap,
									 QGraphicsItem *parent, QGraphicsScene *scene) :
	QGraphicsObject(parent), pixmap(pixmap)
{
	if (scene && !parent)
		scene->addItem(this);

	font = QFont("Arial", 12);

	this->nodeIndex = nodeIndex;
	this->nodeType = nodeType;
	this->setAcceptHoverEvents(true);
	selected = false;
	used = true;
	unusedHidden = false;
}

void NetGraphSceneNode::setPos(const QPointF &pos)
{
	QGraphicsItem::setPos(pos);
	emit positionChanged(pos);
}

void NetGraphSceneNode::setSel(bool newSelected)
{
	selected = newSelected;
	update();
}

void NetGraphSceneNode::setFastMode(bool fastMode)
{
	this->fastMode = fastMode;
	update();
}

void NetGraphSceneNode::setUsed(bool used)
{
	this->used = used;
	update();
}

void NetGraphSceneNode::setUnusedHidden(bool unusedHidden)
{
	this->unusedHidden = unusedHidden;
	update();
}

QRectF NetGraphSceneNode::boundingRect() const
{
	if (!fastMode) {
		//return QRectF(-pixmap.width()/2, -pixmap.height()/2, pixmap.width(), pixmap.height());
		return QRectF(-NETGRAPH_NODE_RADIUS, -NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS);
	} else {
		return QRectF(-NETGRAPH_NODE_RADIUS_FAST, -NETGRAPH_NODE_RADIUS_FAST,
					  2*NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST);
	}
}

void NetGraphSceneNode::paint(QPainter *painter,
							  const QStyleOptionGraphicsItem *option,
							  QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	if (!used && unusedHidden)
		return;

	painter->setPen(Qt::black);
	if (nodeType == NETGRAPH_NODE_HOST) {
		painter->setBrush(Qt::green);
	} else if (nodeType == NETGRAPH_NODE_GATEWAY) {
		painter->setBrush(Qt::yellow);
	} else if (nodeType == NETGRAPH_NODE_ROUTER) {
		painter->setBrush(Qt::blue);
	} else if (nodeType == NETGRAPH_NODE_BORDER) {
		painter->setBrush(Qt::gray);
	}

	if (!fastMode) {
		painter->drawPixmap(-pixmap.width()/2, -pixmap.height()/2, pixmap.width(), pixmap.height(), pixmap);
	} else {
		painter->drawEllipse(-NETGRAPH_NODE_RADIUS_FAST, -NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST);
	}

	if (!fastMode) {
		QString label = QString::number(nodeIndex);
		font.setBold(true);
		QFontMetricsF fm(font);
		QRectF textBoundingRect = fm.tightBoundingRect(label);

		const int spacing = 4;
		const int padding = 2;

		if (nodeType == NETGRAPH_NODE_HOST) {
			painter->setPen(Qt::NoPen);
			painter->setBrush(QColor(255, 255, 255, 170));
			painter->drawRect(-textBoundingRect.width()/2 - padding, pixmap.height()/2 - fm.descent() - spacing - textBoundingRect.height() - padding, textBoundingRect.width() + 2*padding, textBoundingRect.height() + 2*padding);
		}

		painter->setPen(Qt::black);
		painter->setFont(font);
		painter->drawText(QPointF(-textBoundingRect.width()/2.0, pixmap.height()/2 - fm.descent() - spacing), label);
	}

	if (selected) {
		if (!fastMode) {
			painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
			painter->setBrush(Qt::NoBrush);
			painter->drawRect(-NETGRAPH_NODE_RADIUS, -NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS, 2*NETGRAPH_NODE_RADIUS);
		} else {
			painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
			painter->setBrush(Qt::NoBrush);
			painter->drawRect(-NETGRAPH_NODE_RADIUS_FAST, -NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST, 2*NETGRAPH_NODE_RADIUS_FAST);
		}
	}
}

void NetGraphSceneNode::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseEvent->accept();
	emit mousePressed(mouseEvent, this);
}

void NetGraphSceneNode::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseEvent->accept();
	emit mouseMoved(mouseEvent, this);
}

void NetGraphSceneNode::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	mouseEvent->accept();
	emit mouseReleased(mouseEvent, this);
}

void NetGraphSceneNode::hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	hoverEvent->accept();
	emit hoverEnter(hoverEvent, this);
}

void NetGraphSceneNode::hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	hoverEvent->accept();
	emit hoverMove(hoverEvent, this);
}

void NetGraphSceneNode::hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	hoverEvent->accept();
	emit hoverLeave(hoverEvent, this);
}
