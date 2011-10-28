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

#include "gephilayout.h"

#include <QtXml>
#include "util.h"

bool exportGexf(NetGraph &g, QString fileName)
{
	QDomDocument doc("");
	doc.appendChild(doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"ISO-8859-1\""));

	QDomElement gexf = doc.createElement("gexf");
	doc.appendChild(gexf);

	QDomElement graph = doc.createElement("graph");
	gexf.appendChild(graph);
	graph.setAttribute("mode", "static");
	graph.setAttribute("defaultedgetype", "directed");

	QDomElement nodes = doc.createElement("nodes");
	graph.appendChild(nodes);

	foreach (NetGraphNode n, g.nodes) {
		QDomElement node = doc.createElement("node");
		node.setAttribute("id", n.index);
		nodes.appendChild(node);
	}

	QDomElement edges = doc.createElement("edges");
	graph.appendChild(edges);

	foreach (NetGraphEdge e, g.edges) {
		QDomElement edge = doc.createElement("edge");
		edge.setAttribute("id", e.index);
		edge.setAttribute("source", e.source);
		edge.setAttribute("target", e.dest);
		if (g.nodes[e.source].ASNumber == g.nodes[e.dest].ASNumber) {
			edge.setAttribute("weight", 10.0);
		} else {
			edge.setAttribute("weight", 1.0);
		}
		edges.appendChild(edge);
	}

	return saveFile(fileName, doc.toString());
}

bool importGexfPositions(NetGraph &g, QString fileName)
{
	QDomDocument doc(fileName);
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		fdbg << "Could not open file in read mode, file name:" << fileName;
		return false;
	}
	QString errorMsg;
	int errorLine;
	int errorColumn;
	if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
		fdbg << "Could not read XML, file name:"
				<< fileName + ":" + QString::number(errorLine) + ":" + QString::number(errorColumn) <<
				errorMsg;
		file.close();
		return false;
	}
	file.close();

	QDomElement gexf = doc.documentElement();

	QDomNodeList nodes = gexf.elementsByTagName("node");
	for (int iNode = 0; iNode < nodes.count(); iNode++) {
		QDomNode node = nodes.at(iNode);

		bool ok;
		int id = node.attributes().namedItem("id").toAttr().value().toInt(&ok);
		if (!ok || id < 0 || id >= g.nodes.count()) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable";
			return false;
		}

		QDomNodeList position = node.toElement().elementsByTagName("viz:position");
		if (position.count() != 1) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable";
			return false;
		}
		QDomNode pos = position.at(0);
		double x = pos.attributes().namedItem("x").toAttr().value().toDouble(&ok);
		if (!ok) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable";
			return false;
		}
		double y = pos.attributes().namedItem("y").toAttr().value().toDouble(&ok);
		if (!ok) {
			qDebug() << __FILE__ << __LINE__ << "gexf unreadable";
			return false;
		}

		g.nodes[id].x = x;
		g.nodes[id].y = y;
	}

	return true;
}

bool GephiLayout::layout(NetGraph &g)
{
	QString inputFileName = "in.gexf";
	QString outputFileName = "out.gexf";

	if (!exportGexf(g, inputFileName)) {
		qDebug() << "Could not export gexf";
		return false;
	}

	QString output;
	int exitCode;
	if (!runProcess("java", QStringList() << "-jar" << "../line-layout/gephi-toolkit-demos/dist/ToolkitDemos.jar" << inputFileName << outputFileName,
				 output, -1, false, &exitCode) || exitCode != 0) {
		qDebug() << "Could not layout gexf" << output;
		return false;
	}

	return importGexfPositions(g, outputFileName);
}
