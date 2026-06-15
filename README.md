# Map Points Visualizer

A C++ / Qt5 application that takes named geographic points (WGS84 longitude,
latitude, name), draws them on a fragment of a world map, labels them and
connects them with lines, in the order they were given. It is built so that
**more things can be drawn on the same map later** (see *Extending* below).

## Features

- Slippy-map background using **Web Mercator** raster tiles (OpenStreetMap by
  default), downloaded asynchronously and cached on disk and in memory.
- **Fully offline support**: pre-download tiles for an area with the bundled
  `tools/download_tiles.py` script and point the app at the folder (see
  *Fully offline maps*). If a tile is missing, a neutral grid background is
  drawn instead, so points, lines and labels are always visible.
- Editable points table (longitude, latitude, name).
- Points connected by a polyline in input order; labeled markers on top.
- Pan (left-drag), zoom (mouse wheel, cursor-anchored), **Fit to points**.
- Load / save points as CSV, export the current view as PNG/JPEG.

## Input format (CSV)

One point per line, comma-separated, in the order requested:

```
lon,lat,name
```

- `lon`, `lat` are WGS84 degrees (e.g. `37.6173,55.7558`).
- `name` is free text and may itself contain commas.
- Lines starting with `#` and header lines (non-numeric `lon`/`lat`) are ignored.

See `sample_points.csv`.

## Building

Requires Qt5 (`Widgets`, `Network`), CMake >= 3.16 and a C++14 compiler.

```bash
# Install Qt5 dev packages, e.g. on Debian/Ubuntu:
sudo apt install qtbase5-dev cmake g++

cmake -S . -B build
cmake --build build -j

# run
./build/map_points                 # starts with sample data
./build/map_points my_points.csv   # load a CSV at startup

# run the pure-C++ projection self-test
ctest --test-dir build
```

> The `qtbase5-dev` package provides `Qt5::Network`; no extra package is needed.

## Fully offline maps

You can run with no network at all by pre-downloading the map tiles for the
area you care about and pointing the app at the folder.

1. Download tiles with the bundled script (Python 3, standard library only):

   ```bash
   # cover the area of a points file, zoom 4..9, with some margin
   python3 tools/download_tiles.py --csv my_points.csv \
       --min-zoom 4 --max-zoom 9 --margin 0.3 --out tiles

   # or an explicit bounding box (minlon minlat maxlon maxlat)
   python3 tools/download_tiles.py --bbox 28 54 62 61 \
       --min-zoom 4 --max-zoom 9 --out tiles

   # see how many tiles a job needs without downloading
   python3 tools/download_tiles.py --bbox 28 54 62 61 \
       --min-zoom 4 --max-zoom 9 --out tiles --dry-run
   ```

   Tiles are saved as `tiles/<z>/<x>/<y>.png`. The script rate-limits requests
   (`--delay`), skips already-downloaded tiles, and aborts on oversized jobs
   (`--max-tiles`). **Respect the tile provider's usage policy** — OSM forbids
   bulk downloading of large areas; keep zoom ranges modest or use your own
   tile server via `--url-template`.

2. Point the app at the folder, in any of these ways:

   ```bash
   ./build/map_points --tiles ./tiles my_points.csv   # command line
   MAP_TILES_DIR=./tiles ./build/map_points            # environment variable
   ```

   or use the **Offline tiles folder...** toolbar button at runtime.

The tile folder is checked *before* any network access, so once it contains
the tiles for the current view, the map renders entirely offline (areas with no
tile fall back to the neutral grid).

## Usage

- Edit cells in the table, or click **Add point** / **Remove selected**.
- Toggle **Connecting lines**, **Labels**, and **Download map tiles (online)**.
  Turning tiles off forces the offline grid background.
- Enable **Add point on map click** to drop new points by clicking the map.
- **Fit** frames all points; **Export image...** saves the current view.

## Extending ("дорисовывать" more on the map)

All overlay drawing goes through `MapWidget::drawOverlay`, and arbitrary extra
geometry is expressed as `Annotation` objects in geographic coordinates
(`src/GeoTypes.h`):

```cpp
// e.g. draw an extra route and a highlighted area:
map->addAnnotation(Annotation::makePolyline(routePts, Qt::darkGreen, 3));
map->addAnnotation(Annotation::makeMarker(GeoPoint(13.405, 52.52, "Berlin")));
```

Supported annotation types: `Marker`, `Polyline`, `Polygon`, `Circle`, `Text`.
They are projected and drawn automatically, so new features only need to append
annotations — no changes to the rendering/projection code. `MapWidget` also
emits `mapClicked(lon, lat)` for click-driven tools.

## Project layout

| File | Purpose |
|------|---------|
| `src/MercatorProjection.h` | Pure-C++ Web Mercator math (geo ↔ pixel/tile) |
| `src/GeoTypes.h` | `GeoPoint` and the extensible `Annotation` type |
| `src/TileManager.*` | Async tile download + disk/memory cache + offline fallback |
| `src/MapWidget.*` | Tiled map view, overlays, pan/zoom, image export |
| `src/MainWindow.*` | Table editor, CSV I/O, offline-folder picker, toolbar |
| `tools/download_tiles.py` | Download tiles for offline use (`z/x/y.png`) |
| `tests/test_mercator.cpp` | Projection round-trip self-test (no Qt needed) |
| `tests/render_smoke.cpp` | Headless overlay-rendering smoke test |
