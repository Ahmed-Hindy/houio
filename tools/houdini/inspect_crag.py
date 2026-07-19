"""Inspect the installed Houdini Crag SOP and its default output."""

from __future__ import annotations

import collections
import json

import hou

from generate_crag import find_crag_node_type


def main() -> int:
    """Print Crag parameter and primitive diagnostics.

    Returns:
        Process exit code.
    """
    hou.hipFile.clear(suppress_save_prompt=True)
    hou.setFrame(1)

    object_context = hou.node("/obj")
    if object_context is None:
        raise RuntimeError("Houdini object context /obj is unavailable.")

    geometry_container = object_context.createNode("geo", "inspect_crag", run_init_scripts=False)
    crag_node = geometry_container.createNode(find_crag_node_type(), "crag")
    crag_node.cook(force=True)

    parameter_data = []
    for parm in crag_node.parms():
        template = parm.parmTemplate()
        entry = {
            "name": parm.name(),
            "label": template.label(),
            "type": template.type().name(),
            "value": parm.eval(),
        }
        if template.type() == hou.parmTemplateType.Menu:
            entry["menu_items"] = parm.menuItems()
            entry["menu_labels"] = parm.menuLabels()
        parameter_data.append(entry)

    primitive_types = collections.Counter(primitive.type().name() for primitive in crag_node.geometry().prims())
    summary = {
        "houdini_version": hou.applicationVersionString(),
        "node_type": crag_node.type().nameWithCategory(),
        "time_dependent": crag_node.isTimeDependent(),
        "primitive_types": dict(sorted(primitive_types.items())),
        "parameters": parameter_data,
    }
    print(json.dumps(summary, indent=2, sort_keys=True, default=str))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
