#include <QtCore>
#include <limits.h>

// PDF of a geometric distribution, i.e. the probability of having x failures before the first success,
// when the probability of success is p. x can be 0,1,2,3,...
double geopdf(int x, double p)
{
	double result = p;

	for (int k = 0; k < x; k++)
		result *= 1 - p;

	return result;
}

// CDF of a geometric distribution, i.e. the probability of having k <= x failures before the first success,
// when the probability of success is p. x can be 0,1,2,3,...
double geocdf(int x, double p)
{
	double result = 0;

	for (int k = 0; k <= x; k++) {
		result += geopdf(k, p);
	}

	return result;
}

// Takes a chi-square statistic (chi2) and the number of degrees of freedom (dof) to calculate a p-value
// by comparing the value of the statistic to a chi-squared distribution.
// The p-value is the probability of obtaining a test statistic at least as extreme as the one that was
// actually observed, assuming that the null hypothesis (in this case, good fit) is true.
// The fit can be rejected if the p-value is below some significance level, e.g. 1% or 5%.
double chi2pvalue(double chi2, double dof)
{
	// octave -q --eval '1 - chi2cdf(2.304, 2)'
	QProcess octave;
	octave.start("octave", QStringList() << "-q" << "--eval" << QString("1 - chi2cdf(%1, %2)").arg(chi2).arg(dof));
	if (!octave.waitForStarted()) {
		qDebug() << QString("Could not start octave");
		return 0;
	}

	if (!octave.waitForFinished(-1)) {
		qDebug() << QString("Could not wait for octave");
		return 0;
	} else {
		QString output = octave.readAll();
		if (!output.contains("ans =")) {
			qDebug() << QString("Could not understand octave output:");
			qDebug() << output;
			return 0;
		} else {
			output = output.replace("ans =", "").trimmed();
			bool ok;
			double result;
			result = output.toDouble(&ok);
			if (!ok) {
				qDebug() << QString("Could not understand octave output:");
				qDebug() << output;
				return 0;
			}
			return result;
		}
	}
}

// Chi square goodness of fit test to geometric distribution
// Input:
// * p = probability of occurence of an event
// * histogram[v] = number of occurences of v failures before the first success
// We will group together adjacent bins if the number of elements is < 5.
// 0 1 4 5 6 8 10 12 20 25 37
// 0 1 4 5 6 8 10 12 20 (25 37)
// 0 1 4 5 6 (8 10 12) 20 (25 37)
void chi2geom(double p, QHash<int, int> histogram, QHash<int, double> &geomHistogram)
{
	// list of (group of bin values, total group frequncy)
	QList<QPair<QList<int>, int> > groupedHistogram;

	QList<int> keys = histogram.keys();
	qSort(keys);
	foreach (int key, keys) {
		QPair<QList<int>, int> histElem = QPair<QList<int>, int>(QList<int>() << key, histogram[key]);
		groupedHistogram << histElem;
	}

	// run over the data from right to left and group together bins if frequency is below 5
	for (int i = groupedHistogram.count() - 1; i > 0; i--) {
		QPair<QList<int>, int> currentElem = groupedHistogram[i];
		if (currentElem.second < 5) {
			QPair<QList<int>, int> leftElem = groupedHistogram[i-1];
			leftElem.first << currentElem.first;
			leftElem.second += currentElem.second;
			groupedHistogram[i-1] = leftElem;
			groupedHistogram.removeAt(i);
		}
	}

	printf("Running chi-square goodness of fit test to geom. distribution with p = %f\n", p);
	printf("Bin count = %d\n", groupedHistogram.count());

	double totalEvents = 0;
	foreach (int c, histogram.values())
		totalEvents += c;
	printf("Number of events (inter-drop intervals) = %f\n", totalEvents);

	QVector<double> histogramExpected(groupedHistogram.count());
	for (int i = 0; i < groupedHistogram.count(); i++) {
		QPair<QList<int>, int> currentElem = groupedHistogram[i];

		histogramExpected[i] = 0.0;
		foreach (int x, currentElem.first) {
			// histogramExpected[i] += totalEvents * (geocdf(x + binWidth/2, p) - geocdf(x - binWidth/2, p));
			double y = totalEvents * geopdf(x, p);
			geomHistogram[x] = y;
			histogramExpected[i] += y;
		}
	}

	// compute chi2
	double chi2 = 0;
	for (int i = 0; i < groupedHistogram.count(); i++) {
		double actual = groupedHistogram[i].second;
		double expected = histogramExpected[i];
		// prevent division by zero
		if (expected > 1.0e-100)
			chi2 += (actual - expected) * (actual - expected) / expected;
	}
	printf("chi2 = %f\n", chi2);

	// degrees of freedom
	double dof = groupedHistogram.count() - 1 - 1;
	printf("dof = %f\n", dof);

	double pvalue = chi2pvalue(chi2, dof);

	printf("pvalue = %f\n", pvalue);

	if (pvalue > 0.05) {
		printf("With a 5%% confidence, cannot reject fit (pvalue > 0.05)\n");
	} else {
		printf("With a 5%% confidence, reject fit (pvalue <= 0.05)\n");
	}
}

#define TEST 0
#define PROB0 (1-0.119227)

int main(int argc, char *argv[])
{
	argc--, argv++;

#if !TEST
	QString inputFile = argv[0];
	argc--, argv++;
#else
	srand(time(NULL));
#endif

	QString outputFile = argv[0];
	argc--, argv++;
	QString xlabel = argv[0];
	argc--, argv++;
	QString ylabel = argv[0];
	argc--, argv++;

	int interval = 0;

	int nSkip = 10000;
	printf("Skipping:      %d values\n", nSkip);

	QHash<int, int> histogram;
	QList<int> interDropIntervals;

	int nZeros = 0;
	int nOnes = 0;
	QList<double> evolution;
	QList<int> last100;
	QList<double> evolution100;
	QList<int> last1000;
	QList<double> evolution1000;

	// read values, count
#if !TEST
	QFile file(inputFile);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << QString("Failed to open file %1").arg(inputFile);
	}
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_0);

	QVector<quint8> values = QVector<quint8>();
	in >> values;
	if (values.count() < 2 * nSkip) {
		qDebug() << "cannot skip so much data";
		return 1;
	}
	values.remove(0, nSkip);
	values.remove(values.count() - nSkip, nSkip);
#else
	FILE *f;
#endif

	while (1) {
		int x;
#if !TEST
		if (values.isEmpty())
			break;
		x = values.at(0);
		values.remove(0);
#else
		if (nZeros + nOnes > 30000)
			break;
		x = rand() > RAND_MAX * PROB0; // (1 - prob0) loss
#endif
		if (x > 1) {
			printf("Warning: event with value = %d\n", x);
			//exit(1);
			x = 1;
		}
		if (x == 1) {
			if (interval >= 0) {
				histogram[interval]++;
				interDropIntervals << interval;
			}
			interval = 0;
			nOnes++;
		}
		if (x == 0) {
			interval++;
			nZeros++;
		}
		evolution << nZeros / (double) (nZeros + nOnes);
		last100 << x;
		if (last100.count() > 100) {
			last100.takeFirst();
			evolution100 << last100.count(0) / 100.0;
		}
		last1000 << x;
		if (last1000.count() > 1000) {
			last1000.takeFirst();
			evolution1000 << last1000.count(0) / 1000.0;
		}
	}
#if !TEST
	FILE *f = fopen((inputFile + ".intervals").toLatin1().data(), "wt");
	foreach (int x, interDropIntervals) {
		fprintf(f, "%d ", x);
	}
	fclose(f);
#endif

	// chi square goodness of fit test to geometric distribution
	double p = 1 - nZeros / (double)(nZeros + nOnes); // prob of drop
	QHash<int, double> geomHistogram;
	chi2geom(p, histogram, geomHistogram);

	QList<int> bins = histogram.keys();

	// output plot script
	QStringList lines;
	QString line;
	line = QString("binCenters = [");
	foreach (int x, bins) {
		line += QString("%1 ").arg(x);
	}
	line += QString("];");
	lines << line;

	line = QString("bins = [");
	foreach (int x, bins) {
		line += QString("%1 ").arg(histogram[x]);
	}
	line += QString("];");
	lines << line;

	line = QString("binsExpected = [");
	foreach (double x, bins) {
		line += QString("%1 ").arg(geomHistogram[x]);
	}
	line += QString("];");
	lines << line;

	line = QString("evolution = [");
	foreach (double x, evolution) {
		line += QString("%1 ").arg(x);
	}
	line += QString("];");
	lines << line;

	line = QString("evolution100 = [");
	foreach (double x, evolution100) {
		line += QString("%1 ").arg(x);
	}
	line += QString("];");
	lines << line;

	line = QString("evolution1000 = [");
	foreach (double x, evolution1000) {
		line += QString("%1 ").arg(x);
	}
	line += QString("];");
	lines << line;

	lines << QString("figure;");
	lines << QString("hold on;");
	//lines << QString("bar(binCenters, bins);");
	lines << QString("plot(binCenters, bins, 'bx-');");
	lines << QString("plot(binCenters, binsExpected, 'r+-');");
	lines << QString("hold off;");
	lines << QString("xlabel('%1');").arg(xlabel);
	lines << QString("ylabel('%1');").arg(ylabel);
	lines << QString("legend('Actual distribution', 'Geometric distribution (p = %1)');").arg(p);
	lines << QString("print('%1.png', '-dpng', '-FLiberationSansBold:16');").arg(outputFile);

	lines << QString("plot(evolution);");
	lines << QString("ylim([0 1]);");
	lines << QString("xlabel('Lag (1 point = 1 packet)');");
	lines << QString("ylabel('Transmission rate (overall)');");
	lines << QString("print('%1-evolution.png', '-dpng', '-FLiberationSansBold:16');").arg(outputFile);

	lines << QString("plot(evolution100);");
	lines << QString("ylim([0 1]);");
	lines << QString("xlabel('Lag (1 point = 1 packet)');");
	lines << QString("ylabel('Transmission rate (last 100 packets)');");
	lines << QString("print('%1-evolution100.png', '-dpng', '-FLiberationSansBold:16');").arg(outputFile);

	lines << QString("plot(evolution1000);");
	lines << QString("ylim([0 1]);");
	lines << QString("xlabel('Lag (1 point = 1 packet)');");
	lines << QString("ylabel('Transmission rate (last 1000 packets)');");
	lines << QString("print('%1-evolution1000.png', '-dpng', '-FLiberationSansBold:16');").arg(outputFile);

	f = fopen(outputFile.toLatin1().data(), "wt");
	fprintf(f, "%s\n", lines.join("\n").toAscii().data());
	fclose(f);

	return 0;
}
