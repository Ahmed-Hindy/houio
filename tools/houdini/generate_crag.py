"""Generate a static Crag test geometry file with Houdini.

Run this script with a Houdini hython executable. The output contains only the
cooked geometry at frame 1, so it has no animation dependency.
"""

from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path
from typing import Any

import hou


def find_crag_node_type() -> str:
    """Return the installed Crag SOP node type name.

    Returns:
        The preferred Crag SOP type name.

    Raises:
        RuntimeError: If no Crag SOP type is installed.
    """
    node_types = hou.sopNodeTypeCategory().nodeTypes()
    candidates = sorted(name for name in node_types if "crag" in name.lower())
    if not candidates:
        raise RuntimeError("No SOP node type containing 'crag' is installed.")

    preferred_names = ("testgeometry_crag", "testgeometry_crag::2.0")
    for preferred_name in preferred_names:
        if preferred_name in node_types:
            return preferred_name

    return candidates[-1]


def set_menu_item(parm: hou.Parm, preferred_labels: tuple[str, ...]) -> bool:
    """Set a menu parameter by matching one of several labels.

    Args:
        parm: Houdini parameter to inspect and modify.
        preferred_labels: Case-insensitive menu labels to match.

    Returns:
        True when a matching menu item was selected.
    """
    labels = tuple(label.casefold() for label in parm.menuLabels())
    for preferred_label in preferred_labels:
        preferred = preferred_label.casefold()
        for index, label in enumerate(labels):
            if preferred in label:
                parm.set(parm.menuItems()[index])
                return True
    return False


def configure_static_tpose(node: hou.SopNode) -> dict[str, Any]:
    """Disable animation controls and select Crag's rest T-pose.

    Args:
        node: Crag SOP node.

    Returns:
        A report of parameters changed by the configuration pass.
    """
    changes: dict[str, Any] = {}

    rest_pose_parm = node.parm("restpose")
    if rest_pose_parm is not None:
        rest_pose_parm.deleteAllKeyframes()
        rest_pose_parm.set(1)
        changes[rest_pose_parm.name()] = rest_pose_parm.eval()

    frame_parm = node.parm("frame")
    if frame_parm is not None:
        frame_parm.deleteAllKeyframes()
        frame_parm.set(1.0)
        changes[frame_parm.name()] = frame_parm.eval()

    for parm in node.parms():
        name = parm.name().casefold()
        label = parm.parmTemplate().label().casefold()
        descriptor = f"{name} {label}"

        if "pose" in descriptor and parm.parmTemplate().type() == hou.parmTemplateType.Menu:
            if set_menu_item(parm, ("t-pose", "t pose", "rest")):
                changes[parm.name()] = parm.evalAsString()
                continue

        animation_tokens = ("animate", "animation", "motion", "walk", "run")
        if any(token in descriptor for token in animation_tokens):
            template_type = parm.parmTemplate().type()
            if template_type in (hou.parmTemplateType.Toggle, hou.parmTemplateType.Int):
                parm.deleteAllKeyframes()
                parm.set(0)
                changes[parm.name()] = parm.eval()

    return changes


def geometry_summary(node: hou.SopNode, output_path: Path, changes: dict[str, Any]) -> dict[str, Any]:
    """Build a JSON-serializable summary of generated geometry.

    Args:
        node: Cooked Crag SOP node.
        output_path: Saved geometry path.
        changes: Parameter changes applied before cooking.

    Returns:
        Geometry metadata suitable for logging and validation.
    """
    geometry = node.geometry()
    vertex_count = sum(len(primitive.vertices()) for primitive in geometry.prims())
    primitive_types = collections.Counter(primitive.type().name() for primitive in geometry.prims())

    return {
        "houdini_version": hou.applicationVersionString(),
        "node_type": node.type().nameWithCategory(),
        "frame": hou.frame(),
        "time_dependent": node.isTimeDependent(),
        "output": str(output_path),
        "point_count": len(geometry.points()),
        "vertex_count": vertex_count,
        "primitive_count": len(geometry.prims()),
        "primitive_types": dict(sorted(primitive_types.items())),
        "point_attributes": sorted(attribute.name() for attribute in geometry.pointAttribs()),
        "vertex_attributes": sorted(attribute.name() for attribute in geometry.vertexAttribs()),
        "primitive_attributes": sorted(attribute.name() for attribute in geometry.primAttribs()),
        "global_attributes": sorted(attribute.name() for attribute in geometry.globalAttribs()),
        "configured_parameters": changes,
    }


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Output .geo or .bgeo path")
    return parser.parse_args()


def main() -> int:
    """Generate the Crag geometry file.

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

    geometry_container = object_context.createNode("geo", "crag_tpose", run_init_scripts=False)
    crag_node = geometry_container.createNode(find_crag_node_type(), "crag")
    changes = configure_static_tpose(crag_node)

    unpack_node = geometry_container.createNode("unpack", "unpack_crag")
    unpack_node.setInput(0, crag_node)

    attribute_delete_node = geometry_container.createNode("attribdelete", "strip_primitive_metadata")
    attribute_delete_node.setInput(0, unpack_node)
    primitive_delete_parm = attribute_delete_node.parm("primdel")
    if primitive_delete_parm is None:
        raise RuntimeError("Attribute Delete SOP does not expose the expected 'primdel' parameter.")
    primitive_delete_parm.set("name piece")

    attribute_delete_node.setDisplayFlag(True)
    attribute_delete_node.setRenderFlag(True)
    attribute_delete_node.cook(force=True)

    geometry = attribute_delete_node.geometry()
    if len(geometry.points()) == 0 or len(geometry.prims()) == 0:
        raise RuntimeError("Crag cooked to empty geometry.")

    geometry.saveToFile(str(output_path))
    print(
        json.dumps(
            geometry_summary(attribute_delete_node, output_path, changes),
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
