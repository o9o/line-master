#include <QtCore>

#include "../util/util.h"

void autoCorrelation(QVector<double> &data, int lag, QVector<double> &acf, double &bound)
{
	if (lag <= 0)
		return;

	acf.resize(lag + 1);

	double mean = 0;
	for (int i = 0; i < data.count(); i++)
		mean += data[i];
	mean /= data.count();

	for (int h = 0; h <= lag; h++) {
		acf[h] = 0;
		for (int i = 0; i < data.count() - h; i++)
			acf[h] += (data[i] - mean) * (data[i+h] - mean);
		acf[h] /= data.count();
	}

	for (int h = 1; h <= lag; h++)
		acf[h] = acf[h] / acf[0];
	acf[0] = 1;

	bound = 1.96 / sqrt(data.count());
}

int main(int argc, char *argv[])
{
	argc--, argv++;

	if (argc != 4) {
		qDebug() << "Wrong args; expecting inputFile lag outputFile xlabel";
	}

	QString inputFile = argv[0];
	argc--, argv++;
	int lag = QString(argv[0]).toInt();
	argc--, argv++;
	QString outputFile = argv[0];
	argc--, argv++;
	QString xlabel = argv[0];
	argc--, argv++;

	QVector<double> data;
	{
		QStringList tokens;
		{
			QString inputText;
			QFile file(inputFile);
			if (file.open(QIODevice::ReadOnly)) {
				QTextStream t(&file);
				inputText = t.readLine();
				// Close the file
				file.close();
			}
			tokens = inputText.split(' ', QString::SkipEmptyParts);
		}
		data = QVector<double>(tokens.count());
		for (int i = 0; i < tokens.count(); i++) {
			data[i] = tokens[i].toDouble();
		}
	}

	lag = qMin(lag, data.count()-1);

	QVector<double> acf;
	double bound;
	autoCorrelation(data, lag, acf, bound);

	QStringList lines;
	QString line;
	line = QString("x = [");
	foreach (double x, acf) {
		line += QString("%1 ").arg(x);
	}
	line += QString("];");
	lines << line;

	lines << QString("bound = %1;").arg(bound);
	lines << QString("figure;");
	lines << QString("stem(0:length(x)-1, x);");
	lines << QString("hold on;");
	lines << QString("line([0 length(x) + 1], [bound bound], 'Color', 'r', 'LineStyle', '-');");
	lines << QString("line([0 length(x) + 1], [-bound -bound], 'Color', 'r', 'LineStyle', '-');");
	lines << QString("xlabel('%1');").arg(xlabel);
	lines << QString("ylabel('Sample autocorrelation function');");
	lines << QString("print('%1.png', '-dpng', '-FLiberationSansBold:16');").arg(outputFile);

	FILE *f = fopen(outputFile.toLatin1().data(), "wt");
	fprintf(f, "%s\n", lines.join("\n").toAscii().data());
	fclose(f);

	return 0;
}
