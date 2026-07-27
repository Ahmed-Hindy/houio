"""Validate exact HOM extraction boundaries for curve records."""

from __future__ import annotations

import hou

from houio_hom.manifest import UnsupportedHOMDataError, geometry_manifest


def build_bezier_source() -> hou.Geometry:
    """Create one representative rational closed Bezier curve."""
    geometry = hou.Geometry()
    bezier = geometry.createBezierCurve(6, is_closed=True, order=4)
    for index, vertex in enumerate(bezier.vertices()):
        vertex.point().setPosition((float(index), 3.0 + float(index % 2), 0.0))
    weight = geometry.addAttrib(hou.attribType.Point, "Pw", 1.0)
    bezier.vertices()[2].point().setAttribValue(weight, 0.5)
    return geometry


def build_nurbs_source() -> hou.Geometry:
    """Create a NURBS whose serialized endpoint policy HOM cannot expose."""
    geometry = hou.Geometry()
    nurbs = geometry.createNURBSCurve(5, is_closed=False, order=4)
    for index, vertex in enumerate(nurbs.vertices()):
        vertex.point().setPosition((float(index), float(index % 2), 0.0))
    return geometry


def main() -> None:
    """Check exact Bezier extraction and conservative NURBS rejection."""
    manifest = geometry_manifest(build_bezier_source())
    primitives = manifest["primitives"]
    if primitives != [
        {
            "type": "bezier_curve",
            "vertex_offset": 0,
            "vertex_count": 6,
            "closed": True,
            "order": 4,
            "knots": [0.0, 1.0, 2.0],
        }
    ]:
        raise AssertionError(f"Unexpected Bezier manifest: {primitives!r}")

    point_attributes = manifest["attributes"]["point"]
    weights = next(
        (attribute for attribute in point_attributes if attribute["name"] == "Pw"),
        None,
    )
    if weights is None or weights["values"][2] != 0.5:
        raise AssertionError("Curve manifest did not preserve rational Pw weights")

    try:
        geometry_manifest(build_nurbs_source())
    except UnsupportedHOMDataError as error:
        if "endinterpolation" not in str(error):
            raise AssertionError(
                f"NURBS rejection did not identify the ambiguous policy: {error}"
            ) from error
    else:
        raise AssertionError("Direct HOM extraction accepted ambiguous NURBS metadata")


if __name__ == "__main__":
    main()
