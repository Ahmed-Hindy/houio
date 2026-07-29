"""Validate lossless simplified attribute domains in a live Houdini runtime."""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path

import hou


def parse_arguments() -> argparse.Namespace:
    """Parse the isolated output directory."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    return parser.parse_args()


def create_source() -> hou.Geometry:
    """Create two triangles sharing points with a vertex UV discontinuity."""
    geometry = hou.Geometry()
    points = []
    for position in (
        (0.0, 0.0, 0.0),
        (1.0, 0.0, 0.0),
        (1.0, 1.0, 0.0),
        (0.0, 1.0, 0.0),
    ):
        point = geometry.createPoint()
        point.setPosition(position)
        points.append(point)

    first = geometry.createPolygon()
    for point_index in (0, 1, 2):
        first.addVertex(points[point_index])
    second = geometry.createPolygon()
    for point_index in (0, 2, 3):
        second.addVertex(points[point_index])

    mask = geometry.addAttrib(hou.attribType.Point, "mask", 0)
    for point, value in zip(points, (10, 20, 30, 40)):
        point.setAttribValue(mask, value)

    uv = geometry.addAttrib(hou.attribType.Vertex, "uv", (0.0, 0.0))
    normal = geometry.addAttrib(hou.attribType.Vertex, "N", (0.0, 0.0, 1.0))
    uv_values = (
        (0.0, 0.0),
        (1.0, 0.0),
        (1.0, 1.0),
        (0.5, 0.5),
        (1.0, 1.0),
        (0.0, 1.0),
    )
    for vertex, value in zip(
        tuple(first.vertices()) + tuple(second.vertices()), uv_values
    ):
        vertex.setAttribValue(uv, value)
        vertex.setAttribValue(normal, (0.0, 0.0, 1.0))

    primitive_id = geometry.addAttrib(hou.attribType.Prim, "id", 0)
    first.setAttribValue(primitive_id, 7)
    second.setAttribValue(primitive_id, 9)

    scale = geometry.addAttrib(hou.attribType.Global, "scale", 0.0)
    geometry.setGlobalAttribValue(scale, 2.5)
    return geometry


def validate_output(path: Path) -> None:
    """Validate topology and every supported numeric domain in Houdini."""
    result = hou.Geometry()
    result.loadFromFile(str(path))
    if len(result.points()) != 4 or len(result.prims()) != 2:
        raise AssertionError("Simplified round trip changed point or primitive count")

    vertex_count = (
        result.vertexCount()
        if hasattr(result, "vertexCount")
        else sum(len(primitive.vertices()) for primitive in result.prims())
    )
    if vertex_count != 6:
        raise AssertionError("Simplified round trip changed vertex count")

    expected_topology = ((0, 1, 2), (0, 2, 3))
    observed_topology = tuple(
        tuple(vertex.point().number() for vertex in primitive.vertices())
        for primitive in result.prims()
    )
    if observed_topology != expected_topology:
        raise AssertionError(
            f"Simplified round trip changed topology: {observed_topology}"
        )

    mask = result.findPointAttrib("mask")
    if mask is None or tuple(point.attribValue(mask) for point in result.points()) != (
        10,
        20,
        30,
        40,
    ):
        raise AssertionError("Simplified round trip lost point attributes")

    uv = result.findVertexAttrib("UV") or result.findVertexAttrib("uv")
    normal = result.findVertexAttrib("N")
    if uv is None or normal is None:
        raise AssertionError("Simplified round trip lost vertex attributes")
    vertices = tuple(
        vertex for primitive in result.prims() for vertex in primitive.vertices()
    )
    expected_uv = (
        (0.0, 0.0),
        (1.0, 0.0),
        (1.0, 1.0),
        (0.5, 0.5),
        (1.0, 1.0),
        (0.0, 1.0),
    )
    observed_uv = tuple(
        tuple(float(component) for component in vertex.attribValue(uv)[:2])
        for vertex in vertices
    )
    if observed_uv != expected_uv:
        raise AssertionError(
            f"Simplified round trip changed face-varying UVs: {observed_uv}"
        )
    if any(
        tuple(float(component) for component in vertex.attribValue(normal))
        != (0.0, 0.0, 1.0)
        for vertex in vertices
    ):
        raise AssertionError("Simplified round trip changed vertex normals")

    primitive_id = result.findPrimAttrib("id")
    if primitive_id is None or tuple(
        primitive.attribValue(primitive_id) for primitive in result.prims()
    ) != (7, 9):
        raise AssertionError("Simplified round trip lost primitive attributes")

    scale = result.findGlobalAttrib("scale")
    if scale is None or float(result.attribValue(scale)) != 2.5:
        raise AssertionError("Simplified round trip lost global attributes")


def main() -> None:
    """Run the Houdini-authored simplified-domain round trip."""
    arguments = parse_arguments()
    output_directory = arguments.output_directory.resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    source_path = output_directory / "source.bgeo"
    output_path = output_directory / "houio_simplified.bgeo"
    create_source().saveToFile(str(source_path))

    executable = Path(
        os.environ["HOUIO_SIMPLIFIED_ROUNDTRIP_EXECUTABLE"]
    ).resolve()
    completed = subprocess.run(
        [str(executable), str(source_path), str(output_path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            "HouIO simplified-domain round trip failed:\n"
            f"stdout:\n{completed.stdout}\n"
            f"stderr:\n{completed.stderr}"
        )
    validate_output(output_path)
    print(f"HouIO simplified-domain round trip passed: {output_path}")


if __name__ == "__main__":
    main()
