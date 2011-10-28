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

void MainWindow::on_btnDeploy_clicked()
{
	ui->txtDeploy->clear();

	// Make sure we saved
	if (!on_actionSave_triggered())
		return;

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::deploy);
	voidWatcher.setFuture(future);
}

void MainWindow::on_btnComputeRoutes_clicked()
{
	ui->txtDeploy->clear();

	// Make sure we saved
	on_actionSave_triggered();

	// Start the computation.
	blockingOperationStarting();
	QFuture<void> future = QtConcurrent::run(this, &MainWindow::computeRoutes);
	voidWatcher.setFuture(future);
}

void MainWindow::computeRoutes()
{
	emit logInformation(ui->txtDeploy, "Computing routes");
	netGraph.computeRoutes();
	netGraph.updateUsed();
	if (!netGraph.saveToFile()) {
		emit logError(ui->txtDeploy, "Could not save graph.");
		return;
	}

	emit logSuccess(ui->txtDeploy, "OK.");
	emit routingChanged();
	emit usedChanged();
}

void MainWindow::generateHostDeploymentScript()
{
	QStringList lines;
	lines << QString("#!/usr/bin/perl");

	QString netdev = REMOTE_DEDICATED_IF_HOSTS;

	lines << QString("sub command {\n"
					 "my ($cmd) = @_;\n"
					 "print \"$cmd\\n\";\n"
					 "system \"sudo $cmd\";\n"
					 "}");

	lines << QString("# Flush the routing table - only the 10/8 routes\n"
					 "foreach my $route (`netstat -rn | grep '^10.'`) {\n"
					 "my ($dest, $gate, $linuxmask) = split ' ',$route;\n"
					 "command(\"/sbin/route delete -net $dest netmask $linuxmask\");\n"
					 "}");

	lines << QString("#Set interface params");
	lines << QString("command \"sh -c 'echo 0 > /proc/sys/net/ipv4/conf/%1/rp_filter'\";").arg(netdev);
	lines << QString("command \"sh -c 'echo 1 > /proc/sys/net/ipv4/conf/%1/accept_local'\";").arg(netdev);
	lines << QString("command \"sh -c 'echo 0 > /proc/sys/net/ipv4/conf/%1/accept_redirects'\";").arg(netdev);
	lines << QString("command \"sh -c 'echo 0 > /proc/sys/net/ipv4/conf/%1/send_redirects'\";").arg(netdev);

	lines << QString("#Set up interface aliases");

	foreach (NetGraphNode n, netGraph.nodes) {
		if (n.nodeType != NETGRAPH_NODE_HOST)
			continue;
		if (!n.used)
			continue;
		lines << QString("command \"sh -c '/sbin/ifconfig %1:%2 down 2> /dev/null'\";").arg(netdev).arg(n.index);
		lines << QString("command \"sh -c '/sbin/ifconfig %1:%2 %3 netmask 255.128.0.0'\";").arg(netdev).arg(n.index).arg(n.ip());
	}

	QString gateway = REMOTE_DEDICATED_IP_ROUTER;
	lines << QString("command \"sh -c '/sbin/route add -net 10.128.0.0/9 gw %1'\"").arg(gateway);

	saveFile("deployhost.pl", lines.join("\n"));
}

void MainWindow::deploy()
{
	QString description;
	QString command;
	QStringList args;
	QString graphName = getGraphName();
	QString core = getCoreHostname();
	QStringList clients = getClientHostnames();

	emit logClear(ui->txtDeploy);

	netGraph.updateUsed();
	netGraph.computeRoutes();
	netGraph.saveToFile();
	generateHostDeploymentScript();

	description = "Upload the graph file to the router emulator";
	command = "scp";
	args = QStringList() << graphName + ".graph" << QString("root@%1:~").arg(core);
	if (!runCommand(ui->txtDeploy, command, args, description)) {
		emit logError(ui->txtDeploy, "Deployment aborted.");
		return;
	}

	description = "Upload the deployment script to the host emulator";
	command = "scp";
	args = QStringList() << "deployhost.pl" << QString("root@%1:~").arg(clients.first());
	if (!runCommand(ui->txtDeploy, command, args, description)) {
		emit logError(ui->txtDeploy, "Deployment aborted.");
		return;
	}

	description = "Run the deployment script on the host emulator";
	command = "ssh";
	foreach (QString client, clients) {
		args = QStringList() << "-f" << QString("root@%1").arg(client) << "sh -c '(perl deployhost.pl 1> deploy.log 2> deploy.err &)'";
		if (!runCommand(ui->txtDeploy, command, args, description)) {
			emit logError(ui->txtDeploy, "Deployment aborted.");
			return;
		}
	}

	emit logInformation(ui->txtDeploy, "Wait...");
	foreach (QString client, clients) {
		int fastSleeps = 5;
		while (1) {
			command = "ssh";
			args = QStringList() << QString("root@%1").arg(client) << "sh -c 'pstree | grep deployhost.pl || /bin/true'";
			description = "waiting...";
			QString output;
			if (!runCommand(ui->txtDeploy, command, args, description, false, output)) {
				emit logError(ui->txtDeploy, "Deployment aborted.");
				return;
			}
			output = output.trimmed();
			if (output.isEmpty())
				break;
			if (fastSleeps > 0) {
				sleep(1);
				fastSleeps--;
			} else {
				sleep(5);
			}
		}
	}

	description = "Show the deploy log for the host emulator";
	command = "ssh";
	foreach (QString client, clients) {
		args = QStringList() << QString("root@%1").arg(client) << "sh -c 'cat deploy.log'";
		if (!runCommand(ui->txtDeploy, command, args, description)) {
			emit logError(ui->txtDeploy, "Deployment aborted.");
			return;
		}
	}

	description = "Check for deploy errors for the host emulator";
	command = "ssh";
	foreach (QString client, clients) {
		args = QStringList() << QString("root@%1").arg(client) << "sh -c 'cat deploy.err'";
		QString output;
		if (!runCommand(ui->txtDeploy, command, args, description, false, output)) {
			emit logError(ui->txtDeploy, "Deployment aborted.");
			return;
		}
		output = output.trimmed();
		if (!output.isEmpty()) {
			emit logError(ui->txtDeploy, output);
			emit logError(ui->txtDeploy, "Deployment aborted.");
			return;
		}
	}

	description = "Run the deployment script on the router emulator";
	command = "ssh";
	args = QStringList() << "-f" << QString("root@%1").arg(core) << "sh -c '(deploycore.pl 1> deploy.log 2> deploy.err &)'";
	if (!runCommand(ui->txtDeploy, command, args, description)) {
		emit logError(ui->txtDeploy, "Deployment aborted.");
		return;
	}

	emit logInformation(ui->txtDeploy, "Wait...");
	int fastSleeps = 5;
	while (1) {
		command = "ssh";
		args = QStringList() << QString("root@%1").arg(core) << "sh -c 'pstree | grep deploycore.pl || /bin/true'";
		description = "waiting...";
		QString output;
		if (!runCommand(ui->txtDeploy, command, args, description, false, output)) {
			emit logError(ui->txtDeploy, "Deployment aborted.");
			return;
		}
		output = output.trimmed();
		if (output.isEmpty())
			break;
		if (fastSleeps > 0) {
			sleep(1);
			fastSleeps--;
		} else {
			sleep(5);
		}
	}

	description = "Show the deploy log for the router emulator";
	command = "ssh";
	args = QStringList() << QString("root@%1").arg(core) << "sh -c 'cat deploy.log'";
	if (!runCommand(ui->txtDeploy, command, args, description)) {
		emit logError(ui->txtDeploy, "Deployment aborted.");
		return;
	}

	description = "Check for deploy errors for the router emulator";
	command = "ssh";
	{
		args = QStringList() << QString("root@%1").arg(core) << "sh -c 'cat deploy.err'";
		QString output;
		if (!runCommand(ui->txtDeploy, command, args, description, false, output)) {
			emit logError(ui->txtDeploy, "Deployment aborted.");
			return;
		}
		output = output.trimmed();
		if (!output.isEmpty()) {
			emit logError(ui->txtDeploy, output);
			emit logError(ui->txtDeploy, "Deployment aborted.");
			return;
		}
	}

	emit logSuccess(ui->txtDeploy, "Deployment finished.");
}


