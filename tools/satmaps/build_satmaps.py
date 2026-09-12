#!/usr/bin/env python3
"""
Builds the engine's satellite colour strips ("satmaps") from real imagery.

For every region in regions.json the tool fetches a square of Web Mercator
tiles from two public sources: Sentinel-2 cloudless imagery (EOX, CC BY 4.0)
for colour, and Terrarium elevation tiles (AWS Open Data) for height. Every
pixel is then binned by its ordering key -- elevation for most regions,
luminance for water -- and each bin becomes one column of a 1024 x 16 strip.
The sixteen rows are luminance quantiles of the bin, darkest at the top, so a
column keeps the spread of colour real ground shows at that height rather than
a single average.

Strips land in engine/assets/satmaps/<library>/<index>_<name>.png. The
engine's SatMap node reads them by library and index. Nothing here is
randomised, so re-running the tool over the same tiles gives the same bytes.

Usage:
    build_satmaps.py [--only <library>] [--cache <dir>]
"""

from __future__ import annotations

import argparse
import io
import json
import math
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image

TOOL_DIRECTORY = Path(__file__).resolve().parent
REPOSITORY_ROOT = TOOL_DIRECTORY.parents[1]
OUTPUT_DIRECTORY = REPOSITORY_ROOT / "engine" / "assets" / "satmaps"

COLOR_TILE_URL = "https://tiles.maps.eox.at/wmts/1.0.0/s2cloudless-2020_3857/default/g/{z}/{y}/{x}.jpg"
ELEVATION_TILE_URL = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png"
TILE_SIZE = 256
TERRARIUM_NO_DATA = -32768.0

STRIP_WIDTH = 1024
STRIP_ROWS = 16
CLIP_PERCENTILE = 0.5
SMOOTHING_WIDTH = 5


def tileIndexFor(latitude: float, longitude: float, zoom: int) -> tuple[int, int]:
    scale = 2 ** zoom
    x = int((longitude + 180.0) / 360.0 * scale)
    latitudeRadians = math.radians(latitude)
    y = int((1.0 - math.log(math.tan(latitudeRadians) + 1.0 / math.cos(latitudeRadians)) / math.pi) / 2.0 * scale)
    return x, y


def fetchBytes(url: str, cachePath: Path) -> bytes:
    if cachePath.exists():
        return cachePath.read_bytes()
    lastError: Exception | None = None
    for attempt in range(4):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": "terrain-studio-satmap-builder/1.0"})
            with urllib.request.urlopen(request, timeout=60) as response:
                data = response.read()
            cachePath.parent.mkdir(parents=True, exist_ok=True)
            cachePath.write_bytes(data)
            return data
        except Exception as error:  # noqa: BLE001 - retried, then reported
            lastError = error
            time.sleep(1.5 * (attempt + 1))
    raise RuntimeError(f"could not fetch {url}: {lastError}")


def fetchColorTile(zoom: int, x: int, y: int, cache: Path) -> np.ndarray:
    data = fetchBytes(COLOR_TILE_URL.format(z=zoom, x=x, y=y), cache / "color" / f"{zoom}_{x}_{y}.jpg")
    image = Image.open(io.BytesIO(data)).convert("RGB")
    return np.asarray(image, dtype=np.uint8)


def fetchElevationTile(zoom: int, x: int, y: int, cache: Path) -> np.ndarray:
    data = fetchBytes(ELEVATION_TILE_URL.format(z=zoom, x=x, y=y), cache / "elevation" / f"{zoom}_{x}_{y}.png")
    image = np.asarray(Image.open(io.BytesIO(data)).convert("RGB"), dtype=np.float64)
    return image[:, :, 0] * 256.0 + image[:, :, 1] + image[:, :, 2] / 256.0 - 32768.0


def fetchRegion(region: dict, cache: Path) -> tuple[np.ndarray, np.ndarray]:
    zoom = int(region["zoom"])
    span = int(region.get("span", 3))
    centreX, centreY = tileIndexFor(float(region["latitude"]), float(region["longitude"]), zoom)
    first = -(span // 2)
    coordinates = [(centreX + column, centreY + row) for row in range(first, first + span) for column in range(first, first + span)]

    with ThreadPoolExecutor(max_workers=4) as pool:
        colorTiles = list(pool.map(lambda xy: fetchColorTile(zoom, xy[0], xy[1], cache), coordinates))
        elevationTiles = list(pool.map(lambda xy: fetchElevationTile(zoom, xy[0], xy[1], cache), coordinates))

    size = span * TILE_SIZE
    color = np.zeros((size, size, 3), dtype=np.uint8)
    elevation = np.zeros((size, size), dtype=np.float64)
    for tileIndex, (tileX, tileY) in enumerate(coordinates):
        row = (tileY - (centreY + first)) * TILE_SIZE
        column = (tileX - (centreX + first)) * TILE_SIZE
        color[row:row + TILE_SIZE, column:column + TILE_SIZE] = colorTiles[tileIndex]
        elevation[row:row + TILE_SIZE, column:column + TILE_SIZE] = elevationTiles[tileIndex]
    return color, elevation


def luminanceOf(color: np.ndarray) -> np.ndarray:
    return 0.2126 * color[..., 0] + 0.7152 * color[..., 1] + 0.0722 * color[..., 2]


def orderingKey(region: dict, color: np.ndarray, elevation: np.ndarray) -> np.ndarray:
    order = region.get("order", "elevation")
    if order == "luminance":
        return luminanceOf(color.astype(np.float64))
    if order == "elevation":
        return elevation
    raise ValueError(f"unknown order '{order}' in region {region['name']}")


def usablePixels(region: dict, color: np.ndarray, elevation: np.ndarray) -> np.ndarray:
    usable = elevation > TERRARIUM_NO_DATA + 1.0
    # Pure black is the imagery's own no-data value, never a ground colour.
    usable &= color.max(axis=2) > 4
    if region.get("order", "elevation") == "elevation" and not region.get("keep_sea", False):
        usable &= elevation > 0.5
    return usable


def binColumns(key: np.ndarray, color: np.ndarray) -> list[np.ndarray]:
    low, high = np.percentile(key, [CLIP_PERCENTILE, 100.0 - CLIP_PERCENTILE])
    if high <= low:
        raise ValueError("the ordering key is flat over this region")
    position = np.clip((key - low) / (high - low), 0.0, 1.0)
    binIndex = np.minimum((position * STRIP_WIDTH).astype(np.int64), STRIP_WIDTH - 1)
    order = np.argsort(binIndex, kind="stable")
    sortedBins = binIndex[order]
    sortedColors = color[order]
    edges = np.searchsorted(sortedBins, np.arange(STRIP_WIDTH + 1))
    return [sortedColors[edges[index]:edges[index + 1]] for index in range(STRIP_WIDTH)]


def columnRows(pixels: np.ndarray) -> np.ndarray | None:
    if pixels.shape[0] == 0:
        return None
    ordered = pixels[np.argsort(luminanceOf(pixels.astype(np.float64)), kind="stable")]
    quantiles = (np.arange(STRIP_ROWS) + 0.5) / STRIP_ROWS
    picks = np.minimum((quantiles * ordered.shape[0]).astype(np.int64), ordered.shape[0] - 1)
    return ordered[picks].astype(np.float64)


def fillEmptyColumns(columns: list[np.ndarray | None]) -> np.ndarray:
    strip = np.zeros((STRIP_ROWS, STRIP_WIDTH, 3), dtype=np.float64)
    filled = [index for index, column in enumerate(columns) if column is not None]
    if not filled:
        raise ValueError("no usable pixels at all")
    for index in range(STRIP_WIDTH):
        column = columns[index]
        if column is not None:
            strip[:, index] = column
            continue
        left = max((f for f in filled if f < index), default=None)
        right = min((f for f in filled if f > index), default=None)
        if left is None:
            strip[:, index] = columns[right]
        elif right is None:
            strip[:, index] = columns[left]
        else:
            weight = (index - left) / (right - left)
            strip[:, index] = columns[left] * (1.0 - weight) + columns[right] * weight
    return strip


def smoothAlongStrip(strip: np.ndarray) -> np.ndarray:
    kernel = np.ones(SMOOTHING_WIDTH) / SMOOTHING_WIDTH
    padded = np.pad(strip, ((0, 0), (SMOOTHING_WIDTH // 2, SMOOTHING_WIDTH // 2), (0, 0)), mode="edge")
    smoothed = np.empty_like(strip)
    for row in range(STRIP_ROWS):
        for channel in range(3):
            smoothed[row, :, channel] = np.convolve(padded[row, :, channel], kernel, mode="valid")
    return smoothed


def buildStrip(region: dict, color: np.ndarray, elevation: np.ndarray) -> np.ndarray:
    usable = usablePixels(region, color, elevation)
    if usable.sum() < STRIP_WIDTH * 4:
        raise ValueError(f"only {int(usable.sum())} usable pixels")
    key = orderingKey(region, color, elevation)[usable]
    pixels = color[usable]
    columns = [columnRows(column) for column in binColumns(key, pixels)]
    strip = smoothAlongStrip(fillEmptyColumns(columns))
    return np.clip(np.rint(strip), 0, 255).astype(np.uint8)


def outputPathFor(library: str, index: int, name: str) -> Path:
    return OUTPUT_DIRECTORY / library / f"{index:02d}_{name}.png"


def buildRegion(library: str, index: int, region: dict, cache: Path) -> str:
    color, elevation = fetchRegion(region, cache)
    strip = buildStrip(region, color, elevation)
    path = outputPathFor(library, index, region["name"])
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(strip, mode="RGB").save(path, optimize=True)
    return str(path.relative_to(REPOSITORY_ROOT))


def writeAttribution(manifest: dict) -> None:
    lines = [
        "# Satellite colour strips",
        "",
        "Every strip in this directory was built by `tools/satmaps/build_satmaps.py`",
        "from public satellite data. Colour comes from Sentinel-2 cloudless imagery",
        "by EOX IT Services GmbH (https://s2maps.eu), contains modified Copernicus",
        "Sentinel data 2020, licensed CC BY 4.0. Elevation ordering comes from the",
        "Terrarium tiles of the AWS Open Data terrain set (Mapzen, SRTM, ETOPO1 and",
        "others; see https://github.com/tilezen/joerd/blob/master/docs/attribution.md).",
        "",
        "| Library | Strip | Region | Ordering |",
        "| --- | --- | --- | --- |",
    ]
    for library, regions in manifest["libraries"].items():
        for index, region in enumerate(regions):
            lines.append(
                f"| {library} | {index:02d}_{region['name']} | {region['title']} "
                f"({region['latitude']:.2f}, {region['longitude']:.2f}) | {region.get('order', 'elevation')} |"
            )
    (OUTPUT_DIRECTORY / "ATTRIBUTION.md").write_text("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--only", help="build one library only")
    parser.add_argument("--cache", default=str(TOOL_DIRECTORY / "cache"), help="tile cache directory")
    arguments = parser.parse_args()

    manifest = json.loads((TOOL_DIRECTORY / "regions.json").read_text())
    cache = Path(arguments.cache)
    failures = 0
    for library, regions in manifest["libraries"].items():
        if arguments.only and library != arguments.only:
            continue
        for index, region in enumerate(regions):
            try:
                written = buildRegion(library, index, region, cache)
                print(f"wrote {written}")
            except Exception as error:  # noqa: BLE001 - one bad region must not stop the rest
                failures += 1
                print(f"FAILED {library}/{index:02d}_{region['name']}: {error}", file=sys.stderr)
    writeAttribution(manifest)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
