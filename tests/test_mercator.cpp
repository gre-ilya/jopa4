// Plain-C++ sanity checks for the Web Mercator projection math.
// Builds without Qt:  g++ -std=c++14 tests/test_mercator.cpp -o test_mercator
#include "../src/MercatorProjection.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace geo;

static int g_failures = 0;

static void approx(const char* what, double got, double want, double eps) {
    const double diff = std::abs(got - want);
    if (diff > eps) {
        std::printf("FAIL %-28s got=%.6f want=%.6f (diff=%.6g)\n",
                    what, got, want, diff);
        ++g_failures;
    } else {
        std::printf("ok   %-28s = %.6f\n", what, got);
    }
}

int main() {
    // At zoom 0 the world is one 256px tile.
    approx("worldSize(0)", worldSize(0), 256.0, 1e-9);
    approx("worldSize(1)", worldSize(1), 512.0, 1e-9);

    // Longitude edges and centre.
    approx("lonToWorldX(-180, z0)", lonToWorldX(-180.0, 0), 0.0, 1e-9);
    approx("lonToWorldX(+180, z0)", lonToWorldX(180.0, 0), 256.0, 1e-9);
    approx("lonToWorldX(0, z0)", lonToWorldX(0.0, 0), 128.0, 1e-9);

    // Equator sits exactly in the middle vertically.
    approx("latToWorldY(0, z0)", latToWorldY(0.0, 0), 128.0, 1e-9);

    // Round-trip: geo -> world pixel -> geo for a few real cities.
    struct { const char* name; double lon, lat; } pts[] = {
        {"Moscow",    37.6173, 55.7558},
        {"London",    -0.1276, 51.5072},
        {"Sydney",    151.2093, -33.8688},
        {"Quito",     -78.4678, -0.1807},
    };
    for (const auto& p : pts) {
        for (double z : {2.0, 8.0, 14.0}) {
            const double x = lonToWorldX(p.lon, z);
            const double y = latToWorldY(p.lat, z);
            const double lon2 = worldXToLon(x, z);
            const double lat2 = worldYToLat(y, z);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s lon rt z%g", p.name, z);
            approx(buf, lon2, p.lon, 1e-6);
            std::snprintf(buf, sizeof(buf), "%s lat rt z%g", p.name, z);
            approx(buf, lat2, p.lat, 1e-6);
        }
    }

    // zoomForBounds: a tiny box should pick a high zoom, a global box a low one.
    int zTight = zoomForBounds(37.60, 55.74, 37.62, 55.76, 800, 600);
    int zWide  = zoomForBounds(-170, -80, 170, 80, 800, 600);
    std::printf("zoom tight=%d wide=%d\n", zTight, zWide);
    if (!(zTight > zWide)) {
        std::printf("FAIL zoomForBounds ordering\n");
        ++g_failures;
    }

    std::printf("\n%s (%d failure%s)\n", g_failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
