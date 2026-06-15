#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>

#include "GeoTypes.h"

class MapWidget;
class QTableWidget;
class QCheckBox;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void loadCsvFile(const QString& path);
    // Point the map at a folder of pre-downloaded tiles (z/x/y.png) and switch
    // to offline mode so no network access is attempted.
    void setTileDirectory(const QString& dir);

private slots:
    void chooseTileDirectory();
    void onTableChanged();
    void addPointRow();
    void removeSelectedRows();
    void openCsv();
    void saveCsv();
    void exportImage();
    void onMapClicked(double lon, double lat);
    void updateStatus();

private:
    void buildUi();
    void setRows(const QList<GeoPoint>& points);
    QList<GeoPoint> pointsFromTable() const;
    void syncMapFromTable();
    void appendRow(const GeoPoint& p);

    MapWidget* m_map = nullptr;
    QTableWidget* m_table = nullptr;
    QCheckBox* m_chkLines = nullptr;
    QCheckBox* m_chkLabels = nullptr;
    QCheckBox* m_chkNetwork = nullptr;
    QCheckBox* m_chkClickAdd = nullptr;
    QLabel* m_status = nullptr;

    bool m_populating = false;
};

#endif // MAINWINDOW_H
