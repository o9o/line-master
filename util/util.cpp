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

#include "util.h"

#include <QFile>
#include <QProcess>
#include <execinfo.h>

#include "debug.h"

void printBacktrace()
{
	void *frames[30];
	size_t frame_count;

	// get void*'s for all entries on the stack
	frame_count = backtrace(frames, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Backtrace:\n");
	backtrace_symbols_fd(frames, frame_count, 2); // 2 is stderr
}

bool saveFile(QString name, QString content)
{
	QFile file(name);
	if (!file.open(QIODevice::WriteOnly)) {
		fdbg << "Could not open file in write mode, file name:" << name;
		return false;
	}

	QByteArray data = content.toAscii();

	while (data.length() > 0) {
		int bytes = file.write(data);
		if (bytes < 0) {
			fdbg << "Write error, file name:" << name;
			return false;
		}
		data = data.mid(bytes);
	}

	file.close();

	return true;
}

bool readFile(QString name, QString &content, bool silent)
{
	QFile file(name);
	if (!file.open(QIODevice::ReadOnly)) {
		if (!silent) {
			fdbg << "Could not open file in read mode, file name:" << name;
		}
		return false;
	}

	QByteArray data = file.readAll();

	if (data.isEmpty() && file.size() > 0)
		return false;

	content = data;

	file.close();

	return true;
}

bool saveTextArchive(QString name, const QStringList &fileNames, const QStringList &files)
{
	Q_ASSERT_FORCE(fileNames.count() == files.count());

	QFile out(name);
	if (!out.open(QIODevice::WriteOnly)) {
		fdbg << "Could not open file" << name << "in write mode";
		return false;
	}

	for (int i = 0; i < files.size(); ++i) {
		QString fileName = fileNames.at(i);

		if (i % 1000 == 0) {
			//qDebug() << "Progress:" << i << (i * 100.0 / files.size()) << "%";
		}

		QString text = files.at(i);
		int length = text.toAscii().length();
		text.prepend(fileName + " " + QString::number(length) + "\n");

		QByteArray data = text.toAscii();
		while (data.length() > 0) {
			int bytes = out.write(data);
			if (bytes < 0) {
				fdbg << "Write error";
				return false;
			}
			data = data.mid(bytes);
		}
	}

	out.close();
	return true;
}

bool readTextArchive(QString name, QStringList &fileNames, QStringList &files, bool silent)
{
	fileNames.clear();
	files.clear();

	QFile in(name);
	if (!in.open(QIODevice::ReadOnly)) {
		if (!silent) {
			fdbg << "Could not open file" << name << "in read mode";
		}
		return false;
	}

	QByteArray content = in.readAll();

	if (content.isEmpty() && in.size() > 0)
		return false;

	in.close();

	for (int i = 0; i < content.size();) {
		QByteArray line;
		while (i < content.size()) {
			if (content.at(i) == '\n') {
				i++;
				break;
			} else {
				line.append(content.at(i));
				i++;
			}
		}

		if (i >= content.size()) {
			fdbg << "Archive" << name << "damaged at byte" << i;
			return false;
		}

		QString s(line);
		QStringList tokens = s.split(' ');
		if (tokens.count() < 2) {
			fdbg << "Archive" << name << "damaged at byte" << i;
			return false;
		}
		QString fileName = QStringList(tokens.mid(0, tokens.count() - 1)).join(" ");
		QString fileSize = tokens.last();
		bool ok;
		int size = fileSize.toInt(&ok);
		if (!ok) {
			fdbg << "Archive" << name << "damaged at byte" << i;
			return false;
		}

		QByteArray data;
		for (; size > 0 && i < content.size(); size--, i++) {
			data.append(content.at(i));
		}
		QString fileContent = data;

		fileNames << fileName;
		files << fileContent;
	}
	return true;
}

bool keyValHigherEqual(const KeyVal &s1, const KeyVal &s2)
{
	return s1.val >= s2.val;
}

bool randomLessThan(const QString &, const QString &)
{
	return rand() < (RAND_MAX >> 1);
}

bool runProcess(QString name, QStringList args, bool silent)
{
	QProcess p;
	p.setProcessChannelMode(QProcess::ForwardedChannels);
	p.start(name, args);
	if (!p.waitForStarted(-1)) {
		if (!silent) qDebug() << "ERROR: Could not start:" << name;
		return false;
	}

	if (!p.waitForFinished(-1)) {
		if (!silent) qDebug() << "ERROR:" << name << "failed (error or timeout).";
		return false;
	}
	return true;
}

bool runProcess(QString name, QStringList args, QString &output, int timeoutsec, bool silent, int *exitCode)
{
	output.clear();

	QProcess p;
	p.setProcessChannelMode(QProcess::MergedChannels);
	p.start(name, args);
	if (!p.waitForStarted(timeoutsec == -1 ? -1 : timeoutsec * 1000)) {
		if (!silent) qDebug() << "ERROR: Could not start:" << name;
		return false;
	}

	if (!p.waitForFinished(timeoutsec == -1 ? -1 : timeoutsec * 1000)) {
		if (!silent) qDebug() << "ERROR:" << name << "failed (error or timeout). Command:" << name << args;
		p.kill();
		p.waitForFinished(timeoutsec * 1000 < 5000 ? timeoutsec * 1000 : 5000);
		output = p.readAllStandardOutput();
		return false;
	}
	output = p.readAllStandardOutput();

	if (exitCode)
		*exitCode = p.exitCode();
	return true;
}

void initRand()
{
	FILE *frand = fopen("/dev/random", "rt");
	int seed;
	int unused = 0;
	unused += fscanf(frand, "%c", ((char*)&seed));
	unused += fscanf(frand, "%c", ((char*)&seed)+1);
	unused += fscanf(frand, "%c", ((char*)&seed)+2);
	unused += fscanf(frand, "%c", ((char*)&seed)+3);
	fclose(frand);

	srand(seed);
}

bool stopSerial;
void serialSigInt(int )
{
	stopSerial = true;
}


#ifdef QT_NETWORK_LIB
#include <QHostInfo>
QString resolveDNSName(QString hostname)
{
	QHostInfo hInfo = QHostInfo::fromName(hostname);
	if (hInfo.addresses().count() > 0) {
		if (hInfo.addresses().count() == 0) {
			return QString();
		}
		return hInfo.addresses()[0].toString();
	}
	return QString();
}

QString resolveDNSReverse(QString ip)
{
	if (!ipnumOk(ip)) {
		orange(ip);
		exit(1);
	}
	QHostInfo hInfo = QHostInfo::fromName(ip);
	return hInfo.hostName();
}

QMap<QString, QString> reverseDNSCache;
QMutex reverseDNSCacheMutex;
#define REV_DNS_CACHE "/home/ovi/src/tracegraph/geoipdata/revdns.cache"
QString resolveDNSReverseCached(QString ip, bool silent)
{
	if (!ipnumOk(ip)) {
		orange(ip);
		exit(1);
	}

	reverseDNSCacheMutex.lock();
	if (reverseDNSCache.isEmpty()) {
		QFile file(REV_DNS_CACHE);
		if (file.open(QIODevice::ReadOnly)) {
			QDataStream in(&file);
			in >> reverseDNSCache;
		}
	}
	if (reverseDNSCache.contains(ip)) {
		QString dns = reverseDNSCache[ip];
		reverseDNSCacheMutex.unlock();
		return dns;
	}
	reverseDNSCacheMutex.unlock();

	if (!silent) {
		graySilentNobr("Querying reverse DNS for " + ip + "... ");
	}
	QHostInfo hInfo = QHostInfo::fromName(ip);
	if (!silent) {
		if (hInfo.hostName().isEmpty()) {
			redSilent("failed");
		} else {
			greenSilent("found " + hInfo.hostName());
		}
	}
	if (!hInfo.hostName().isEmpty()) {
		reverseDNSCacheMutex.lock();
		reverseDNSCache[ip] = hInfo.hostName();
		QFile file(REV_DNS_CACHE);
		if (file.open(QIODevice::WriteOnly)) {
			QDataStream out(&file);
			out << reverseDNSCache;
		}
		reverseDNSCacheMutex.unlock();
	}
	return hInfo.hostName();
}

#endif

#define CHECK(cond) if (!(cond)) return false;

// Returns true if ip is a valid IPv4 address in a.b.c.d format
bool ipnumOk(QString address)
{
	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	CHECK(bytes.count() == 4);

	unsigned a, b, c, d;
	bool ok;
	a = bytes[0].toInt(&ok); CHECK(ok); CHECK(a <= 255);
	b = bytes[1].toInt(&ok); CHECK(ok); CHECK(b <= 255);
	c = bytes[2].toInt(&ok); CHECK(ok); CHECK(c <= 255);
	d = bytes[3].toInt(&ok); CHECK(ok); CHECK(d <= 255);

	return true;
}

QString ipnumToString(ipnum ip)
{
	ipnum a, b, c, d;
	d = ip & 0xFFU;
	ip >>= 8;
	c = ip & 0xFFU;
	ip >>= 8;
	b = ip & 0xFFU;
	ip >>= 8;
	a = ip & 0xFFU;
	return QString::number(a) + '.' + QString::number(b) + '.' +
			QString::number(c) + '.' + QString::number(d);
}

ipnum stringToIpnum(QString address)
{
	ipnum ip;

	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	Q_ASSERT_FORCE(bytes.count() == 4);

	ipnum a, b, c, d;
	bool ok;
	a = bytes[0].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(a <= 255);
	b = bytes[1].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(b <= 255);
	c = bytes[2].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(c <= 255);
	d = bytes[3].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(d <= 255);

	ip = (a << 24) | (b << 16) | (c << 8) | d;
	return ip;
}

QString blockToString(Block &block)
{
	return ipnumToString(block.first) + " - " + ipnumToString(block.second);
}

#define CHECK(cond) if (!(cond)) return false;

// Returns true if prefix is a valid IPv4 route in a.b.c.d/mask format
// also accepts a.b.c/mask, a.b/mask, a/mask
bool prefixOk(QString prefix)
{

	QStringList tokens = prefix.split('/', QString::SkipEmptyParts);
	CHECK(tokens.count() == 2);

	QString address = tokens[0];
	QString mask = tokens[1];
	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	CHECK(bytes.count() >= 1);
	CHECK(bytes.count() <= 4);

	ipnum a, b, c, d, m;
	bool ok;
	a = b = c = d = 0;
	if (bytes.count() >= 1) {
		a = bytes[0].toInt(&ok); CHECK(ok); CHECK(a <= 255);
	}
	if (bytes.count() >= 2) {
		b = bytes[1].toInt(&ok); CHECK(ok); CHECK(b <= 255);
	}
	if (bytes.count() >= 3) {
		c = bytes[2].toInt(&ok); CHECK(ok); CHECK(c <= 255);
	}
	if (bytes.count() >= 4) {
		d = bytes[3].toInt(&ok); CHECK(ok); CHECK(d <= 255);
	}
	m = mask.toInt(&ok); CHECK(ok);
	CHECK(m <= 32);

	return true;
}

// Converts a.b.c.d/mask into a block (first address, last address)
Block prefixToBlock(QString prefix)
{
	/*
 // masks generation:
 ipnum ip = 0xffFFffFFU, m = 0;
 while (m <= 32) {
  printf("%uU,\n", ip);
  m++;
  ip >>= 1;
 }
 */
	static ipnum masks[33] = {4294967295U,
							  2147483647U,
							  1073741823U,
							  536870911U,
							  268435455U,
							  134217727U,
							  67108863U,
							  33554431U,
							  16777215U,
							  8388607U,
							  4194303U,
							  2097151U,
							  1048575U,
							  524287U,
							  262143U,
							  131071U,
							  65535U,
							  32767U,
							  16383U,
							  8191U,
							  4095U,
							  2047U,
							  1023U,
							  511U,
							  255U,
							  127U,
							  63U,
							  31U,
							  15U,
							  7U,
							  3U,
							  1U,
							  0U
							 };
	ipnum first, last;

	QStringList tokens = prefix.split('/', QString::SkipEmptyParts);
	Q_ASSERT_FORCE(tokens.count() == 2);

	QString address = tokens[0];
	QString mask = tokens[1];
	QStringList bytes = address.split('.', QString::SkipEmptyParts);
	Q_ASSERT_FORCE(bytes.count() >= 1);
	Q_ASSERT_FORCE(bytes.count() <= 4);

	ipnum a, b, c, d, m;
	bool ok;
	a = b = c = d = 0;
	if (bytes.count() >= 1) {
		a = bytes[0].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(a <= 255);
	}
	if (bytes.count() >= 2) {
		b = bytes[1].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(b <= 255);
	}
	if (bytes.count() >= 3) {
		c = bytes[2].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(c <= 255);
	}
	if (bytes.count() >= 4) {
		d = bytes[3].toInt(&ok); Q_ASSERT_FORCE(ok); Q_ASSERT_FORCE(d <= 255);
	}
	m = mask.toInt(&ok); Q_ASSERT_FORCE(ok);
	Q_ASSERT_FORCE(m <= 32);

	first = (a << 24) | (b << 16) | (c << 8) | d;
	first &= ~masks[m];
	last = first | masks[m];
	return Block (first, last);
}

QString downloadFile(QString webAddress)
{
	QString output;
	graySilentNobr("Downloading...");
	if (!runProcess("wget", QStringList() << "-q" << "-O" << "-" << webAddress, output, -1)) {
		orangeSilent("wget failed.");
		return QString();
	}
	greenSilent(" done.");
	return output;
}

QString getMyIp()
{
	QString myip;
	graySilentNobr("Finding my IP address...");
	if (!runProcess("wget", QStringList() << "-q" << "-O" << "-" << "http://www.interfete-web-club.com/demo/php/ip.php", myip, 30)) {
		orangeSilent("wget failed.");
		return QString();
	}
	if (!ipnumOk(myip)) {
		orangeSilent("received garbage: " + myip);
		return QString();
	}
	greenSilent(myip);
	return myip;
}

QString getMyIface()
{
	QString iface;
	int metric = 99999;
	graySilentNobr("Finding the default interface...");
	QString routes;
	if (!runProcess("sh", QStringList() << "-c" << "/sbin/route -n | grep \"^0.0.0.0\"", routes, 30)) {
		orangeSilent("failed.");
		return QString();
	}
	QStringList lines = routes.split('\n', QString::SkipEmptyParts);
	foreach (QString line, lines) {
		QString newIface = line.section(" ", -1, -1, QString::SectionSkipEmpty).trimmed();
		bool ok;
		int newMetric = line.section(" ", -4, -4, QString::SectionSkipEmpty).trimmed().toInt(&ok);
		Q_ASSERT_FORCE(ok);
		if (iface.isEmpty() || newMetric < metric) {
			iface = newIface;
			metric = newMetric;
		}
	}
	if (iface.isEmpty()) {
		orangeSilent("failed.");
		return QString();
	}
	greenSilent(iface);
	return iface;
}

QString getIfaceIp(QString iface)
{
	graySilentNobr("Finding IP for " + iface + "...");
	QString output;
	if (!runProcess("/sbin/ifconfig", QStringList() << iface, output, 30)) {
		orangeSilent("failed.");
		return QString();
	}
	QStringList lines = output.split('\n', QString::SkipEmptyParts);
	QString ip;
	foreach (QString line, lines) {
		ip = line.section("inet addr:", 1).section("Bcast", 0, 0).trimmed();
		if (!ipnumOk(ip))
			ip = "";
		else
			break;
	}
	if (ip.isEmpty()) {
		orangeSilent("failed.");
		return QString();
	}
	greenSilent(ip);
	return ip;
}

void makePrintable(QString &s)
{
	s.replace(QRegExp("[^\x20-\x7f]"), "?");
}

// int LevenshteinDistance(char s[1..m], char t[1..n])
int levenshteinDistance(QString s, QString t, Qt::CaseSensitivity cs)
{
	if (cs == Qt::CaseInsensitive) {
		s = s.toUpper();
		t = t.toUpper();
	}

	// d is a table with m+1 rows and n+1 columns
	int m = s.length();
	int n = t.length();

	// adapt for indexing from 1
	s.prepend(' ');
	t.prepend(' ');

	QVarLengthArray<int, 1024> d1(n + 1);
	QVarLengthArray<int, 1024> d2(n + 1);

	int i, j;

	for (j = 0; j <= n; j++)
		d1[j] = j;    // insertion
	d2[0] = 0;

	for (i = 1; i <= m; i++) {
		for (j = 1; j <= n; j++) {
			if (s[i] == t[j]) {
				d2[j] = d1[j-1];
			} else {
				int v;
				v = d1[j] + 1;      // deletion
				d2[j] = v;
				v = d2[j-1] + 1;    // insertion
				if (v < d2[j])
					d2[j] = v;
				v = d1[j-1] + 1;    // substitution
				if (v < d2[j])
					d2[j] = v;
			}
		}
		for (j = 1; j <= n; j++)
			d1[j] = d2[j];
	}

	return d1[n];
}

QString longestCommonSubstring(QString str1, QString str2, Qt::CaseSensitivity cs)
{
	if (str1.isEmpty() || str2.isEmpty()) {
		return 0;
	}

	QString str1Orig = str1;
	QString str2Orig = str2;

	if (cs == Qt::CaseInsensitive) {
		str1 = str1.toUpper();
		str2 = str2.toUpper();
	}

	QVarLengthArray<int, 1024> curr(str2.size());
	QVarLengthArray<int, 1024> prev(str2.size());

	int maxSubstr = 0;
	int endSubstr = 0;
	for (int i = 0; i < str1.size(); ++i) {
		for (int j = 0; j < str2.size(); ++j) {
			if (str1[i] != str2[j]) {
				curr[j] = 0;
			} else {
				if (i == 0 || j == 0) {
					curr[j] = 1;
				} else {
					curr[j] = 1 + prev[j-1];
				}
				if (maxSubstr < curr[j]) {
					maxSubstr = curr[j];
					endSubstr = j;
				}
			}
		}
		for (int j = 0; j < str2.size(); ++j)
			prev[j] = curr[j];
	}
	return str2Orig.mid(endSubstr - maxSubstr + 1, maxSubstr);
}
