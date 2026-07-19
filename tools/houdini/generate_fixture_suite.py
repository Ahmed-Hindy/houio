"""Generate minimal Houdini 21/22 geometry compatibility fixtures."""

from __future__ import annotations

import argparse
import json
from collections.abc import Callable, Sequence
from pathlib import Path

import hou

FixtureBuilder = Callable[[], hou.Geometry]
Position = tuple[float, float, float]


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed command-line arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output_directory", type=Path, help="Directory for generated .bgeo files")
    return parser.parse_args()


def create_points(geometry: hou.Geometry, positions: Sequence[Position]) -> list[hou.Point]:
    """Create points with deterministic positions.

    Args:
        geometry: Geometry receiving the points.
        positions: Point positions in creation order.

    Returns:
        Created Houdini points.
    """
    points = []
    for position in positions:
        point = geometry.createPoint()
        point.setPosition(position)
        points.append(point)
    return points


def create_polygon(geometry: hou.Geometry, points: Sequence[hou.Point], indices: Sequence[int], closed: bool = True) -> hou.Polygon:
    """Create one polygon from point indices.

    Args:
        geometry: Geometry receiving the polygon.
        points: Available points.
        indices: Point indices used by the polygon.
        closed: Whether the polygon is closed.

    Returns:
        Created polygon.
    """
    polygon = geometry.createPolygon(is_closed=closed)
    for point_index in indices:
        polygon.addVertex(points[point_index])
    return polygon


def add_numeric_point_attribute(
    geometry: hou.Geometry, name: str, type_index: int, precision_index: int, values: Sequence[int | float]
) -> hou.Geometry:
    """Add a point attribute with an explicit Houdini numeric precision.

    Args:
        geometry: Source geometry.
        name: Attribute name.
        type_index: Attribute Create SOP type menu index.
        precision_index: Attribute Create SOP precision menu index.
        values: Scalar values in point order.

    Returns:
        Geometry containing the explicitly typed attribute.
    """
    if len(values) != len(geometry.points()):
        raise ValueError(f"{name}: value count does not match point count")

    verb = hou.sopNodeTypeCategory().nodeVerb("attribcreate::2.0")
    if verb is None:
        raise RuntimeError("Houdini does not provide the attribcreate::2.0 SOP verb")
    attribute_parameters = verb.parms()["numattr"][0].copy()
    attribute_parameters.update(
        {
            "name#": name,
            "class#": 2,
            "type#": type_index,
            "precision#": precision_index,
            "size#": 1,
            "value#v": hou.Vector4(float(values[0]), 0.0, 0.0, 0.0),
        }
    )
    verb.setParms({"numattr": (attribute_parameters,)})
    result = hou.Geometry()
    verb.execute(result, [geometry])
    attribute = result.findPointAttrib(name)
    if attribute is None:
        raise RuntimeError(f"Houdini did not create point attribute {name}")
    for point, value in zip(result.points(), values):
        point.setAttribValue(attribute, value)
    return result


def build_empty_geometry() -> hou.Geometry:
    """Build geometry with no elements.

    Returns:
        Empty geometry.
    """
    return hou.Geometry()


def build_point_attributes_geometry() -> hou.Geometry:
    """Build points carrying representative numeric and string attributes.

    Returns:
        Point-only geometry.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        ((0.0, 0.0, 0.0), (1.0, 0.5, -1.0), (-2.0, 1.5, 3.0), (4.0, -0.25, 2.0)),
    )

    color_attribute = geometry.addAttrib(hou.attribType.Point, "Cd", (0.0, 0.0, 0.0))
    weight_attribute = geometry.addAttrib(hou.attribType.Point, "weight", 0.0)
    identifier_attribute = geometry.addAttrib(hou.attribType.Point, "id", 0)
    label_attribute = geometry.addAttrib(hou.attribType.Point, "label", "")

    colors = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (0.25, 0.5, 0.75))
    weights = (0.0, 0.25, 0.5, 1.0)
    identifiers = (7, 11, 42, 99)
    labels = ("origin", "green", "blue", "last")
    for point, color, weight, identifier, label in zip(
        points, colors, weights, identifiers, labels
    ):
        point.setAttribValue(color_attribute, color)
        point.setAttribValue(weight_attribute, weight)
        point.setAttribValue(identifier_attribute, identifier)
        point.setAttribValue(label_attribute, label)

    return geometry


def build_numeric_precision_geometry() -> hou.Geometry:
    """Build point attributes using explicit 64-bit integer storage.

    Returns:
        Point geometry with values outside the signed 32-bit range.
    """
    geometry = hou.Geometry()
    create_points(geometry, ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (2.0, 0.0, 0.0)))
    geometry = add_numeric_point_attribute(
        geometry,
        "large_id",
        type_index=1,
        precision_index=3,
        values=(1099511627776, -1099511627777, 4294967299),
    )
    return add_numeric_point_attribute(
        geometry,
        "half_value",
        type_index=0,
        precision_index=1,
        values=(0.5, -2.0, 3.25),
    )


def build_triangles_geometry() -> hou.Geometry:
    """Build two closed triangles.

    Returns:
        Triangle-only geometry.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0)),
    )
    create_polygon(geometry, points, (0, 1, 2))
    create_polygon(geometry, points, (0, 2, 3))
    return geometry


def build_quads_geometry() -> hou.Geometry:
    """Build two closed quads.

    Returns:
        Quad-only geometry.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        (
            (0.0, 0.0, 0.0),
            (1.0, 0.0, 0.0),
            (2.0, 0.0, 0.0),
            (0.0, 1.0, 0.0),
            (1.0, 1.0, 0.0),
            (2.0, 1.0, 0.0),
        ),
    )
    create_polygon(geometry, points, (0, 1, 4, 3))
    create_polygon(geometry, points, (1, 2, 5, 4))
    return geometry


def build_mixed_polygons_geometry() -> hou.Geometry:
    """Build adjacent triangle, quad, and pentagon primitives.

    Returns:
        Mixed polygon geometry.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        (
            (0.0, 0.0, 0.0),
            (1.0, 0.0, 0.0),
            (0.5, 1.0, 0.0),
            (2.0, 0.0, 0.0),
            (3.0, 0.0, 0.0),
            (3.0, 1.0, 0.0),
            (2.0, 1.0, 0.0),
            (4.0, 0.0, 0.0),
            (5.0, 0.0, 0.0),
            (5.5, 0.75, 0.0),
            (4.5, 1.5, 0.0),
            (3.75, 0.75, 0.0),
        ),
    )
    create_polygon(geometry, points, (0, 1, 2))
    create_polygon(geometry, points, (3, 4, 5, 6))
    create_polygon(geometry, points, (7, 8, 9, 10, 11))
    return geometry


def build_ngon_geometry() -> hou.Geometry:
    """Build one closed five-sided polygon.

    Returns:
        N-gon geometry.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        ((0.0, 0.0, 0.0), (1.0, -0.25, 0.0), (1.5, 0.75, 0.0), (0.5, 1.5, 0.0), (-0.5, 0.75, 0.0)),
    )
    create_polygon(geometry, points, (0, 1, 2, 3, 4))
    return geometry


def build_open_polygon_geometry() -> hou.Geometry:
    """Build one open four-vertex polygon.

    Returns:
        Open polygon geometry.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        ((0.0, 0.0, 0.0), (1.0, 0.5, 0.0), (2.0, 0.0, 0.0), (3.0, 1.0, 0.0)),
    )
    create_polygon(geometry, points, (0, 1, 2, 3), closed=False)
    return geometry


def build_uv_seam_geometry() -> hou.Geometry:
    """Build two triangles with different UVs on shared points.

    Returns:
        Geometry containing a vertex-domain UV seam.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0)),
    )
    first_polygon = create_polygon(geometry, points, (0, 1, 2))
    second_polygon = create_polygon(geometry, points, (0, 2, 3))
    uv_attribute = geometry.addAttrib(hou.attribType.Vertex, "uv", (0.0, 0.0, 0.0))

    first_uvs = ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 1.0, 0.0))
    second_uvs = ((0.5, 0.0, 0.0), (0.5, 1.0, 0.0), (0.0, 1.0, 0.0))
    for vertex, uv_value in zip(first_polygon.vertices(), first_uvs):
        vertex.setAttribValue(uv_attribute, uv_value)
    for vertex, uv_value in zip(second_polygon.vertices(), second_uvs):
        vertex.setAttribValue(uv_attribute, uv_value)

    return geometry


def build_multiple_polygon_runs_geometry() -> hou.Geometry:
    """Build separate closed and open polygon runs in one geometry.

    Returns:
        Geometry containing two polygon-run primitive records.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        (
            (0.0, 0.0, 0.0),
            (1.0, 0.0, 0.0),
            (0.5, 1.0, 0.0),
            (2.0, 0.0, 0.0),
            (3.0, 0.5, 0.0),
            (4.0, 0.0, 0.0),
            (5.0, 1.0, 0.0),
        ),
    )
    create_polygon(geometry, points, (0, 1, 2), closed=True)
    create_polygon(geometry, points, (3, 4, 5, 6), closed=False)
    return geometry


def build_global_attributes_geometry() -> hou.Geometry:
    """Build geometry carrying numeric and string global attributes.

    Returns:
        Geometry with representative global attributes.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry, ((0.0, 0.0, 0.0), (1.0, 0.5, 0.0), (2.0, 0.0, 0.0))
    )
    create_polygon(geometry, points, (0, 1, 2), closed=False)

    geometry.addAttrib(hou.attribType.Global, "fixture_name", "")
    geometry.addAttrib(hou.attribType.Global, "version", 0)
    geometry.addAttrib(hou.attribType.Global, "scale", 0.0)
    geometry.addAttrib(hou.attribType.Global, "offset", (0.0, 0.0, 0.0))
    geometry.setGlobalAttribValue("fixture_name", "global_attributes")
    geometry.setGlobalAttribValue("version", 22)
    geometry.setGlobalAttribValue("scale", 1.25)
    geometry.setGlobalAttribValue("offset", (1.0, -2.0, 3.5))
    return geometry


def build_dense_volume_geometry() -> hou.Geometry:
    """Build a dense scalar volume spanning multiple x-axis tiles.

    Returns:
        Dense volume geometry with deterministic values and transform.
    """
    geometry = hou.Geometry()
    bounding_box = hou.BoundingBox(-2.0, -1.0, 3.0, 6.0, 5.0, 7.0)
    volume = geometry.createVolume(17, 2, 1, bounding_box)
    volume.setAllVoxels(tuple(float(x + y * 100) for y in range(2) for x in range(17)))
    return geometry


def build_primitive_groups_geometry() -> hou.Geometry:
    """Build overlapping point, vertex, and primitive groups.

    Returns:
        Geometry with all supported group domains and primitive attributes.
    """
    geometry = hou.Geometry()
    points = create_points(
        geometry,
        ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0)),
    )
    left_polygon = create_polygon(geometry, points, (0, 1, 2))
    right_polygon = create_polygon(geometry, points, (0, 2, 3))

    left_points_group = geometry.createPointGroup("left_points")
    shared_points_group = geometry.createPointGroup("shared_points")
    for point_index in (0, 3):
        left_points_group.add(points[point_index])
    for point_index in (0, 2):
        shared_points_group.add(points[point_index])

    first_face_vertices_group = geometry.createVertexGroup("first_face_vertices")
    shared_point_vertices_group = geometry.createVertexGroup("shared_point_vertices")
    for vertex in left_polygon.vertices():
        first_face_vertices_group.add(vertex)
    for primitive in geometry.prims():
        for vertex in primitive.vertices():
            if vertex.point().number() in (0, 2):
                shared_point_vertices_group.add(vertex)

    left_group = geometry.createPrimGroup("left")
    right_group = geometry.createPrimGroup("right")
    left_group.add(left_polygon)
    right_group.add(right_polygon)

    name_attribute = geometry.addAttrib(hou.attribType.Prim, "name", "")
    piece_attribute = geometry.addAttrib(hou.attribType.Prim, "piece", 0)
    left_polygon.setAttribValue(name_attribute, "left_piece")
    right_polygon.setAttribValue(name_attribute, "right_piece")
    left_polygon.setAttribValue(piece_attribute, 1)
    right_polygon.setAttribValue(piece_attribute, 2)
    return geometry


def geometry_summary(geometry: hou.Geometry) -> dict[str, object]:
    """Create a deterministic summary of generated geometry.

    Args:
        geometry: Geometry to summarize.

    Returns:
        Serializable geometry summary.
    """
    return {
        "point_count": len(geometry.points()),
        "vertex_count": sum(len(primitive.vertices()) for primitive in geometry.prims()),
        "primitive_count": len(geometry.prims()),
        "closed": [
            primitive.isClosed() if isinstance(primitive, hou.Polygon) else None
            for primitive in geometry.prims()
        ],
        "point_attributes": sorted(attribute.name() for attribute in geometry.pointAttribs()),
        "vertex_attributes": sorted(attribute.name() for attribute in geometry.vertexAttribs()),
        "primitive_attributes": sorted(attribute.name() for attribute in geometry.primAttribs()),
        "global_attributes": sorted(attribute.name() for attribute in geometry.globalAttribs()),
        "point_groups": sorted(group.name() for group in geometry.pointGroups()),
        "vertex_groups": sorted(group.name() for group in geometry.vertexGroups()),
        "primitive_groups": sorted(group.name() for group in geometry.primGroups()),
    }


def main() -> int:
    """Generate the fixture suite and its manifest.

    Returns:
        Process exit code.
    """
    args = parse_args()
    output_directory = args.output_directory.resolve()
    output_directory.mkdir(parents=True, exist_ok=True)

    hou.hipFile.clear(suppress_save_prompt=True)
    hou.setFrame(1)

    fixtures: tuple[tuple[str, FixtureBuilder, tuple[str, ...]], ...] = (
        ("empty", build_empty_geometry, ()),
        ("point_attributes", build_point_attributes_geometry, ()),
        ("numeric_precision", build_numeric_precision_geometry, ()),
        ("triangles", build_triangles_geometry, ()),
        ("quads", build_quads_geometry, ()),
        ("mixed_polygons", build_mixed_polygons_geometry, ()),
        ("ngon", build_ngon_geometry, ()),
        ("open_polygon", build_open_polygon_geometry, ()),
        ("uv_seam", build_uv_seam_geometry, ()),
        ("multiple_polygon_runs", build_multiple_polygon_runs_geometry, ()),
        ("global_attributes", build_global_attributes_geometry, ()),
        ("dense_volume", build_dense_volume_geometry, ()),
        ("primitive_groups", build_primitive_groups_geometry, ()),
    )

    manifest_entries = []
    for fixture_name, fixture_builder, known_losses in fixtures:
        geometry = fixture_builder()
        fixture_path = output_directory / f"{fixture_name}.bgeo"
        geometry.saveToFile(str(fixture_path))
        manifest_entries.append(
            {
                "name": fixture_name,
                "file": fixture_path.name,
                "known_losses": list(known_losses),
                "source": geometry_summary(geometry),
            }
        )

    manifest = {
        "houdini_version": hou.applicationVersionString(),
        "time_dependent": False,
        "fixtures": manifest_entries,
    }
    manifest_path = output_directory / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
