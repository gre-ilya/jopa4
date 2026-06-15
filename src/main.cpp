#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MapPointsVisualizer"));
    QApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Visualize named WGS84 points on a map and connect them with lines."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("csv"),
        QStringLiteral("Optional CSV file of points: lon,lat,name"));
    parser.process(app);

    MainWindow w;
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty())
        w.loadCsvFile(args.first());

    w.show();
    return app.exec();
}
