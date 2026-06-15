#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

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
    QCommandLineOption tilesOpt(
        QStringList{QStringLiteral("t"), QStringLiteral("tiles")},
        QStringLiteral("Folder of pre-downloaded offline tiles (z/x/y.png)."),
        QStringLiteral("dir"));
    parser.addOption(tilesOpt);
    parser.process(app);

    MainWindow w;

    // Offline tiles: --tiles wins over the MAP_TILES_DIR environment variable.
    QString tilesDir = parser.value(tilesOpt);
    if (tilesDir.isEmpty())
        tilesDir = qEnvironmentVariable("MAP_TILES_DIR");
    if (!tilesDir.isEmpty())
        w.setTileDirectory(tilesDir);

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty())
        w.loadCsvFile(args.first());

    w.show();
    return app.exec();
}
