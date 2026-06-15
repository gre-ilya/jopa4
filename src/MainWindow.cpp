#include "MainWindow.h"
#include "MapWidget.h"
#include "TileManager.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QImage>
#include <QModelIndex>
#include <QItemSelectionModel>

#include <algorithm>
#include <functional>

namespace {

// Parses a CSV line of the form  lon,lat,name  (extra commas fold into name).
// Returns false for blank/comment/header lines that should be skipped.
bool parseCsvLine(const QString& rawLine, GeoPoint& out) {
    const QString line = rawLine.trimmed();
    if (line.isEmpty() || line.startsWith('#'))
        return false;

    const QStringList parts = line.split(',');
    if (parts.size() < 2)
        return false;

    bool okLon = false, okLat = false;
    const double lon = parts.at(0).trimmed().toDouble(&okLon);
    const double lat = parts.at(1).trimmed().toDouble(&okLat);
    if (!okLon || !okLat)
        return false;   // likely a header line such as "lon,lat,name"

    QString name;
    if (parts.size() >= 3)
        name = QStringList(parts.mid(2)).join(',').trimmed();

    out = GeoPoint(lon, lat, name);
    return true;
}

const char* kSampleCsv =
    "# lon,lat,name  (WGS84 degrees)\n"
    "37.6173,55.7558,Moscow\n"
    "30.3158,59.9391,Saint Petersburg\n"
    "49.1221,55.7887,Kazan\n"
    "44.0059,56.3269,Nizhny Novgorod\n";

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();

    // Seed with sample data so the window is not empty on first launch.
    QList<GeoPoint> sample;
    QByteArray sampleBytes(kSampleCsv);
    QTextStream ts(&sampleBytes, QIODevice::ReadOnly);
    QString line;
    GeoPoint p;
    while (ts.readLineInto(&line))
        if (parseCsvLine(line, p))
            sample.append(p);
    setRows(sample);
    syncMapFromTable();
    m_map->fitToPoints();
    updateStatus();
}

void MainWindow::buildUi() {
    setWindowTitle(tr("Map Points Visualizer"));
    resize(1100, 720);

    m_map = new MapWidget(this);

    // ---- Left panel: points table + editing buttons ----
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Longitude"), tr("Latitude"), tr("Name")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(m_table, &QTableWidget::itemChanged, this, &MainWindow::onTableChanged);

    auto* btnAdd = new QPushButton(tr("Add point"), this);
    auto* btnDel = new QPushButton(tr("Remove selected"), this);
    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::addPointRow);
    connect(btnDel, &QPushButton::clicked, this, &MainWindow::removeSelectedRows);

    auto* rowBtns = new QHBoxLayout;
    rowBtns->addWidget(btnAdd);
    rowBtns->addWidget(btnDel);

    m_chkLines = new QCheckBox(tr("Connecting lines"), this);
    m_chkLines->setChecked(true);
    m_chkLabels = new QCheckBox(tr("Labels"), this);
    m_chkLabels->setChecked(true);
    m_chkNetwork = new QCheckBox(tr("Download map tiles (online)"), this);
    m_chkNetwork->setChecked(true);
    m_chkClickAdd = new QCheckBox(tr("Add point on map click"), this);
    m_chkClickAdd->setChecked(false);

    connect(m_chkLines, &QCheckBox::toggled, m_map, &MapWidget::setShowConnectingLines);
    connect(m_chkLabels, &QCheckBox::toggled, m_map, &MapWidget::setShowLabels);
    connect(m_chkNetwork, &QCheckBox::toggled, this, [this](bool on) {
        m_map->tileManager()->setNetworkEnabled(on);
        m_map->update();
        updateStatus();
    });

    auto* left = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(new QLabel(tr("Points (WGS84):"), this));
    leftLayout->addWidget(m_table, 1);
    leftLayout->addLayout(rowBtns);
    leftLayout->addWidget(m_chkLines);
    leftLayout->addWidget(m_chkLabels);
    leftLayout->addWidget(m_chkNetwork);
    leftLayout->addWidget(m_chkClickAdd);

    auto* splitter = new QSplitter(this);
    splitter->addWidget(left);
    splitter->addWidget(m_map);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 780});
    setCentralWidget(splitter);

    // ---- Toolbar ----
    auto* tb = addToolBar(tr("Main"));
    tb->addAction(tr("Open CSV"), this, &MainWindow::openCsv);
    tb->addAction(tr("Save CSV"), this, &MainWindow::saveCsv);
    tb->addSeparator();
    tb->addAction(tr("Fit"), m_map, &MapWidget::fitToPoints);
    tb->addAction(tr("Zoom +"), m_map, &MapWidget::zoomIn);
    tb->addAction(tr("Zoom -"), m_map, &MapWidget::zoomOut);
    tb->addSeparator();
    tb->addAction(tr("Offline tiles folder..."), this, &MainWindow::chooseTileDirectory);
    tb->addAction(tr("Export image..."), this, &MainWindow::exportImage);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status);

    connect(m_map, &MapWidget::mapClicked, this, &MainWindow::onMapClicked);
    connect(m_map, &MapWidget::viewChanged, this, &MainWindow::updateStatus);
}

void MainWindow::appendRow(const GeoPoint& p) {
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto* lonItem = new QTableWidgetItem(QString::number(p.lon, 'f', 6));
    auto* latItem = new QTableWidgetItem(QString::number(p.lat, 'f', 6));
    auto* nameItem = new QTableWidgetItem(p.name);
    m_table->setItem(row, 0, lonItem);
    m_table->setItem(row, 1, latItem);
    m_table->setItem(row, 2, nameItem);
}

void MainWindow::setRows(const QList<GeoPoint>& points) {
    m_populating = true;
    m_table->setRowCount(0);
    for (const GeoPoint& p : points)
        appendRow(p);
    m_populating = false;
}

QList<GeoPoint> MainWindow::pointsFromTable() const {
    QList<GeoPoint> result;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* lonItem = m_table->item(row, 0);
        const QTableWidgetItem* latItem = m_table->item(row, 1);
        const QTableWidgetItem* nameItem = m_table->item(row, 2);
        if (!lonItem || !latItem)
            continue;
        bool okLon = false, okLat = false;
        const double lon = lonItem->text().trimmed().toDouble(&okLon);
        const double lat = latItem->text().trimmed().toDouble(&okLat);
        if (!okLon || !okLat)
            continue;
        result.append(GeoPoint(lon, lat, nameItem ? nameItem->text() : QString()));
    }
    return result;
}

void MainWindow::syncMapFromTable() {
    m_map->setPoints(pointsFromTable());
}

void MainWindow::onTableChanged() {
    if (m_populating)
        return;
    syncMapFromTable();
}

void MainWindow::addPointRow() {
    appendRow(GeoPoint(m_map->centerLon(), m_map->centerLat(),
                       tr("Point %1").arg(m_table->rowCount() + 1)));
    syncMapFromTable();
}

void MainWindow::removeSelectedRows() {
    QList<int> rows;
    for (const QModelIndex& idx : m_table->selectionModel()->selectedRows())
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    m_populating = true;
    for (int r : rows)
        m_table->removeRow(r);
    m_populating = false;
    syncMapFromTable();
}

void MainWindow::loadCsvFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open CSV"),
                             tr("Cannot open file:\n%1").arg(path));
        return;
    }
    QTextStream ts(&f);
    QList<GeoPoint> points;
    QString line;
    GeoPoint p;
    while (ts.readLineInto(&line))
        if (parseCsvLine(line, p))
            points.append(p);

    setRows(points);
    syncMapFromTable();
    m_map->fitToPoints();
    updateStatus();
}

void MainWindow::openCsv() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open points CSV"), QString(),
        tr("CSV files (*.csv *.txt);;All files (*)"));
    if (!path.isEmpty())
        loadCsvFile(path);
}

void MainWindow::saveCsv() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save points CSV"), QStringLiteral("points.csv"),
        tr("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save CSV"),
                             tr("Cannot write file:\n%1").arg(path));
        return;
    }
    QTextStream ts(&f);
    ts << "# lon,lat,name (WGS84 degrees)\n";
    for (const GeoPoint& p : pointsFromTable())
        ts << QString::number(p.lon, 'f', 6) << ','
           << QString::number(p.lat, 'f', 6) << ','
           << p.name << '\n';
}

void MainWindow::exportImage() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export map image"), QStringLiteral("map.png"),
        tr("PNG image (*.png);;JPEG image (*.jpg)"));
    if (path.isEmpty())
        return;

    const QImage img = m_map->renderToImage(m_map->size());
    if (!img.save(path)) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Failed to save image:\n%1").arg(path));
    }
}

void MainWindow::setTileDirectory(const QString& dir) {
    if (dir.isEmpty())
        return;
    m_map->tileManager()->setTileDirectory(dir);
    // Pre-downloaded tiles imply offline use; stop hitting the network.
    m_chkNetwork->setChecked(false);
    m_map->tileManager()->setNetworkEnabled(false);
    m_map->update();
    updateStatus();
}

void MainWindow::chooseTileDirectory() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select offline tiles folder (contains z/x/y.png)"),
        m_map->tileManager()->tileDirectory());
    if (!dir.isEmpty())
        setTileDirectory(dir);
}

void MainWindow::onMapClicked(double lon, double lat) {
    if (m_chkClickAdd->isChecked()) {
        appendRow(GeoPoint(lon, lat, tr("Point %1").arg(m_table->rowCount() + 1)));
        syncMapFromTable();
    }
    updateStatus();
}

void MainWindow::updateStatus() {
    const QString net = m_map->tileManager()->networkEnabled()
                            ? tr("online tiles")
                            : tr("offline grid");
    m_status->setText(tr("center: %1, %2    zoom: %3    [%4]    points: %5")
                          .arg(m_map->centerLat(), 0, 'f', 5)
                          .arg(m_map->centerLon(), 0, 'f', 5)
                          .arg(m_map->zoom())
                          .arg(net)
                          .arg(m_table->rowCount()));
}
