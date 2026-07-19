"""Generate a minimal static polygon box fixture with Houdini."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import hou


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed command-line arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Output .geo or .bgeo path")
    return parser.parse_args()


def main() -> int:
    """Generate and save a polygon box.

    Returns:
        Process exit code.
    """
    args = parse_args()
    output_path = args.output.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    hou.hipFile.clear(suppress_save_prompt=True)
    hou.setFrame(1)

    object_context = hou.node("/obj")
    if object_context is None:
        raise RuntimeError("Houdini object context /obj is unavailable.")

    geometry_container = object_context.createNode("geo", "box_fixture", run_init_scripts=False)
    box_node = geometry_container.createNode("box", "box")
    box_node.cook(force=True)

    geometry = box_node.geometry()
    geometry.saveToFile(str(output_path))

    summary = {
        "houdini_version": hou.applicationVersionString(),
        "output": str(output_path),
        "time_dependent": box_node.isTimeDependent(),
        "point_count": len(geometry.points()),
        "vertex_count": sum(len(primitive.vertices()) for primitive in geometry.prims()),
        "primitive_count": len(geometry.prims()),
        "point_attributes": sorted(attribute.name() for attribute in geometry.pointAttribs()),
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
