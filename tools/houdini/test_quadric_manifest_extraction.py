"""Validate exact HOM extraction for native Sphere and Tube records."""

from __future__ import annotations

import math

import hou

from houio_hom.manifest import geometry_manifest


def build_source() -> tuple[hou.Geometry, hou.Prim, hou.Prim]:
    """Create transformed native sphere and capped tapered tube primitives."""
    hou.hipFile.clear(suppress_save_prompt=True)
    object_context = hou.node("/obj")
    if object_context is None:
        raise RuntimeError("Houdini object context is unavailable")
    container = object_context.createNode("geo", "houio_quadric_manifest")
    for child in container.children():
        child.destroy()
    try:
        sphere_node = container.createNode("sphere", "sphere")
        sphere_node.parm("type").set("prim")
        sphere_node.parmTuple("rad").set((2.0, 3.0, 4.0))
        sphere_node.parmTuple("t").set((1.0, 2.0, 3.0))
        sphere_node.parmTuple("r").set((10.0, 20.0, 30.0))
        sphere_node.cook(force=True)

        tube_node = container.createNode("tube", "tube")
        tube_node.parm("type").set("prim")
        tube_node.parm("cap").set(1)
        tube_node.parm("rad1").set(2.0)
        tube_node.parm("rad2").set(1.0)
        tube_node.parm("height").set(5.0)
        tube_node.parmTuple("t").set((-1.0, 0.5, 2.0))
        tube_node.parmTuple("r").set((5.0, 15.0, 25.0))
        tube_node.cook(force=True)

        geometry = hou.Geometry()
        geometry.merge(sphere_node.geometry())
        geometry.merge(tube_node.geometry())
    finally:
        container.destroy()

    sphere, tube = geometry.prims()
    return geometry, sphere, tube


def main() -> int:
    """Compare extracted values with Houdini's authoritative intrinsics."""
    geometry, source_sphere, source_tube = build_source()
    manifest = geometry_manifest(geometry)
    primitives = manifest["primitives"]
    if len(primitives) != 2:
        raise AssertionError(f"Unexpected primitive count: {primitives!r}")

    sphere, tube = primitives
    expected_sphere_transform = [
        float(value) for value in source_sphere.intrinsicValue("transform")
    ]
    if sphere != {
        "type": "sphere",
        "vertex_offset": 0,
        "transform": expected_sphere_transform,
    }:
        raise AssertionError(f"Unexpected sphere manifest: {sphere!r}")

    expected_tube_transform = [
        float(value) for value in source_tube.intrinsicValue("transform")
    ]
    expected_taper = float(source_tube.intrinsicValue("tubetaper"))
    if tube.get("type") != "tube" or tube.get("vertex_offset") != 1:
        raise AssertionError(f"Unexpected tube identity: {tube!r}")
    if tube.get("transform") != expected_tube_transform:
        raise AssertionError(f"Tube transform changed: {tube!r}")
    if tube.get("caps") is not True or not math.isclose(
        float(tube.get("taper")), expected_taper
    ):
        raise AssertionError(f"Tube cap or taper changed: {tube!r}")

    print(manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
