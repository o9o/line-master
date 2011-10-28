#include <QtCore>
#include <limits.h>

#define TEST 0
#define PROB0 0.5

int main(int argc, char *argv[])
{
	argc--, argv++;

#if !TEST
	QString inputFile = argv[0];
	argc--, argv++;
#else
	srand(time(NULL));
#endif

	double nLow = 0;
	double nHigh = 0;
	double nRuns = 1;
	int nSkip = 10000;

	printf("Skipping:      %d values\n", nSkip);

	int xPrev = -1;
	// read values, count
#if !TEST
	//FILE *f = fopen(inputFile.toLatin1().data(), "rt");
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
#endif
	while (1) {
		int x;
#if !TEST
		if (values.isEmpty())
			break;
		x = values.at(0);
		values.remove(0);
#else
		if (nLow + nHigh > 30000)
			break;
		x = rand() > RAND_MAX * PROB0; // (1 - prob0) loss
#endif
		if (x > 1) {
			printf("Warning: event with value = %d\n", x);
			//exit(1);
			x = 1;
		}
		if (x == 1) {
			nHigh += 1.0;
		}
		if (x == 0) {
			nLow += 1.0;
		}
		if (xPrev >= 0) {
			if (x != xPrev) {
				nRuns += 1.0;
			}
		}
		xPrev = x;
	}

	double expectedRuns = 2.0 * nLow * nHigh / (nLow + nHigh) + 1;
	double variance = (2.0*nLow*nHigh) * (2.0*nLow*nHigh - (nLow+nHigh)) / (((nLow+nHigh)*(nLow+nHigh))*((nLow+nHigh)-1));

	double z = (nRuns - expectedRuns) / sqrt(variance);

	printf("Zeroes:        %d\n", (int)nLow);
	printf("Ones:          %d\n", (int)nHigh);
	printf("Runs:          %d\n", (int)nRuns);
	printf("Expected runs: %f\n", expectedRuns);
	printf("Variance:      %f\n", variance);
	printf("Z:             %f\n", z);

	if (z < -2.580 || z > 2.580) {
		printf("Strong evidence (1%%) that the pattern is not random: ");
		printf("%s\n", z < -2.580 ? "Too few runs." : "Too many runs.");
	} else if (z < -1.960 || z > 1.960) {
		printf("Strong evidence (5%%) that the pattern is not random: ");
		printf("%s\n", z < -1.960 ? "Too few runs." : "Too many runs.");
	} else if (z < -1.645 || z > 1.645) {
		printf("Strong evidence (10%%) that the pattern is not random: ");
		printf("%s\n", z < -1.645 ? "Too few runs." : "Too many runs.");
	} else {
		printf("Insufficient evidence to suggest that the pattern is not random.\n");
	}


	return 0;
}
