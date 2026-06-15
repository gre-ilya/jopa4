// Offline rendering smoke test: builds a MapWidget, feeds it points and an
// annotation, renders to an image with tile downloads disabled (so the grid
// fallback is used) and checks that something was actually drawn.
//
// Run headless with:  QT_QPA_PLATFORM=offscreen ./render_smoke
#include "../src/MapWidget.h"
#include "../src/TileManager.h"
#include "../src/GeoTypes.h"

#include <QApplication>
#include <QImage>
#include <QColor>
#include <cstdio>

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    MapWidget map;
    map.resize(640, 480);
    map.tileManager()->setNetworkEnabled(false); // force offline grid background

    QList<GeoPoint> pts{
        GeoPoint(37.6173, 55.7558, "Moscow"),
        GeoPoint(30.3158, 59.9391, "Saint Petersburg"),
        GeoPoint(49.1221, 55.7887, "Kazan"),
    };
    map.setPoints(pts);
    map.addAnnotation(Annotation::makeMarker(GeoPoint(44.0, 56.3, "extra")));
    map.fitToPoints();

    const QImage img = map.renderToImage(QSize(640, 480));
    if (img.isNull() || img.size() != QSize(640, 480)) {
        std::printf("FAIL: rendered image is null or wrong size\n");
        return 1;
    }

    // The red point markers must appear somewhere in the output.
    bool foundMarker = false;
    for (int y = 0; y < img.height() && !foundMarker; ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() > 150 && c.green() < 90 && c.blue() < 90) {
                foundMarker = true;
                break;
            }
        }
    }
    if (!foundMarker) {
        std::printf("FAIL: no point markers found in rendered image\n");
        return 1;
    }

    std::printf("ok: rendered %dx%d image with visible markers\n",
                img.width(), img.height());
    return 0;
}
