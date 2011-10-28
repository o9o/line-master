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

#ifndef NETGRAPHSCENENODE_H
#define NETGRAPHSCENENODE_H

#include <QGraphicsItem>
#include <QtGui>

#include "netgraph.h"

class NetGraphSceneNode : public QGraphicsObject
{
	Q_OBJECT

public:
	explicit NetGraphSceneNode(int nodeIndex, int nodeType,
						  QPixmap pixmap,
						  QGraphicsItem *parent = 0, QGraphicsScene *scene = 0);
	int nodeIndex;
	int nodeType;
	bool selected;
	QFont font;
	QPixmap pixmap;
	bool fastMode;
	bool used;
	bool unusedHidden;

	QRectF boundingRect() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void setPos(const QPointF &pos);
	void setSel(bool newSelected);
	void setFastMode(bool fastMode);
	void setUsed(bool used);
	void setUnusedHidden(bool unusedHidden);

signals:
	void mousePressed(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void mouseMoved(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void mouseReleased(QGraphicsSceneMouseEvent *mouseEvent, NetGraphSceneNode *node);
	void hoverEnter(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node);
	void hoverMove(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node);
	void hoverLeave(QGraphicsSceneHoverEvent *hoverEvent, NetGraphSceneNode *node);

	void positionChanged(QPointF newPosition);

public slots:

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);
	void hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent);
	void hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent);
};

#endif // NETGRAPHSCENENODE_H
