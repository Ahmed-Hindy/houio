"""Validate direct HOM extraction through HouIO's custom writer."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import hou

from houio_hom import write_geometry


def parse_arguments() -> argparse.Namespace:
    """Parse the test output directory."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    return parser.parse_args()


def create_source_geometry() -> hou.Geometry:
    """Create mixed polygon and dense-volume geometry with maintained domains."""
    geometry = hou.Geometry()
    points = []
    for position in ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)):
        point = geometry.createPoint()
        point.setPosition(position)
        points.append(point)

    polygon = geometry.createPolygon()
    for point in points:
        polygon.addVertex(point)

    volume = geometry.createVolume(2, 1, 1)
    volume.setVoxel((0, 0, 0), 1.25)
    volume.setVoxel((1, 0, 0), -3.5)
    volume.setIntrinsicValue("volumevisualmode", "smoke")
    volume.setIntrinsicValue("volumevisualdensity", 1.75)

    embedded = hou.Geometry()
    embedded_points = []
    for position in ((0.0, 0.0, 0.0), (2.0, 0.0, 0.0)):
        point = embedded.createPoint()
        point.setPosition(position)
        embedded_points.append(point)
    embedded_line = embedded.createPolygon(is_closed=False)
    for point in embedded_points:
        embedded_line.addVertex(point)
    packed = geometry.createPackedGeometry(embedded)
    packed.setIntrinsicValue("pivot", (0.25, 0.5, 0.75))

    label = geometry.addAttrib(hou.attribType.Point, "label", "")
    for point in geometry.points():
        point.setAttribValue(label, f"point_{point.number()}")

    uv = geometry.addAttrib(hou.attribType.Vertex, "uv", (0.0, 0.0))
    for vertex in polygon.vertices():
        vertex.setAttribValue(uv, (float(vertex.number()), 0.5))

    primitive_kind = geometry.addAttrib(hou.attribType.Prim, "kind", "")
    polygon.setAttribValue(primitive_kind, "polygon")
    volume.setAttribValue(primitive_kind, "volume")
    packed.setAttribValue(primitive_kind, "packed")
    geometry.setGlobalAttribValue(
        geometry.addAttrib(hou.attribType.Global, "asset", ""),
        "direct_writer",
    )

    geometry.createPointGroup("selected_points").add(points[0])
    geometry.createVertexGroup("selected_vertices").add(polygon.vertices()[1])
    geometry.createPrimGroup("volumes").add(volume)
    return geometry


def validate_output(path: Path) -> None:
    """Use Houdini's reader to verify the file produced by HouIO."""
    result = hou.Geometry()
    result.loadFromFile(str(path))
    if len(result.points()) != 5:
        raise AssertionError("Direct writer did not preserve point count")
    vertex_count = (
        result.vertexCount()
        if hasattr(result, "vertexCount")
        else sum(len(primitive.vertices()) for primitive in result.prims())
    )
    if vertex_count != 5 or len(result.prims()) != 3:
        raise AssertionError("Direct writer did not preserve topology or primitives")
    if result.findPointAttrib("label") is None:
        raise AssertionError("Direct writer lost point string attributes")
    if result.findVertexAttrib("uv") is None:
        raise AssertionError("Direct writer lost vertex attributes")
    if result.findPrimAttrib("kind") is None:
        raise AssertionError("Direct writer lost primitive attributes")
    if result.findGlobalAttrib("asset") is None:
        raise AssertionError("Direct writer lost global attributes")
    if result.findPointGroup("selected_points") is None:
        raise AssertionError("Direct writer lost point groups")
    if result.findVertexGroup("selected_vertices") is None:
        raise AssertionError("Direct writer lost vertex groups")
    if result.findPrimGroup("volumes") is None:
        raise AssertionError("Direct writer lost primitive groups")

    packed_primitives = [
        primitive
        for primitive in result.prims()
        if isinstance(primitive, hou.PackedGeometry)
    ]
    if len(packed_primitives) != 1:
        raise AssertionError("Direct writer did not preserve packed geometry")
    packed = packed_primitives[0]
    embedded = packed.getEmbeddedGeometry()
    if len(embedded.points()) != 2 or len(embedded.prims()) != 1:
        raise AssertionError("Direct writer changed the embedded packed payload")
    if tuple(float(value) for value in packed.intrinsicValue("pivot")) != (
        0.25,
        0.5,
        0.75,
    ):
        raise AssertionError("Direct writer changed packed pivot metadata")

    volumes = [primitive for primitive in result.prims() if isinstance(primitive, hou.Volume)]
    if len(volumes) != 1:
        raise AssertionError("Direct writer did not preserve the dense volume")
    values = tuple(float(value) for value in volumes[0].allVoxels())
    if len(values) != 2 or not math.isclose(values[0], 1.25) or not math.isclose(values[1], -3.5):
        raise AssertionError(f"Direct writer changed dense-volume voxels: {values}")


def main() -> int:
    """Run the direct custom-writer integration test."""
    arguments = parse_arguments()
    arguments.output_directory.mkdir(parents=True, exist_ok=True)
    output_path = arguments.output_directory / "direct_writer.bgeo"
    write_result = write_geometry(create_source_geometry(), output_path)
    if not write_result.success:
        raise AssertionError(
            "HouIO direct writer failed:\n"
            + write_result.stderr
            + "\n"
            + write_result.stdout
        )
    validate_output(output_path)
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
