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


def create_source_geometry(output_directory: Path) -> hou.Geometry:
    """Create mixed geometry, including one external PackedDisk reference."""
    geometry = hou.Geometry()
    points = []
    for position in ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)):
        point = geometry.createPoint()
        point.setPosition(position)
        points.append(point)

    polygon = geometry.createPolygon()
    for point in points:
        polygon.addVertex(point)

    bezier = geometry.createBezierCurve(6, is_closed=True, order=4)
    for index, vertex in enumerate(bezier.vertices()):
        vertex.point().setPosition((float(index), 5.0 + float(index % 2), 0.0))
    weight = geometry.addAttrib(hou.attribType.Point, "Pw", 1.0)
    bezier.vertices()[2].point().setAttribValue(weight, 0.5)

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

    disk_payload = hou.Geometry()
    disk_points = []
    for position in ((0.0, 0.0, 0.0), (0.0, 2.0, 0.0)):
        point = disk_payload.createPoint()
        point.setPosition(position)
        disk_points.append(point)
    disk_line = disk_payload.createPolygon(is_closed=False)
    for point in disk_points:
        disk_line.addVertex(point)
    disk_payload_path = output_directory / "direct_writer_payload.bgeo"
    disk_payload.saveToFile(str(disk_payload_path))

    obj = hou.node("/obj")
    if obj is None:
        raise RuntimeError("Houdini object context is unavailable")
    container = obj.createNode("geo", "houio_direct_writer_packed_disk")
    for child in container.children():
        child.destroy()
    file_node = container.createNode("file", "packed_disk")
    file_node.parm("file").set(str(disk_payload_path))
    file_node.parm("loadtype").set("delayed")
    file_node.parm("packexpanded").set(1)
    file_node.parm("viewportlod").set("box")
    file_node.cook(force=True)
    geometry.merge(file_node.geometry())
    container.destroy()
    packed_disk = geometry.prims()[-1]
    packed_disk.setIntrinsicValue("pivot", (-0.25, 0.5, 1.0))

    for frame, x_position in enumerate((1.0, 2.0, 3.0), start=1):
        sequence_payload = hou.Geometry()
        point = sequence_payload.createPoint()
        point.setPosition((x_position, 0.0, 0.0))
        sequence_payload.saveToFile(
            str(output_directory / f"direct_sequence.{frame:04d}.bgeo")
        )
    sequence_container = obj.createNode("geo", "houio_direct_writer_sequence")
    for child in sequence_container.children():
        child.destroy()
    sequence_node = sequence_container.createNode("file", "packed_sequence")
    sequence_node.parm("file").set(str(output_directory / "direct_sequence.$F4.bgeo"))
    sequence_node.parm("loadtype").set("packedseq")
    sequence_node.parm("packexpanded").set(0)
    sequence_node.parm("f1").set(1)
    sequence_node.parm("f2").set(3)
    sequence_node.parm("index").set(1.5)
    sequence_node.parm("wrap").set("mirror")
    sequence_node.parm("viewportlod").set("box")
    sequence_node.cook(force=True)
    geometry.merge(sequence_node.geometry())
    sequence_container.destroy()
    packed_sequence = geometry.prims()[-1]
    packed_sequence.setIntrinsicValue("pivot", (0.5, -0.25, 1.25))
    packed_sequence.setIntrinsicValue("index", 1.5)
    packed_sequence.setIntrinsicValue("wrap", "mirror")

    quadric_container = obj.createNode("geo", "houio_direct_writer_quadrics")
    for child in quadric_container.children():
        child.destroy()
    sphere_node = quadric_container.createNode("sphere", "sphere")
    sphere_node.parm("type").set("prim")
    sphere_node.parmTuple("rad").set((2.0, 3.0, 4.0))
    sphere_node.parmTuple("t").set((1.0, 2.0, 3.0))
    sphere_node.cook(force=True)
    geometry.merge(sphere_node.geometry())
    tube_node = quadric_container.createNode("tube", "tube")
    tube_node.parm("type").set("prim")
    tube_node.parm("cap").set(1)
    tube_node.parm("rad1").set(2.0)
    tube_node.parm("rad2").set(1.0)
    tube_node.parm("height").set(5.0)
    tube_node.parmTuple("t").set((-1.0, 0.5, 2.0))
    tube_node.cook(force=True)
    geometry.merge(tube_node.geometry())
    quadric_container.destroy()
    sphere = geometry.prims()[-2]
    tube = geometry.prims()[-1]

    label = geometry.addAttrib(hou.attribType.Point, "label", "")
    for point in geometry.points():
        point.setAttribValue(label, f"point_{point.number()}")

    uv = geometry.addAttrib(hou.attribType.Vertex, "uv", (0.0, 0.0))
    for vertex in polygon.vertices():
        vertex.setAttribValue(uv, (float(vertex.number()), 0.5))

    primitive_kind = geometry.addAttrib(hou.attribType.Prim, "kind", "")
    polygon.setAttribValue(primitive_kind, "polygon")
    bezier.setAttribValue(primitive_kind, "bezier_curve")
    volume.setAttribValue(primitive_kind, "volume")
    packed.setAttribValue(primitive_kind, "packed")
    packed_disk.setAttribValue(primitive_kind, "packed_disk")
    packed_sequence.setAttribValue(primitive_kind, "packed_sequence")
    sphere.setAttribValue(primitive_kind, "sphere")
    tube.setAttribValue(primitive_kind, "tube")
    geometry.setGlobalAttribValue(
        geometry.addAttrib(hou.attribType.Global, "asset", ""),
        "direct_writer",
    )

    geometry.createPointGroup("selected_points").add(points[0])
    geometry.createVertexGroup("selected_vertices").add(polygon.vertices()[1])
    geometry.createPrimGroup("curves").add(bezier)
    geometry.createPrimGroup("volumes").add(volume)
    geometry.createPrimGroup("packed_disks").add(packed_disk)
    geometry.createPrimGroup("packed_sequences").add(packed_sequence)
    geometry.createPrimGroup("quadrics").add((sphere, tube))
    return geometry


def validate_output(path: Path) -> None:
    """Use Houdini's reader to verify the file produced by HouIO."""
    result = hou.Geometry()
    result.loadFromFile(str(path))
    if len(result.points()) != 15:
        raise AssertionError("Direct writer did not preserve point count")
    vertex_count = (
        result.vertexCount()
        if hasattr(result, "vertexCount")
        else sum(len(primitive.vertices()) for primitive in result.prims())
    )
    if vertex_count != 15 or len(result.prims()) != 8:
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
    if (
        result.findPrimGroup("curves") is None
        or result.findPrimGroup("volumes") is None
        or result.findPrimGroup("packed_disks") is None
        or result.findPrimGroup("packed_sequences") is None
        or result.findPrimGroup("quadrics") is None
    ):
        raise AssertionError("Direct writer lost primitive groups")

    curves = [
        primitive
        for primitive in result.prims()
        if primitive.type().name() in {"NURBSCurve", "BezierCurve"}
    ]
    if [primitive.type().name() for primitive in curves] != ["BezierCurve"]:
        raise AssertionError("Direct writer did not preserve the Bezier record")
    bezier_result = curves[0]
    if (
        int(bezier_result.intrinsicValue("order")) != 4
        or tuple(float(value) for value in bezier_result.intrinsicValue("knots"))
        != (0.0, 1.0, 2.0)
        or not bezier_result.isClosed()
        or int(bezier_result.intrinsicValue("rational")) != 1
    ):
        raise AssertionError("Direct writer changed Bezier basis metadata")
    weights = result.findPointAttrib("Pw")
    if weights is None or not math.isclose(
        float(bezier_result.vertices()[2].point().attribValue(weights)), 0.5
    ):
        raise AssertionError("Direct writer changed rational curve weights")

    quadrics = [
        primitive
        for primitive in result.prims()
        if primitive.type().name() in {"Sphere", "Tube"}
    ]
    if [primitive.type().name() for primitive in quadrics] != ["Sphere", "Tube"]:
        raise AssertionError("Direct writer did not preserve native quadric records")
    sphere_result, tube_result = quadrics
    if tuple(float(value) for value in sphere_result.intrinsicValue("transform")) != (
        2.0, 0.0, 0.0, 0.0, 0.0, -4.0, 0.0, 3.0, 0.0
    ):
        raise AssertionError("Direct writer changed sphere transform")
    if (
        tuple(float(value) for value in tube_result.intrinsicValue("transform"))
        != (1.0, 0.0, 0.0, 0.0, 5.0, 0.0, 0.0, 0.0, 1.0)
        or int(tube_result.intrinsicValue("closed")) != 1
        or not math.isclose(float(tube_result.intrinsicValue("tubetaper")), 2.0)
    ):
        raise AssertionError("Direct writer changed tube transform, caps, or taper")

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

    packed_disks = [
        primitive
        for primitive in result.prims()
        if "packedtypename" in primitive.intrinsicNames()
        and str(primitive.intrinsicValue("packedtypename")) == "PackedDisk"
    ]
    if len(packed_disks) != 1:
        raise AssertionError("Direct writer did not preserve packed disk")
    packed_disk = packed_disks[0]
    if str(packed_disk.intrinsicValue("unexpandedfilename")) != str(
        path.parent / "direct_writer_payload.bgeo"
    ):
        raise AssertionError("Direct writer changed packed disk filename")
    if str(packed_disk.intrinsicValue("viewportlod")) != "box":
        raise AssertionError("Direct writer changed packed disk viewport metadata")

    packed_sequences = [
        primitive
        for primitive in result.prims()
        if "packedtypename" in primitive.intrinsicNames()
        and str(primitive.intrinsicValue("packedtypename")) == "PackedDiskSequence"
    ]
    if len(packed_sequences) != 1:
        raise AssertionError("Direct writer did not preserve packed disk sequence")
    packed_sequence = packed_sequences[0]
    expected_sequence_filenames = [
        str(path.parent / f"direct_sequence.{frame:04d}.bgeo")
        for frame in range(1, 4)
    ]
    actual_sequence_filenames = [
        str(filename) for filename in packed_sequence.intrinsicValue("filenames")
    ]
    if actual_sequence_filenames != expected_sequence_filenames:
        raise AssertionError("Direct writer changed packed sequence samples")
    if str(packed_sequence.intrinsicValue("wrap")) != "mirror":
        raise AssertionError("Direct writer changed packed sequence wrap mode")
    if not math.isclose(float(packed_sequence.intrinsicValue("index")), 1.5):
        raise AssertionError("Direct writer changed packed sequence index")

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
    write_result = write_geometry(
        create_source_geometry(arguments.output_directory), output_path
    )
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
