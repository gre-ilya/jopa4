#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QList>
#include <QVector>
#include <QPointF>

#include "GeoTypes.h"

class TileManager;

// A slippy-map view that renders raster tiles as a background and draws the
// supplied named points (connected, in order, by a polyline) plus an arbitrary
// list of geographic annotations on top.
//
// Pan with left-drag, zoom with the mouse wheel. The widget emits mapClicked()
// for plain clicks, which downstream code can use to add new annotations.
class MapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MapWidget(QWidget* parent = nullptr);

    void setPoints(const QList<GeoPoint>& points);
    const QList<GeoPoint>& points() const { return m_points; }

    void setAnnotations(const QVector<Annotation>& annotations);
    void addAnnotation(const Annotation& a);
    void clearAnnotations();
    const QVector<Annotation>& annotations() const { return m_annotations; }

    void setShowConnectingLines(bool on);
    void setShowLabels(bool on);

    TileManager* tileManager() const { return m_tiles; }

    int zoom() const { return m_zoom; }
    double centerLon() const { return m_centerLon; }
    double centerLat() const { return m_centerLat; }

    // Renders the current view (tiles + overlays) into an image of the given
    // size for exporting to a file.
    QImage renderToImage(const QSize& size);

public slots:
    void setCenter(double lon, double lat);
    void setZoom(int z);
    void zoomIn();
    void zoomOut();
    // Pick center and zoom so that all current points are visible.
    void fitToPoints();

signals:
    void mapClicked(double lon, double lat);
    void viewChanged();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    // Coordinate transforms for the current view state and widget size.
    QPointF originWorld() const;                 // top-left world pixel
    QPointF geoToScreen(double lon, double lat) const;
    void screenToGeo(const QPointF& p, double& lon, double& lat) const;
    void setCenterFromWorld(double worldX, double worldY);

    void drawTiles(QPainter& p, const QSize& viewport, const QPointF& origin);
    void drawOverlay(QPainter& p, const QPointF& origin);
    QPointF geoToScreenWithOrigin(double lon, double lat, const QPointF& origin) const;

    TileManager* m_tiles;
    QList<GeoPoint> m_points;
    QVector<Annotation> m_annotations;

    double m_centerLon = 0.0;
    double m_centerLat = 20.0;
    int m_zoom = 2;

    bool m_showLines = true;
    bool m_showLabels = true;

    bool m_dragging = false;
    bool m_dragMoved = false;
    QPoint m_lastMousePos;
};

#endif // MAPWIDGET_H
