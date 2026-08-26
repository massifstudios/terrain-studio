"""Regenerate the `.massifdem` compatibility fixtures with the game's own writer.

The test suite must not depend on the game checkout, so the outputs are
committed here. Run this only when the format itself changes — which is the
game's decision, never this tool's.

    python3 make_fixture.py --massif-tools ../../../../massif/Tools
"""

import argparse
import os
import struct
import sys

import numpy as np


def analytic_surface(size):
    """A cone crossed with a ridge: smooth, asymmetric, no flat regions."""
    rows, columns = np.meshgrid(np.arange(size), np.arange(size), indexing="ij")
    northing = (rows / (size - 1)) * 2.0 - 1.0
    easting = (columns / (size - 1)) * 2.0 - 1.0

    radius = np.sqrt(northing**2 + easting**2)
    cone = np.clip(1.0 - radius, 0.0, 1.0) * 900.0
    ridge = np.exp(-((northing * 3.0) ** 2)) * 350.0
    tilt = easting * 120.0
    return 1200.0 + cone + ridge + tilt


def dequantised_heights(path):
    """What the engine holds after import: the stored levels as float32 meters.

    The preview reference has to start from these, not from the float64 source,
    or it would be comparing against altitudes the engine never sees.
    """
    raw = open(path, "rb").read()
    width, height = struct.unpack_from("<II", raw, 12)
    _, minimum, step, _, _ = struct.unpack_from("<ddddd", raw, 24)
    levels = np.frombuffer(raw[192:], dtype="<u2").astype(np.float64).reshape(height, width)
    return (minimum + levels * step).astype(np.float32)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--massif-tools", required=True,
                        help="path to the game's Tools directory, for massif_dem.py")
    parser.add_argument("--size", type=int, default=96)
    arguments = parser.parse_args()

    sys.path.insert(0, os.path.abspath(arguments.massif_tools))
    from massif_dem import write_dem, write_preview

    here = os.path.dirname(os.path.abspath(__file__))
    heights = analytic_surface(arguments.size)

    dem_path = os.path.join(here, "fixture.massifdem")
    write_dem(dem_path, heights,
              resolution_meters=30.0, easting_min=39075.5, northing_max=214292.25,
              label="Terrain Studio format fixture", coordinate_system="EPSG:31254")
    write_preview(os.path.join(here, "fixture_preview.png"),
                  dequantised_heights(dem_path), resolution_meters=30.0)
    print(f"wrote {arguments.size}x{arguments.size} fixture and preview to {here}")


if __name__ == "__main__":
    main()
