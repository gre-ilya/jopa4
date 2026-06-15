#!/usr/bin/env python3
"""Download slippy-map tiles for offline use by the Map Points Visualizer.

Tiles are written in the standard ``<out>/<z>/<x>/<y>.png`` layout, which is
exactly what the application reads (point it there with ``--tiles <out>`` or
the *Offline tiles folder...* toolbar button).

Define the area to cover either explicitly with a bounding box or implicitly
from a points CSV (the same ``lon,lat,name`` format the app uses):

    # by bounding box (minlon minlat maxlon maxlat), zoom 4..9
    python3 tools/download_tiles.py --out tiles \\
        --bbox 28 54 62 61 --min-zoom 4 --max-zoom 9

    # from a CSV of points, with a margin around them
    python3 tools/download_tiles.py --out tiles \\
        --csv sample_points.csv --min-zoom 4 --max-zoom 9 --margin 0.3

Only the Python standard library is required.

Please respect the tile provider's usage policy. The OpenStreetMap policy
(https://operations.osmfoundation.org/policies/tiles/) requires a valid
User-Agent, forbids bulk/automated downloading of large areas, and asks for a
low request rate -- keep zoom ranges modest and the delay non-trivial. For
anything heavy, use your own tile server or a provider that permits it.
"""

import argparse
import math
import os
import sys
import time
import urllib.request
import urllib.error


def deg2num(lat_deg, lon_deg, zoom):
    """Convert lat/lon (WGS84 degrees) to (xtile, ytile) at a zoom level."""
    lat_deg = max(-85.05112878, min(85.05112878, lat_deg))
    lat_rad = math.radians(lat_deg)
    n = 2 ** zoom
    xtile = int((lon_deg + 180.0) / 360.0 * n)
    ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    xtile = max(0, min(n - 1, xtile))
    ytile = max(0, min(n - 1, ytile))
    return xtile, ytile


def read_csv_bbox(path):
    """Return (minlon, minlat, maxlon, maxlat) covering points in a CSV file."""
    lons, lats = [], []
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) < 2:
                continue
            try:
                lon = float(parts[0])
                lat = float(parts[1])
            except ValueError:
                continue  # header / non-numeric line
            lons.append(lon)
            lats.append(lat)
    if not lons:
        sys.exit("error: no valid points found in %s" % path)
    return min(lons), min(lats), max(lons), max(lats)


def count_tiles(bbox, zmin, zmax):
    minlon, minlat, maxlon, maxlat = bbox
    total = 0
    per_zoom = []
    for z in range(zmin, zmax + 1):
        x0, y0 = deg2num(maxlat, minlon, z)  # top-left
        x1, y1 = deg2num(minlat, maxlon, z)  # bottom-right
        xlo, xhi = min(x0, x1), max(x0, x1)
        ylo, yhi = min(y0, y1), max(y0, y1)
        n = (xhi - xlo + 1) * (yhi - ylo + 1)
        per_zoom.append((z, xlo, xhi, ylo, yhi, n))
        total += n
    return total, per_zoom


def main():
    ap = argparse.ArgumentParser(
        description="Download offline map tiles (z/x/y.png).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--bbox", nargs=4, type=float,
                     metavar=("MINLON", "MINLAT", "MAXLON", "MAXLAT"),
                     help="bounding box in WGS84 degrees")
    src.add_argument("--csv", metavar="FILE",
                     help="points CSV (lon,lat,name); bbox is derived from it")
    ap.add_argument("--out", required=True, metavar="DIR",
                    help="output tiles directory")
    ap.add_argument("--min-zoom", type=int, default=4)
    ap.add_argument("--max-zoom", type=int, default=10)
    ap.add_argument("--margin", type=float, default=0.2,
                    help="fraction of bbox size added as padding (CSV mode)")
    ap.add_argument("--url-template",
                    default="https://tile.openstreetmap.org/{z}/{x}/{y}.png",
                    help="tile URL template with {z} {x} {y} placeholders")
    ap.add_argument("--user-agent",
                    default="MapPointsVisualizer/1.0 (offline tile fetch)",
                    help="HTTP User-Agent (required by OSM policy)")
    ap.add_argument("--delay", type=float, default=0.5,
                    help="seconds to wait between downloads (be polite)")
    ap.add_argument("--retries", type=int, default=3)
    ap.add_argument("--max-tiles", type=int, default=50000,
                    help="abort if the job would exceed this many tiles")
    ap.add_argument("--dry-run", action="store_true",
                    help="only report how many tiles would be downloaded")
    args = ap.parse_args()

    if args.min_zoom > args.max_zoom:
        sys.exit("error: --min-zoom must be <= --max-zoom")

    if args.csv:
        minlon, minlat, maxlon, maxlat = read_csv_bbox(args.csv)
        dlon = max(1e-4, (maxlon - minlon) * args.margin)
        dlat = max(1e-4, (maxlat - minlat) * args.margin)
        bbox = (minlon - dlon, minlat - dlat, maxlon + dlon, maxlat + dlat)
    else:
        bbox = tuple(args.bbox)

    total, per_zoom = count_tiles(bbox, args.min_zoom, args.max_zoom)
    print("Area  (minlon,minlat,maxlon,maxlat): %.5f, %.5f, %.5f, %.5f"
          % bbox)
    for z, xlo, xhi, ylo, yhi, n in per_zoom:
        print("  zoom %2d: x[%d..%d] y[%d..%d] -> %d tiles"
              % (z, xlo, xhi, ylo, yhi, n))
    print("Total tiles: %d" % total)

    if total > args.max_tiles:
        sys.exit("error: %d tiles exceeds --max-tiles=%d (narrow the bbox or "
                 "zoom range, or raise the limit)" % (total, args.max_tiles))
    if args.dry_run:
        return

    downloaded = skipped = failed = 0
    for z, xlo, xhi, ylo, yhi, _ in per_zoom:
        for x in range(xlo, xhi + 1):
            col_dir = os.path.join(args.out, str(z), str(x))
            os.makedirs(col_dir, exist_ok=True)
            for y in range(ylo, yhi + 1):
                dest = os.path.join(col_dir, "%d.png" % y)
                if os.path.exists(dest) and os.path.getsize(dest) > 0:
                    skipped += 1
                    continue
                url = (args.url_template
                       .replace("{z}", str(z))
                       .replace("{x}", str(x))
                       .replace("{y}", str(y)))
                ok = False
                for attempt in range(1, args.retries + 1):
                    try:
                        req = urllib.request.Request(
                            url, headers={"User-Agent": args.user_agent})
                        with urllib.request.urlopen(req, timeout=30) as resp:
                            data = resp.read()
                        with open(dest, "wb") as fh:
                            fh.write(data)
                        ok = True
                        break
                    except (urllib.error.URLError, OSError) as exc:
                        if attempt == args.retries:
                            print("  FAILED %d/%d/%d: %s" % (z, x, y, exc),
                                  file=sys.stderr)
                        else:
                            time.sleep(args.delay * attempt)
                if ok:
                    downloaded += 1
                    time.sleep(args.delay)
                else:
                    failed += 1
        print("zoom %d done (downloaded=%d skipped=%d failed=%d)"
              % (z, downloaded, skipped, failed))

    print("Finished: downloaded=%d skipped=%d failed=%d -> %s"
          % (downloaded, skipped, failed, os.path.abspath(args.out)))
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
