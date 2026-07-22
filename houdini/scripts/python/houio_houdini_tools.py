"""User-facing Houdini tools for HouIO."""

from __future__ import annotations

from pathlib import Path
from typing import Optional

import hou

from houio_hom import convert_with_houio


ROUNDTRIP_SOP_CODE = """from houio_hom import roundtrip_current_sop

roundtrip_current_sop()
"""


def _selected_sop() -> hou.SopNode:
    """Return the selected SOP node or raise a user-facing error."""
    selected_nodes = hou.selectedNodes()
    if len(selected_nodes) != 1 or not isinstance(selected_nodes[0], hou.SopNode):
        raise hou.Error("Select exactly one SOP node before running HouIO Round Trip SOP.")
    return selected_nodes[0]


def create_roundtrip_sop(source_node: Optional[hou.SopNode] = None) -> hou.SopNode:
    """Create and cook a HouIO Python SOP after a source SOP.

    Args:
        source_node: Optional source SOP. When omitted, the single selected SOP is used.

    Returns:
        The newly created Python SOP.
    """
    source = source_node or _selected_sop()
    parent = source.parent()
    node = parent.createNode("python", node_name="houio_roundtrip")
    node.setInput(0, source)
    node.parm("python").set(ROUNDTRIP_SOP_CODE)
    node.moveToGoodPosition()
    node.setDisplayFlag(True)
    node.setRenderFlag(True)
    node.setSelected(True, clear_all_selected=True)
    node.cook(force=True)
    if node.errors():
        message = "; ".join(node.errors())
        node.destroy()
        raise hou.Error(f"HouIO round trip failed: {message}")
    return node


def convert_file() -> Optional[Path]:
    """Prompt for source and destination paths and run HouIO conversion."""
    source_text = hou.ui.selectFile(
        title="Select HouIO input",
        file_type=hou.fileType.Geometry,
        chooser_mode=hou.fileChooserMode.Read,
    )
    if not source_text:
        return None

    destination_text = hou.ui.selectFile(
        title="Select HouIO output",
        file_type=hou.fileType.Geometry,
        chooser_mode=hou.fileChooserMode.Write,
    )
    if not destination_text:
        return None

    source = Path(hou.text.expandString(source_text))
    destination = Path(hou.text.expandString(destination_text))
    try:
        convert_with_houio(source, destination)
    except Exception as exception:
        raise hou.Error(f"HouIO conversion failed: {exception}") from exception

    hou.ui.displayMessage(f"HouIO wrote:\n{destination}", title="HouIO")
    return destination
