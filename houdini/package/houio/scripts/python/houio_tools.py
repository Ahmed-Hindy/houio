"""User-facing Houdini tools for the HouIO package."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any, Mapping, Optional

import hou

DEFAULT_TIMEOUT_SECONDS = 300.0
ROUNDTRIP_NODE_BASENAME = "houio_roundtrip"
ROUNDTRIP_NODE_COLOR = hou.Color((0.24, 0.46, 0.68))
SUPPORTED_FILE_SUFFIXES = (".bgeo.sc", ".bgeo", ".geo", ".vdb")

ROUNDTRIP_PYTHON_CODE = """from houio_hom import roundtrip_current_sop

node = hou.pwd()
enabled_parameter = node.parm("houio_enabled")
if enabled_parameter is None or enabled_parameter.eval():
    timeout_parameter = node.parm("houio_timeout")
    timeout_seconds = (
        float(timeout_parameter.eval())
        if timeout_parameter is not None
        else 300.0
    )
    roundtrip_current_sop(timeout_seconds=timeout_seconds)
"""


def _is_sop_network(node: Optional[hou.Node]) -> bool:
    """Return whether a node can contain SOP nodes.

    Args:
        node: Candidate Houdini network node.

    Returns:
        True when the node's child category is SOP.
    """
    return bool(
        node is not None
        and node.childTypeCategory() == hou.sopNodeTypeCategory()
    )


def _network_editor(kwargs: Mapping[str, Any]) -> Optional[hou.NetworkEditor]:
    """Resolve the network editor associated with a shelf or Tab-menu call.

    Args:
        kwargs: Houdini tool keyword arguments.

    Returns:
        The active network editor, when one is available.
    """
    if not hasattr(hou, "ui"):
        return None

    pane = kwargs.get("pane")
    if isinstance(pane, hou.NetworkEditor):
        return pane

    active_pane = hou.ui.paneTabUnderCursor()
    if isinstance(active_pane, hou.NetworkEditor):
        return active_pane

    fallback_pane = hou.ui.paneTabOfType(hou.paneTabType.NetworkEditor)
    if isinstance(fallback_pane, hou.NetworkEditor):
        return fallback_pane
    return None


def _selected_sop(parent: Optional[hou.Node] = None) -> Optional[hou.SopNode]:
    """Return the last selected SOP, optionally within one network.

    Args:
        parent: Optional SOP network used to restrict the selection.

    Returns:
        The last matching selected SOP, when one exists.
    """
    selected_nodes = [
        node
        for node in hou.selectedNodes()
        if isinstance(node, hou.SopNode)
        and (parent is None or node.parent() == parent)
    ]
    return selected_nodes[-1] if selected_nodes else None


def _add_roundtrip_parameters(node: hou.SopNode) -> None:
    """Add HouIO controls to a standard Python SOP.

    Args:
        node: Python SOP receiving the controls.
    """
    parameter_group = node.parmTemplateGroup()
    settings_folder = hou.FolderParmTemplate("houio_settings", "HouIO")
    settings_folder.addParmTemplate(
        hou.ToggleParmTemplate(
            "houio_enabled",
            "Enabled",
            default_value=True,
            help="Run the input geometry through HouIO when this node cooks.",
        )
    )
    settings_folder.addParmTemplate(
        hou.FloatParmTemplate(
            "houio_timeout",
            "Timeout (seconds)",
            1,
            default_value=(DEFAULT_TIMEOUT_SECONDS,),
            help="Maximum time allowed for the external HouIO converter.",
        )
    )
    parameter_group.append(settings_folder)
    node.setParmTemplateGroup(parameter_group)


def create_roundtrip_sop(
    parent: hou.Node,
    input_node: Optional[hou.SopNode] = None,
    position: Optional[hou.Vector2] = None,
) -> hou.SopNode:
    """Create a configured HouIO Python SOP.

    Args:
        parent: SOP network that will contain the new node.
        input_node: Optional SOP connected to the new node's first input.
        position: Optional network editor position.

    Returns:
        The newly created Python SOP.

    Raises:
        hou.OperationFailed: If the parent is not a SOP network.
    """
    if not _is_sop_network(parent):
        raise hou.OperationFailed("HouIO Round Trip must be created in a SOP network.")
    if input_node is not None and input_node.parent() != parent:
        raise hou.OperationFailed("The selected SOP belongs to a different network.")

    roundtrip_node = parent.createNode("python", node_name=ROUNDTRIP_NODE_BASENAME)
    _add_roundtrip_parameters(roundtrip_node)
    python_parameter = roundtrip_node.parm("python")
    if python_parameter is None:
        roundtrip_node.destroy()
        raise hou.OperationFailed("The Python SOP has no Python code parameter.")
    python_parameter.set(ROUNDTRIP_PYTHON_CODE)

    roundtrip_node.setColor(ROUNDTRIP_NODE_COLOR)
    roundtrip_node.setComment("Processes input geometry through HouIO.")
    roundtrip_node.setGenericFlag(hou.nodeFlag.DisplayComment, True)

    if input_node is not None:
        roundtrip_node.setInput(0, input_node)
        roundtrip_node.moveToGoodPosition()
    elif position is not None:
        roundtrip_node.setPosition(position)
    else:
        roundtrip_node.moveToGoodPosition()

    roundtrip_node.setSelected(True, clear_all_selected=True)
    roundtrip_node.setDisplayFlag(True)
    roundtrip_node.setRenderFlag(True)
    return roundtrip_node


def create_roundtrip_from_tool(kwargs: Mapping[str, Any]) -> Optional[hou.SopNode]:
    """Create a HouIO SOP from a shelf or Tab-menu invocation.

    Args:
        kwargs: Houdini tool keyword arguments.

    Returns:
        The created node, or None when no SOP network is active.
    """
    network_editor = _network_editor(kwargs)
    parent = None
    input_node = None
    position = None

    if network_editor is not None:
        candidate_parent = network_editor.pwd()
        if _is_sop_network(candidate_parent):
            parent = candidate_parent
            input_node = _selected_sop(parent)
            position = network_editor.visibleBounds().center()

    if parent is None:
        input_node = _selected_sop()
        parent = input_node.parent() if input_node is not None else None

    if parent is None:
        hou.ui.displayMessage(
            "Open a SOP network or select a SOP node, then run HouIO Round Trip again.",
            severity=hou.severityType.Warning,
            title="HouIO",
        )
        return None

    try:
        return create_roundtrip_sop(parent, input_node=input_node, position=position)
    except hou.Error as exception:
        hou.ui.displayMessage(
            str(exception),
            severity=hou.severityType.Error,
            title="HouIO Round Trip",
        )
        return None


def _output_path_for(input_path: Path) -> Path:
    """Return a non-destructive default output path.

    Args:
        input_path: Source geometry path.

    Returns:
        A sibling path with ``_houio`` inserted before the full geometry suffix.
    """
    input_text = str(input_path)
    input_text_lower = input_text.lower()
    for suffix in SUPPORTED_FILE_SUFFIXES:
        if input_text_lower.endswith(suffix):
            return Path(input_text[: -len(suffix)] + "_houio" + input_text[-len(suffix) :])
    return input_path.with_name(input_path.name + "_houio.bgeo.sc")


def convert_geometry_file() -> Optional[Path]:
    """Open Houdini file dialogs and convert one geometry file with HouIO.

    Returns:
        The output path, or None when the user cancels.
    """
    from houio_hom import convert_with_houio

    input_text = hou.ui.selectFile(
        title="Select geometry to process with HouIO",
        chooser_mode=hou.fileChooserMode.Read,
        file_type=hou.fileType.Geometry,
        collapse_sequences=True,
    )
    if not input_text:
        return None

    input_path = Path(hou.text.expandString(input_text)).resolve()
    default_output = _output_path_for(input_path)
    output_text = hou.ui.selectFile(
        start_directory=default_output.parent.as_posix(),
        default_value=default_output.name,
        title="Save HouIO result",
        chooser_mode=hou.fileChooserMode.Write,
        file_type=hou.fileType.Geometry,
        collapse_sequences=True,
    )
    if not output_text:
        return None

    output_path = Path(hou.text.expandString(output_text)).resolve()
    if output_path == input_path:
        hou.ui.displayMessage(
            "Choose a different output file. HouIO does not overwrite its input.",
            severity=hou.severityType.Warning,
            title="HouIO",
        )
        return None

    try:
        convert_with_houio(input_path, output_path)
    except Exception as exception:
        hou.ui.displayMessage(
            str(exception),
            severity=hou.severityType.Error,
            title="HouIO Conversion Failed",
        )
        return None

    hou.ui.displayMessage(
        f"HouIO wrote:\n{output_path}",
        severity=hou.severityType.Message,
        title="HouIO Conversion Complete",
    )
    return output_path


def package_diagnostics() -> dict[str, object]:
    """Collect package and runtime diagnostics.

    Returns:
        A dictionary suitable for display or automated validation.
    """
    import houio_hom

    root_text = os.environ.get("HOUIO_ROOT", "")
    converter_text = os.environ.get("HOUIO_CONVERT_EXECUTABLE", "")
    blosc_text = hou.text.expandString(os.environ.get("HOUIO_BLOSC_LIBRARY", ""))
    root_path = Path(root_text) if root_text else None
    converter_path = Path(converter_text) if converter_text else None
    blosc_path = Path(blosc_text) if blosc_text else None

    return {
        "houdini_version": hou.applicationVersionString(),
        "license_category": str(hou.licenseCategory()),
        "package_version": os.environ.get("HOUIO_VERSION", "unknown"),
        "package_root": str(root_path) if root_path else "not set",
        "package_root_exists": bool(root_path and root_path.is_dir()),
        "python_module": str(Path(houio_hom.__file__).resolve()),
        "converter": str(converter_path) if converter_path else "not set",
        "converter_exists": bool(converter_path and converter_path.is_file()),
        "blosc_library": str(blosc_path) if blosc_path else "not set",
        "blosc_exists": bool(blosc_path and blosc_path.is_file()),
    }


def show_package_diagnostics() -> dict[str, object]:
    """Display package diagnostics in Houdini.

    Returns:
        The diagnostics dictionary.
    """
    diagnostics = package_diagnostics()
    rows = [f"{key}: {value}" for key, value in diagnostics.items()]
    healthy = bool(
        diagnostics["package_root_exists"]
        and diagnostics["converter_exists"]
        and diagnostics["blosc_exists"]
    )
    hou.ui.displayMessage(
        "\n".join(rows),
        severity=hou.severityType.Message if healthy else hou.severityType.Warning,
        title="HouIO Package Diagnostics",
    )
    return diagnostics
