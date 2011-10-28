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
#include "netgraphsceneedge.h"

#define PI 3.14159265359
#define RAD_TO_DEG (180.0 / 3.14159265359)
#define DEG_TO_RAD (3.14159265359 / 180.0)

NetGraphSceneEdge::NetGraphSceneEdge(int startIndex, int endIndex, int edgeIndex,
							  QGraphicsItem *parent, QGraphicsScene *scene) :
	QGraphicsObject(parent)
{
	if (scene && !parent)
	    scene->addItem(this);

	this->startIndex = startIndex;
	this->endIndex = endIndex;
	this->edgeIndex = edgeIndex;
	this->setAcceptHoverEvents(true);
	text = "";
	selected = false;
	hovered = false;
	font = QFont("Arial", 8);
	// how far from the line that connects the centers of the nodes should we draw the arrow
	offset = 5;
	tipw = 6;
	tiph = 8;
	textSpacing = 2;
	used = true;
	unusedHidden = false;
	flowEdge = false;
	color = QColor(Qt::black);
}

QRectF NetGraphSceneEdge::boundingRect() const
{
	return shape().boundingRect();
}

QPainterPath NetGraphSceneEdge::shape() const
{
	double dist = sqrt((endPoint.x() - startPoint.x()) * (endPoint.x() - startPoint.x()) +
				    (endPoint.y() - startPoint.y()) * (endPoint.y() - startPoint.y()));

	QFontMetricsF fm(font);
	double textHeight = fm.height();

	QPolygonF p = QPolygonF(QRectF(-(textHeight + textSpacing), -dist, textHeight + textSpacing + tipw/2, dist));

	QTransform transform;
	transform = transform.translate(startPoint.x(), startPoint.y());
	transform = transform.rotate(90 + atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG);
	transform = transform.translate(-offset, 0);

	p = p * transform;

	QPainterPath path;
	path.addPolygon(p);
	return path;
}

void NetGraphSceneEdge::paint(QPainter *painter,
						const QStyleOptionGraphicsItem *option,
						QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	if (startIndex == endIndex)
		return;

	if (!used && unusedHidden)
		return;

	if (selected) {
		if (hovered) {
			painter->setPen(Qt::cyan);
			painter->setBrush(Qt::cyan);
		} else {
			painter->setPen(Qt::blue);
			painter->setBrush(Qt::blue);
		}
	} else {
		if (hovered) {
			painter->setPen(Qt::darkGray);
			painter->setBrush(Qt::darkGray);
		} else {
			painter->setPen(color);
			painter->setBrush(color);
		}
	}

	double dist = sqrt((endPoint.x() - startPoint.x()) * (endPoint.x() - startPoint.x()) +
				    (endPoint.y() - startPoint.y()) * (endPoint.y() - startPoint.y()));

	QTransform transform = painter->transform();

	painter->translate(startPoint);
	painter->rotate(270 + atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG);
	painter->translate(offset, 0);

	painter->drawLine(0, 0, 0, dist);

	if (!flowEdge) {
		QPolygonF tip;
		float tipy;
		if (!fastMode) {
			tipy = sqrt(NETGRAPH_NODE_RADIUS * NETGRAPH_NODE_RADIUS - offset * offset);
		} else {
			tipy = sqrt(NETGRAPH_NODE_RADIUS_FAST * NETGRAPH_NODE_RADIUS_FAST - offset * offset);
		}
		tip << QPointF(0, dist - tipy)
				<< QPointF(tipw/2, dist - tipy - tiph)
				<< QPointF(-tipw/2, dist - tipy - tiph);
			painter->drawPolygon(tip);

		if (!fastMode) {
			QFontMetricsF fm(font);
			double textWidth = fm.width(text);
			double textHeight = fm.height();

			if (1 || textWidth + textSpacing <= dist - 2 * tipy) {
				painter->setFont(font);

				double angle = atan2(endPoint.y() - startPoint.y(), endPoint.x() - startPoint.x()) * RAD_TO_DEG;

				if (fabs(angle) <= 90) {
					painter->rotate(90);

					painter->drawText(tipy + textSpacing, -(int)(textSpacing + textHeight + 0.5), (int)(dist - 2 * tipy + 0.5), (int)(textHeight + 0.5), Qt::AlignCenter | Qt::AlignVCenter, text);
				} else {
					painter->rotate(-90);
					int textX;
					if (textWidth + textSpacing <= dist - 2 * tipy) {
						textX = -(tipy + textWidth + textSpacing);
					} else {
						textX = -(tipy + dist - 2 * tipy);
					}
					painter->drawText(-tipy + textSpacing - (int)(dist - 2 * tipy + 0.5), textSpacing, (int)(dist - 2 * tipy + 0.5), (int)(textHeight + 0.5), Qt::AlignCenter | Qt::AlignVCenter, text);
					// painter->drawText(textX, textSpacing, (int)(dist - 2 * tipy + 0.5), (int)(textHeight + 0.5), Qt::AlignCenter | Qt::AlignVCenter, text);
				}
			}
		}
	}

	painter->setTransform(transform);
	painter->setPen(Qt::red);
	painter->setBrush(Qt::NoBrush);
//	painter->drawPath(shape());
//	painter->drawRect(boundingRect());
}

void NetGraphSceneEdge::setStartPoint(QPointF newStartPoint)
{
	startPoint = newStartPoint;
	updateShape();
}

void NetGraphSceneEdge::setEndPoint(QPointF newEndPoint)
{
	endPoint = newEndPoint;
	updateShape();
}

void NetGraphSceneEdge::setFastMode(bool fastMode)
{
	this->fastMode = fastMode;
	if (fastMode) {
		offset = 1;
		tipw = 2;
		tiph = 3;
		textSpacing = 2;
	} else {
		offset = 5;
		tipw = 6;
		tiph = 8;
		textSpacing = 2;
	}
	update();
}

void NetGraphSceneEdge::setUsed(bool used)
{
	this->used = used;
	update();
}

void NetGraphSceneEdge::setUnusedHidden(bool unusedHidden)
{
	this->unusedHidden = unusedHidden;
	update();
}

void NetGraphSceneEdge::setSelected(bool newSelected)
{
	selected = newSelected;
	update();
}

void NetGraphSceneEdge::setHovered(bool newHovered)
{
	hovered = newHovered;
	update();
}

void NetGraphSceneEdge::setText(QString text)
{
	this->text = text;
	update();
}

void NetGraphSceneEdge::setFlowEdge(int extraOffset, QColor color)
{
	flowEdge = true;
	offset += extraOffset % 10;
	this->color = color;
	update();
}

void NetGraphSceneEdge::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mousePressed(mouseEvent, this);
}

void NetGraphSceneEdge::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mouseMoved(mouseEvent, this);
}

void NetGraphSceneEdge::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	emit mouseReleased(mouseEvent, this);
}

void NetGraphSceneEdge::hoverEnterEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverEnter(hoverEvent, this);
}

void NetGraphSceneEdge::hoverMoveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverMove(hoverEvent, this);
}

void NetGraphSceneEdge::hoverLeaveEvent(QGraphicsSceneHoverEvent *hoverEvent)
{
	emit hoverLeave(hoverEvent, this);
}
