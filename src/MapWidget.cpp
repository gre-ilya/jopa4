#include "MapWidget.h"
#include "TileManager.h"
#include "MercatorProjection.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QPolygonF>
#include <QImage>
#include <algorithm>

using namespace geo;

static const int kMinZoom = 0;
static const int kMaxZoom = 19;

MapWidget::MapWidget(QWidget* parent)
    : QWidget(parent)
    , m_tiles(new TileManager(this))
{
    setMinimumSize(480, 360);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setFocusPolicy(Qt::WheelFocus);

    connect(m_tiles, &TileManager::tileReady, this,
            [this](int, int, int) { update(); });
}

void MapWidget::setPoints(const QList<GeoPoint>& points) {
    m_points = points;
    update();
}

void MapWidget::setAnnotations(const QVector<Annotation>& annotations) {
    m_annotations = annotations;
    update();
}

void MapWidget::addAnnotation(const Annotation& a) {
    m_annotations.append(a);
    update();
}

void MapWidget::clearAnnotations() {
    m_annotations.clear();
    update();
}

void MapWidget::setShowConnectingLines(bool on) { m_showLines = on; update(); }
void MapWidget::setShowLabels(bool on) { m_showLabels = on; update(); }

void MapWidget::setCenter(double lon, double lat) {
    m_centerLon = lon;
    m_centerLat = clampLatitude(lat);
    update();
    emit viewChanged();
}

void MapWidget::setZoom(int z) {
    z = std::max(kMinZoom, std::min(kMaxZoom, z));
    if (z == m_zoom)
        return;
    m_zoom = z;
    update();
    emit viewChanged();
}

void MapWidget::zoomIn() { setZoom(m_zoom + 1); }
void MapWidget::zoomOut() { setZoom(m_zoom - 1); }

void MapWidget::fitToPoints() {
    if (m_points.isEmpty())
        return;

    double minLon = m_points.first().lon, maxLon = minLon;
    double minLat = m_points.first().lat, maxLat = minLat;
    for (const GeoPoint& p : m_points) {
        minLon = std::min(minLon, p.lon);
        maxLon = std::max(maxLon, p.lon);
        minLat = std::min(minLat, p.lat);
        maxLat = std::max(maxLat, p.lat);
    }

    const double w = std::max(1, width());
    const double h = std::max(1, height());

    if (m_points.size() == 1) {
        m_zoom = 12;
    } else {
        // Add a small geographic margin so points are not glued to the edges.
        const double dLon = std::max(0.0005, (maxLon - minLon) * 0.15);
        const double dLat = std::max(0.0005, (maxLat - minLat) * 0.15);
        m_zoom = zoomForBounds(minLon - dLon, minLat - dLat,
                               maxLon + dLon, maxLat + dLat, w, h,
                               kMinZoom, kMaxZoom);
    }

    m_centerLon = (minLon + maxLon) / 2.0;
    m_centerLat = clampLatitude((minLat + maxLat) / 2.0);
    update();
    emit viewChanged();
}

QPointF MapWidget::originWorld() const {
    const double cx = lonToWorldX(m_centerLon, m_zoom);
    const double cy = latToWorldY(m_centerLat, m_zoom);
    return QPointF(cx - width() / 2.0, cy - height() / 2.0);
}

QPointF MapWidget::geoToScreenWithOrigin(double lon, double lat,
                                         const QPointF& origin) const {
    return QPointF(lonToWorldX(lon, m_zoom) - origin.x(),
                   latToWorldY(lat, m_zoom) - origin.y());
}

QPointF MapWidget::geoToScreen(double lon, double lat) const {
    return geoToScreenWithOrigin(lon, lat, originWorld());
}

void MapWidget::screenToGeo(const QPointF& p, double& lon, double& lat) const {
    const QPointF o = originWorld();
    lon = worldXToLon(o.x() + p.x(), m_zoom);
    lat = worldYToLat(o.y() + p.y(), m_zoom);
}

void MapWidget::setCenterFromWorld(double worldX, double worldY) {
    m_centerLon = worldXToLon(worldX, m_zoom);
    m_centerLat = clampLatitude(worldYToLat(worldY, m_zoom));
}

void MapWidget::drawTiles(QPainter& p, const QSize& viewport,
                          const QPointF& origin) {
    // Ocean-like background shows through where tiles are missing (poles).
    p.fillRect(QRect(QPoint(0, 0), viewport), QColor(0xAA, 0xD3, 0xDF));

    const int ts = TileManager::tileSize();
    const int n = 1 << m_zoom;   // tiles per axis at this zoom

    const int firstTx = static_cast<int>(std::floor(origin.x() / ts));
    const int lastTx  = static_cast<int>(std::floor((origin.x() + viewport.width()) / ts));
    const int firstTy = static_cast<int>(std::floor(origin.y() / ts));
    const int lastTy  = static_cast<int>(std::floor((origin.y() + viewport.height()) / ts));

    for (int tx = firstTx; tx <= lastTx; ++tx) {
        for (int ty = firstTy; ty <= lastTy; ++ty) {
            if (ty < 0 || ty >= n)
                continue;                       // above/below the world
            const int wx = ((tx % n) + n) % n;  // wrap longitude
            const QPixmap pm = m_tiles->tile(m_zoom, wx, ty);
            const double sx = tx * ts - origin.x();
            const double sy = ty * ts - origin.y();
            p.drawPixmap(QPointF(sx, sy), pm);
        }
    }
}

void MapWidget::drawOverlay(QPainter& p, const QPointF& origin) {
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1) Generic annotations (drawn underneath the points).
    for (const Annotation& a : m_annotations) {
        switch (a.type) {
        case Annotation::Polyline:
        case Annotation::Polygon: {
            if (a.coords.size() < 2) break;
            QPen pen(a.color);
            pen.setWidthF(a.widthPx);
            pen.setJoinStyle(Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(a.type == Annotation::Polygon
                           ? QBrush(QColor(a.color.red(), a.color.green(),
                                           a.color.blue(), 60))
                           : Qt::NoBrush);
            QPolygonF poly;
            for (const GeoPoint& g : a.coords)
                poly << geoToScreenWithOrigin(g.lon, g.lat, origin);
            if (a.type == Annotation::Polygon)
                p.drawPolygon(poly);
            else
                p.drawPolyline(poly);
            break;
        }
        case Annotation::Circle: {
            if (a.coords.isEmpty()) break;
            QPen pen(a.color);
            pen.setWidthF(a.widthPx);
            p.setPen(pen);
            p.setBrush(QColor(a.color.red(), a.color.green(), a.color.blue(), 40));
            const QPointF c = geoToScreenWithOrigin(a.coords.first().lon,
                                                    a.coords.first().lat, origin);
            p.drawEllipse(c, a.radiusPx, a.radiusPx);
            break;
        }
        case Annotation::Marker: {
            if (a.coords.isEmpty()) break;
            const QPointF c = geoToScreenWithOrigin(a.coords.first().lon,
                                                    a.coords.first().lat, origin);
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(a.color);
            p.drawEllipse(c, a.radiusPx, a.radiusPx);
            break;
        }
        case Annotation::Text: {
            if (a.coords.isEmpty()) break;
            const QPointF c = geoToScreenWithOrigin(a.coords.first().lon,
                                                    a.coords.first().lat, origin);
            p.setPen(a.color);
            p.drawText(c, a.text);
            break;
        }
        }
    }

    // 2) Connecting polyline through the points, in input order.
    if (m_showLines && m_points.size() >= 2) {
        QPolygonF line;
        for (const GeoPoint& g : m_points)
            line << geoToScreenWithOrigin(g.lon, g.lat, origin);
        QPen pen(QColor(0x1E, 0x88, 0xE5, 220));
        pen.setWidthF(3.0);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(line);
    }

    // 3) Point markers and labels on top.
    const QColor markerColor(0xD3, 0x2F, 0x2F);
    QFont labelFont = p.font();
    labelFont.setPointSizeF(labelFont.pointSizeF() > 0 ? labelFont.pointSizeF() : 10);
    labelFont.setBold(true);
    p.setFont(labelFont);
    const QFontMetrics fm(labelFont);
    const double r = 6.0;

    for (int i = 0; i < m_points.size(); ++i) {
        const GeoPoint& g = m_points.at(i);
        const QPointF c = geoToScreenWithOrigin(g.lon, g.lat, origin);

        p.setPen(QPen(Qt::white, 2));
        p.setBrush(markerColor);
        p.drawEllipse(c, r, r);

        if (m_showLabels && !g.name.isEmpty()) {
            const QString text = g.name;
            const QRect tb = fm.boundingRect(text);
            const double pad = 3.0;
            QRectF bg(c.x() + r + 4, c.y() - tb.height() / 2.0 - pad,
                      tb.width() + 2 * pad, tb.height() + 2 * pad);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 200));
            p.drawRoundedRect(bg, 3, 3);
            p.setPen(QColor(0x21, 0x21, 0x21));
            p.drawText(bg.adjusted(pad, pad, -pad, -pad),
                       Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    }
}

void MapWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const QSize vp = size();
    const QPointF origin = originWorld();
    drawTiles(p, vp, origin);
    drawOverlay(p, origin);
}

QImage MapWidget::renderToImage(const QSize& size) {
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const double cx = lonToWorldX(m_centerLon, m_zoom);
    const double cy = latToWorldY(m_centerLat, m_zoom);
    const QPointF origin(cx - size.width() / 2.0, cy - size.height() / 2.0);

    QPainter p(&img);
    drawTiles(p, size, origin);
    drawOverlay(p, origin);
    p.end();
    return img;
}

void MapWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragMoved = false;
        m_lastMousePos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging) {
        const QPoint delta = e->pos() - m_lastMousePos;
        if (delta.manhattanLength() > 2)
            m_dragMoved = true;
        m_lastMousePos = e->pos();

        const QPointF o = originWorld();
        // Dragging the map right should move the view left, hence subtract delta.
        setCenterFromWorld(o.x() + width() / 2.0 - delta.x(),
                           o.y() + height() / 2.0 - delta.y());
        update();
        emit viewChanged();
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        if (!m_dragMoved) {
            double lon, lat;
            screenToGeo(e->pos(), lon, lat);
            emit mapClicked(lon, lat);
        }
    }
}

void MapWidget::wheelEvent(QWheelEvent* e) {
    const int steps = e->angleDelta().y() > 0 ? 1 : -1;
    const int newZoom = std::max(kMinZoom, std::min(kMaxZoom, m_zoom + steps));
    if (newZoom == m_zoom) {
        e->accept();
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF cursor = e->position();
#else
    const QPointF cursor = e->posF();
#endif

    // Keep the geographic point under the cursor fixed across the zoom change.
    double lon, lat;
    screenToGeo(cursor, lon, lat);

    m_zoom = newZoom;

    const double wx = lonToWorldX(lon, m_zoom);
    const double wy = latToWorldY(lat, m_zoom);
    // We want this world point to land back under the cursor.
    const double ox = wx - cursor.x();
    const double oy = wy - cursor.y();
    setCenterFromWorld(ox + width() / 2.0, oy + height() / 2.0);

    update();
    emit viewChanged();
    e->accept();
}
