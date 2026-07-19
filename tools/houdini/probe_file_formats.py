"""Generate bounded Houdini files for inspecting raw and SCF wrappers."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import hou


def build_triangle() -> hou.Geometry:
    """Create a minimal triangle geometry."""
    geometry = hou.Geometry()
    points = geometry.createPoints(((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)))
    polygon = geometry.createPolygon()
    for point in points:
        polygon.addVertex(point)
    return geometry


def build_points(point_count: int) -> hou.Geometry:
    """Create deterministic point-only geometry.

    Args:
        point_count: Number of points to create.

    Returns:
        Point-only geometry suitable for forcing multiple SCF blocks.
    """
    geometry = hou.Geometry()
    geometry.createPoints(
        tuple((float(index), float(index % 7), 0.0) for index in range(point_count))
    )
    return geometry


def write_pair(
    geometry: hou.Geometry, output_directory: Path, stem: str
) -> dict[str, int]:
    """Write raw and SCF variants and return their byte sizes."""
    sizes: dict[str, int] = {}
    for extension in ("bgeo", "bgeo.sc"):
        output_path = output_directory / f"{stem}.{extension}"
        geometry.saveToFile(str(output_path))
        sizes[extension] = output_path.stat().st_size
    return sizes


def main() -> int:
    """Write probe files and print a JSON size report."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--point-count", type=int, default=10_000)
    arguments = parser.parse_args()
    if arguments.point_count <= 0:
        parser.error("--point-count must be positive")

    arguments.output_directory.mkdir(parents=True, exist_ok=True)
    report = {
        "houdini_version": hou.applicationVersionString(),
        "triangle": write_pair(
            build_triangle(), arguments.output_directory, "triangle"
        ),
        "points": write_pair(
            build_points(arguments.point_count), arguments.output_directory, "points"
        ),
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
