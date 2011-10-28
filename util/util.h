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

#ifndef UTIL_H
#define UTIL_H

#include <signal.h>
#include <QtCore>
#include "debug.h"

bool saveFile(QString name, QString content);
bool readFile(QString name, QString &content, bool silent = false);
bool readTextArchive(QString name, QStringList &fileNames, QStringList &files, bool silent = false);
bool saveTextArchive(QString name, const QStringList &fileNames, const QStringList &files);
bool runProcess(QString name, QStringList args, bool silent = false);
bool runProcess(QString name, QStringList args, QString &output, int timeoutsec = -1, bool silent = false, int *exitCode = NULL);

bool randomLessThan(const QString &, const QString &);

template <typename T>
class qRandomLess
{
public:
    inline bool operator()(const T &, const T &) const
    {
	   return rand() < (RAND_MAX >> 1);
    }
};

template <typename ForwardIterator, typename T>
T qMinimum(ForwardIterator begin, ForwardIterator end, T defaultValue)
{
	if (begin == end) {
		return defaultValue;
	}
	ForwardIterator result = begin;
	while (begin != end) {
		if (*begin > *result)
			result = begin;
		++begin;
	}
	return *result;
}

template <typename Container, typename T>
T qMinimum(const Container &c, T defaultValue)
{
	return qMinimum(c.begin(), c.end(), defaultValue);
}

template <typename ForwardIterator, typename T>
T qMaximum(ForwardIterator begin, ForwardIterator end, T defaultValue)
{
	if (begin == end) {
		return defaultValue;
	}
	ForwardIterator result = begin;
	while (begin != end) {
		if (*begin < *result)
			result = begin;
		++begin;
	}
	return *result;
}

template <typename Container, typename T>
T qMaximum(const Container &c, T defaultValue)
{
	return qMaximum(c.begin(), c.end(), defaultValue);
}

class KeyVal {
public:
	int val;
	int key;
};

bool keyValHigherEqual(const KeyVal &s1, const KeyVal &s2);

void serialSigInt(int sig);

void printBacktrace();

template <typename JobList, typename MapFunctor>
void parallel(MapFunctor functor, JobList &jobs, int maxThreads, bool silent)
{
	int oldMaxThreads = QThreadPool::globalInstance()->maxThreadCount();
	QThreadPool::globalInstance()->setMaxThreadCount(maxThreads);
	if (!silent)
		gray(QString() + "Job count: " + QString::number(jobs.count()) + " Thread pool size: " + QString::number(maxThreads));
	graySilent("[" + QString("=").repeated(jobs.count()) + "]");
	graySilentNobr("[");
	(void) signal(SIGINT, serialSigInt);
	QtConcurrent::blockingMap(jobs.begin(), jobs.end(), functor);
	graySilent("]");
	(void) signal(SIGINT, SIG_DFL);
	QThreadPool::globalInstance()->setMaxThreadCount(oldMaxThreads);
}

extern bool stopSerial;

template <typename Iterator, typename MapFunctor>
void serialHelper(MapFunctor functor, Iterator begin, Iterator end)
{
	Iterator i;
	for (i = begin; i != end && !stopSerial; ++i)
		functor(*i);
	if (stopSerial && i != end) {
		orange("Interrupted");
	}
}

template <typename JobList, typename MapFunctor>
void serial(MapFunctor functor, JobList &jobs, bool silent)
{
	if (!silent)
		gray(QString() + "Job count: " + QString::number(jobs.count()));
	stopSerial = false;
	(void) signal(SIGINT, serialSigInt);
	graySilent("[" + QString("=").repeated(jobs.count()) + "]");
	graySilentNobr("[");
	serialHelper(functor, jobs.begin(), jobs.end());
	graySilent("]");
	(void) signal(SIGINT, SIG_DFL);
}

void initRand();

#ifdef QT_NETWORK_LIB

QString resolveDNSName(QString hostname);
QString resolveDNSReverse(QString ip);
QString resolveDNSReverseCached(QString ip, bool silent = true);

#endif

typedef unsigned ipnum;
typedef QPair<ipnum, ipnum> Block;

// Returns true if ip is a valid IPv4 address in a.b.c.d format
bool ipnumOk(QString address);

QString ipnumToString(ipnum ip);
ipnum stringToIpnum(QString s);

QString blockToString(Block &block);

// Converts a.b.c.d/mask into a block (first address, last address)
Block prefixToBlock(QString prefix);

// Returns true if prefix is a valid IPv4 route in a.b.c.d/mask format
bool prefixOk(QString prefix);

QString getMyIp();
QString getMyIface();
QString getIfaceIp(QString iface);
QString downloadFile(QString webAddress);

// Replaces non-printable ascii chars in s with '?'
void makePrintable(QString &s);

int levenshteinDistance(QString s, QString t, Qt::CaseSensitivity cs = Qt::CaseSensitive);
QString longestCommonSubstring(QString str1, QString str2, Qt::CaseSensitivity cs = Qt::CaseSensitive);


#endif // UTIL_H
