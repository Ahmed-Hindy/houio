"""Validate a geometry file by loading it through a Houdini File SOP."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import hou


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Geometry file to validate")
    parser.add_argument("--expect-points", type=int)
    parser.add_argument("--expect-primitives", type=int)
    return parser.parse_args()


def main() -> int:
    """Load and validate a geometry file.

    Returns:
        Process exit code.
    """
    args = parse_args()
    input_path = args.input.resolve()
    if not input_path.is_file():
        raise FileNotFoundError(input_path)

    hou.hipFile.clear(suppress_save_prompt=True)
    hou.setFrame(1)

    object_context = hou.node("/obj")
    if object_context is None:
        raise RuntimeError("Houdini object context /obj is unavailable.")

    geometry_container = object_context.createNode("geo", "validate_geometry", run_init_scripts=False)
    file_node = geometry_container.createNode("file", "input_geometry")
    file_node.parm("file").set(str(input_path))
    try:
        file_node.cook(force=True)
    except hou.OperationFailed as error:
        details = "\n".join(file_node.errors()) or str(error)
        warnings = "\n".join(file_node.warnings())
        if warnings:
            details = f"{details}\nWarnings:\n{warnings}"
        raise RuntimeError(f"Houdini failed to load {input_path}:\n{details}") from error

    geometry = file_node.geometry()
    point_count = len(geometry.points())
    primitive_count = len(geometry.prims())
    vertex_count = sum(len(primitive.vertices()) for primitive in geometry.prims())

    if point_count == 0 or primitive_count == 0:
        raise RuntimeError("Loaded geometry is empty.")
    if args.expect_points is not None and point_count != args.expect_points:
        raise RuntimeError(f"Expected {args.expect_points} points, found {point_count}.")
    if args.expect_primitives is not None and primitive_count != args.expect_primitives:
        raise RuntimeError(
            f"Expected {args.expect_primitives} primitives, found {primitive_count}."
        )
    if file_node.isTimeDependent():
        raise RuntimeError("The generated File SOP is unexpectedly time dependent.")

    summary = {
        "houdini_version": hou.applicationVersionString(),
        "input": str(input_path),
        "time_dependent": file_node.isTimeDependent(),
        "point_count": point_count,
        "vertex_count": vertex_count,
        "primitive_count": primitive_count,
        "point_attributes": sorted(attribute.name() for attribute in geometry.pointAttribs()),
        "vertex_attributes": sorted(attribute.name() for attribute in geometry.vertexAttribs()),
        "primitive_attributes": sorted(attribute.name() for attribute in geometry.primAttribs()),
        "global_attributes": sorted(attribute.name() for attribute in geometry.globalAttribs()),
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
