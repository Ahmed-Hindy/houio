"""Validate native HouIO Alembic and USD exports inside Houdini."""

from __future__ import annotations

import argparse
from pathlib import Path

import hou
from pxr import Usd, UsdGeom


def parse_arguments() -> argparse.Namespace:
    """Parse the output directory used by the scene-format test."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    return parser.parse_args()


def require_parm(node: hou.Node, name: str) -> hou.Parm:
    """Return a required parameter or fail with node context."""
    parm = node.parm(name)
    if parm is None:
        raise AssertionError(f"{node.path()} has no parameter {name!r}")
    return parm


def geometry_counts(geometry: hou.Geometry) -> tuple[int, int, int, int]:
    """Return point, vertex, closed-face, and open-curve counts."""
    closed_faces = 0
    open_curves = 0
    primitives = geometry.prims()
    for primitive in primitives:
        if isinstance(primitive, hou.Polygon):
            if primitive.isClosed():
                closed_faces += 1
            else:
                open_curves += 1
        elif primitive.type().name() == "PolySoup":
            face_counts = primitive.intrinsicValue("facecounts")
            closed_faces += len(face_counts)
    return (
        len(geometry.points()),
        sum(len(primitive.vertices()) for primitive in primitives),
        closed_faces,
        open_curves,
    )


def minimum_x(geometry: hou.Geometry) -> float:
    """Return the minimum point X coordinate."""
    return min(float(point.position()[0]) for point in geometry.points())


def build_source(container: hou.Node) -> hou.Node:
    """Build one animated polygon and polyline source."""
    for child in container.children():
        child.destroy()

    box = container.createNode("box", "closed_mesh")
    line = container.createNode("line", "open_polyline")
    require_parm(line, "points").set(4)
    require_parm(line, "originx").set(-1.5)
    require_parm(line, "originy").set(-1.0)
    require_parm(line, "originz").set(0.5)
    require_parm(line, "dirx").set(3.0)
    require_parm(line, "diry").set(0.5)
    require_parm(line, "dirz").set(1.0)

    merge = container.createNode("merge", "mesh_and_curve")
    merge.setInput(0, box)
    merge.setInput(1, line)

    transform = container.createNode("xform", "animated_transform")
    transform.setInput(0, merge)
    require_parm(transform, "tx").setExpression(
        "$F",
        language=hou.exprLanguage.Hscript,
    )
    transform.setDisplayFlag(True)
    transform.setRenderFlag(True)
    container.layoutChildren()
    return transform


def configure_rop(rop: hou.RopNode, source: hou.Node, output: Path) -> None:
    """Configure the native ROP for one archive output."""
    require_parm(rop, "soppath").set(source.path())
    require_parm(rop, "sopoutput").set(str(output))
    require_parm(rop, "createdirs").set(1)
    require_parm(rop, "overwrite").set(1)
    require_parm(rop, "atomic").set(1)


def validate_alembic(
    object_context: hou.Node,
    source: hou.Node,
    path: Path,
) -> None:
    """Reload the Alembic archive with Houdini's Alembic SOP."""
    reader_container = object_context.createNode("geo", "alembic_readback")
    for child in reader_container.children():
        child.destroy()
    reader = reader_container.createNode("alembic", "read_houio_alembic")
    require_parm(reader, "fileName").set(str(path))
    require_parm(reader, "loadmode").set("unpack")
    require_parm(reader, "frame").setExpression(
        "$FF",
        language=hou.exprLanguage.Hscript,
    )

    source_counts: list[tuple[int, int, int, int]] = []
    read_counts: list[tuple[int, int, int, int]] = []
    source_minimums: list[float] = []
    read_minimums: list[float] = []
    for frame in (1, 2):
        hou.setFrame(frame)
        source_geometry = source.geometry()
        read_geometry = reader.geometry()
        if reader.errors():
            raise AssertionError("Alembic reader errors: " + " | ".join(reader.errors()))
        source_counts.append(geometry_counts(source_geometry))
        read_counts.append(geometry_counts(read_geometry))
        source_minimums.append(minimum_x(source_geometry))
        read_minimums.append(minimum_x(read_geometry))

    if read_counts != source_counts:
        raise AssertionError(
            f"Alembic round trip changed geometry counts: {source_counts!r} -> {read_counts!r}"
        )
    if abs((read_minimums[1] - read_minimums[0]) - 1.0) > 1.0e-5:
        raise AssertionError("Alembic archive did not preserve per-frame animation")
    for expected, actual in zip(source_minimums, read_minimums):
        if abs(expected - actual) > 1.0e-5:
            raise AssertionError("Alembic readback positions differ from the SOP source")


def usd_points_at_time(stage: Usd.Stage, frame: float) -> list[tuple[float, float, float]]:
    """Collect mesh and curve points from a HouIO USD stage."""
    mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/HouIO/mesh"))
    curves = UsdGeom.BasisCurves(stage.GetPrimAtPath("/HouIO/curves"))
    if not mesh or not curves:
        raise AssertionError("USD archive is missing /HouIO/mesh or /HouIO/curves")
    time_code = Usd.TimeCode(frame)
    mesh_points = mesh.GetPointsAttr().Get(time_code)
    curve_points = curves.GetPointsAttr().Get(time_code)
    if mesh_points is None or curve_points is None:
        raise AssertionError("USD archive has no point sample at the requested frame")
    return [
        (float(point[0]), float(point[1]), float(point[2]))
        for point in tuple(mesh_points) + tuple(curve_points)
    ]


def validate_usd(path: Path) -> None:
    """Validate one USD archive with Houdini's bundled USD runtime."""
    stage = Usd.Stage.Open(str(path))
    if stage is None:
        raise AssertionError(f"USD could not open {path}")
    if stage.GetDefaultPrim().GetPath().pathString != "/HouIO":
        raise AssertionError("USD default prim is not /HouIO")
    if abs(float(UsdGeom.GetStageMetersPerUnit(stage)) - 1.0) > 1.0e-9:
        raise AssertionError("USD metersPerUnit is not 1.0")
    if UsdGeom.GetStageUpAxis(stage) != UsdGeom.Tokens.y:
        raise AssertionError("USD upAxis is not Y")
    if abs(float(stage.GetStartTimeCode()) - 1.0) > 1.0e-6:
        raise AssertionError("USD start time code is not frame 1")
    if abs(float(stage.GetEndTimeCode()) - 2.0) > 1.0e-6:
        raise AssertionError("USD end time code is not frame 2")

    mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/HouIO/mesh"))
    curves = UsdGeom.BasisCurves(stage.GetPrimAtPath("/HouIO/curves"))
    if list(mesh.GetFaceVertexCountsAttr().Get()) != [4, 4, 4, 4, 4, 4]:
        raise AssertionError("USD mesh face counts differ from the source box")
    if list(curves.GetCurveVertexCountsAttr().Get()) != [4]:
        raise AssertionError("USD curve counts differ from the source polyline")
    if curves.GetTypeAttr().Get() != UsdGeom.Tokens.linear:
        raise AssertionError("USD polyline was not written as linear basis curves")
    if curves.GetWrapAttr().Get() != UsdGeom.Tokens.nonperiodic:
        raise AssertionError("USD polyline was not written as nonperiodic")

    first = usd_points_at_time(stage, 1.0)
    second = usd_points_at_time(stage, 2.0)
    if len(first) != 12 or len(second) != 12:
        raise AssertionError("USD archive changed the source point count")
    first_minimum = min(point[0] for point in first)
    second_minimum = min(point[0] for point in second)
    if abs((second_minimum - first_minimum) - 1.0) > 1.0e-5:
        raise AssertionError("USD archive did not preserve per-frame animation")


def validate(output_directory: Path) -> None:
    """Render and validate Alembic and USD archives."""
    if hou.isApprentice():
        raise AssertionError("Scene-format validation requires a permitted Houdini license")
    output_directory.mkdir(parents=True, exist_ok=True)

    object_context = hou.node("/obj")
    output_context = hou.node("/out")
    if object_context is None or output_context is None:
        raise AssertionError("Houdini object or output context is unavailable")
    if "houio::geometry" not in hou.ropNodeTypeCategory().nodeTypes():
        raise AssertionError("The native houio::geometry ROP is not registered")

    source_container = object_context.createNode("geo", "native_scene_source")
    source = build_source(source_container)
    rop = output_context.createNode("houio::geometry", "native_scene_export")
    try:
        paths = {
            "abc": output_directory / "native_scene.abc",
            "usd": output_directory / "native_scene.usd",
            "usda": output_directory / "native_scene.usda",
            "usdc": output_directory / "native_scene.usdc",
        }
        for path in paths.values():
            temporary_path = path.with_name(
                path.stem + ".houio.tmp" + path.suffix
            )
            path.unlink(missing_ok=True)
            temporary_path.unlink(missing_ok=True)
            configure_rop(rop, source, path)
            rop.render(frame_range=(1, 2, 1), verbose=True, output_progress=True)
            if not path.is_file() or path.stat().st_size == 0:
                raise AssertionError(f"Native ROP did not produce {path.name}")
            if temporary_path.exists():
                raise AssertionError(f"Native ROP left a temporary file for {path.name}")

        validate_alembic(object_context, source, paths["abc"])
        validate_usd(paths["usd"])
        validate_usd(paths["usda"])
        validate_usd(paths["usdc"])
    finally:
        rop.destroy()
        source_container.destroy()


def main() -> int:
    """Run the native scene-format validation."""
    arguments = parse_arguments()
    validate(arguments.output_directory.resolve())
    print(
        "Native HouIO Alembic/USD exports passed in Houdini "
        + hou.applicationVersionString()
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
