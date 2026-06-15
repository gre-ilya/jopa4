#ifndef GEOTYPES_H
#define GEOTYPES_H

#include <QString>
#include <QColor>
#include <QVector>

#include <utility>

// A named geographic point in WGS84 (longitude/latitude in degrees).
struct GeoPoint {
    QString name;
    double lon = 0.0;
    double lat = 0.0;

    GeoPoint() = default;
    GeoPoint(double lon_, double lat_, QString name_ = QString())
        : name(std::move(name_)), lon(lon_), lat(lat_) {}
};

// A free-form overlay item expressed in geographic coordinates. This is the
// extension point for "drawing more things on the map later": new features can
// simply append annotations and the map widget renders them generically,
// without needing to know what they mean.
struct Annotation {
    enum Type { Marker, Polyline, Polygon, Circle, Text };

    Type type = Marker;
    QVector<GeoPoint> coords;   // geometry vertices (interpretation depends on type)
    QString text;               // label / text content
    QColor color = QColor(0xD3, 0x2F, 0x2F);
    double widthPx = 2.0;       // line width (Polyline/Polygon/Circle outline)
    double radiusPx = 6.0;      // pixel radius for Marker / Circle

    static Annotation makeMarker(const GeoPoint& p, const QString& label = {},
                                 const QColor& c = QColor(0xD3, 0x2F, 0x2F)) {
        Annotation a;
        a.type = Marker;
        a.coords = {p};
        a.text = label;
        a.color = c;
        return a;
    }
    static Annotation makePolyline(const QVector<GeoPoint>& pts,
                                   const QColor& c, double width = 2.0) {
        Annotation a;
        a.type = Polyline;
        a.coords = pts;
        a.color = c;
        a.widthPx = width;
        return a;
    }
    static Annotation makeText(const GeoPoint& p, const QString& text,
                               const QColor& c = Qt::black) {
        Annotation a;
        a.type = Text;
        a.coords = {p};
        a.text = text;
        a.color = c;
        return a;
    }
};

#endif // GEOTYPES_H
