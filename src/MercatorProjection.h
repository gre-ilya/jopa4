#ifndef MERCATORPROJECTION_H
#define MERCATORPROJECTION_H

// Web Mercator (EPSG:3857) projection helpers used for slippy-map tiles.
//
// This header is intentionally free of any Qt dependency so that the
// projection math can be unit-tested as plain C++.
//
// Coordinate conventions:
//   * Geographic coordinates are WGS84 longitude/latitude in degrees.
//   * "World pixel" coordinates describe a pixel on the global map at a given
//     zoom level, where the whole world spans worldSize(zoom) pixels in both
//     directions. The origin (0,0) is the top-left corner: longitude -180 and
//     the maximum Mercator latitude (~+85.0511 deg).
//   * Tiles are 256x256 px; tile (x,y) at a zoom covers world pixels
//     [x*256, (x+1)*256) x [y*256, (y+1)*256).

#include <cmath>
#include <algorithm>

namespace geo {

constexpr double kTileSize = 256.0;
// Maximum latitude representable in Web Mercator (where y would go to +/-inf).
constexpr double kMaxLatitude = 85.05112877980659;
constexpr double kPi = 3.14159265358979323846;

inline double deg2rad(double deg) { return deg * kPi / 180.0; }
inline double rad2deg(double rad) { return rad * 180.0 / kPi; }

// Number of world pixels along one axis at the given (possibly fractional) zoom.
inline double worldSize(double zoom) {
    return kTileSize * std::pow(2.0, zoom);
}

inline double clampLatitude(double lat) {
    return std::max(-kMaxLatitude, std::min(kMaxLatitude, lat));
}

// Longitude (deg) -> world pixel X at the given zoom.
inline double lonToWorldX(double lon, double zoom) {
    return (lon + 180.0) / 360.0 * worldSize(zoom);
}

// Latitude (deg) -> world pixel Y at the given zoom.
inline double latToWorldY(double lat, double zoom) {
    const double s = std::sin(deg2rad(clampLatitude(lat)));
    const double y = 0.5 - std::log((1.0 + s) / (1.0 - s)) / (4.0 * kPi);
    return y * worldSize(zoom);
}

// World pixel X -> longitude (deg).
inline double worldXToLon(double x, double zoom) {
    return x / worldSize(zoom) * 360.0 - 180.0;
}

// World pixel Y -> latitude (deg).
inline double worldYToLat(double y, double zoom) {
    const double n = kPi - 2.0 * kPi * y / worldSize(zoom);
    return rad2deg(std::atan(std::sinh(n)));
}

// Choose the largest integer zoom such that a geographic bounding box fits
// inside a viewport of the given pixel size (with a little padding). Result is
// clamped to [minZoom, maxZoom].
inline int zoomForBounds(double minLon, double minLat,
                         double maxLon, double maxLat,
                         double viewWidthPx, double viewHeightPx,
                         int minZoom = 0, int maxZoom = 19,
                         double paddingFactor = 0.85) {
    if (viewWidthPx <= 0 || viewHeightPx <= 0)
        return minZoom;

    for (int z = maxZoom; z >= minZoom; --z) {
        const double x0 = lonToWorldX(minLon, z);
        const double x1 = lonToWorldX(maxLon, z);
        // Note: higher latitude maps to a smaller world Y.
        const double y0 = latToWorldY(maxLat, z);
        const double y1 = latToWorldY(minLat, z);
        const double w = std::abs(x1 - x0);
        const double h = std::abs(y1 - y0);
        if (w <= viewWidthPx * paddingFactor &&
            h <= viewHeightPx * paddingFactor) {
            return z;
        }
    }
    return minZoom;
}

} // namespace geo

#endif // MERCATORPROJECTION_H
