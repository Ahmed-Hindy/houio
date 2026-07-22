"""Validate the distributable HouIO package inside Houdini."""

from __future__ import annotations

import os
from pathlib import Path

import hou

import houio_hom
import houio_tools


def validate_environment() -> None:
    """Validate package paths and imported modules."""
    package_root = Path(os.environ["HOUIO_ROOT"]).resolve()
    converter_path = Path(os.environ["HOUIO_CONVERT_EXECUTABLE"]).resolve()
    blosc_path = Path(hou.text.expandString(os.environ["HOUIO_BLOSC_LIBRARY"])).resolve()

    if not package_root.is_dir():
        raise AssertionError(f"Missing package root: {package_root}")
    if not converter_path.is_file():
        raise AssertionError(f"Missing converter: {converter_path}")
    if not blosc_path.is_file():
        raise AssertionError(f"Missing Blosc runtime: {blosc_path}")
    if package_root not in Path(houio_hom.__file__).resolve().parents:
        raise AssertionError("houio_hom was not imported from the package")
    if package_root not in Path(houio_tools.__file__).resolve().parents:
        raise AssertionError("houio_tools was not imported from the package")


def validate_shelf_tools() -> None:
    """Validate the shelf and Tab-menu tool definitions."""
    expected_tools = {
        "houio_roundtrip_sop",
        "houio_convert_geometry_file",
        "houio_package_diagnostics",
    }
    loaded_tools = set(hou.shelves.tools())
    missing_tools = expected_tools - loaded_tools
    if missing_tools:
        raise AssertionError(f"Missing HouIO shelf tools: {sorted(missing_tools)}")


def validate_roundtrip_sop() -> None:
    """Create and cook the user-facing HouIO Round Trip SOP."""
    object_context = hou.node("/obj")
    if object_context is None:
        raise AssertionError("Houdini object context is unavailable")

    geometry_container = object_context.createNode("geo", node_name="houio_package_test")
    try:
        source_node = geometry_container.createNode("box", node_name="SOURCE")
        source_node.setSelected(True, clear_all_selected=True)
        tool_arguments = {}
        if hasattr(hou, "ui"):
            network_editor = hou.ui.paneTabOfType(hou.paneTabType.NetworkEditor)
            if isinstance(network_editor, hou.NetworkEditor):
                network_editor.setPwd(geometry_container)
                tool_arguments["pane"] = network_editor
        roundtrip_node = houio_tools.create_roundtrip_from_tool(tool_arguments)
        if roundtrip_node is None:
            raise AssertionError("The shelf tool did not create a Round Trip SOP")
        roundtrip_node.cook(force=True)
        if roundtrip_node.errors():
            raise AssertionError("; ".join(roundtrip_node.errors()))
        if roundtrip_node.warnings():
            raise AssertionError("; ".join(roundtrip_node.warnings()))

        source_geometry = source_node.geometry()
        result_geometry = roundtrip_node.geometry()
        if len(source_geometry.points()) != len(result_geometry.points()):
            raise AssertionError("Round Trip SOP changed the box point count")
        if len(source_geometry.prims()) != len(result_geometry.prims()):
            raise AssertionError("Round Trip SOP changed the box primitive count")
        enabled_parameter = roundtrip_node.parm("houio_enabled")
        if enabled_parameter is None:
            raise AssertionError("Round Trip SOP is missing the Enabled parameter")
        if roundtrip_node.parm("houio_timeout") is None:
            raise AssertionError("Round Trip SOP is missing the Timeout parameter")

        enabled_parameter.set(0)
        roundtrip_node.cook(force=True)
        bypass_geometry = roundtrip_node.geometry()
        if len(source_geometry.points()) != len(bypass_geometry.points()):
            raise AssertionError("Disabled Round Trip SOP changed the box point count")
        if len(source_geometry.prims()) != len(bypass_geometry.prims()):
            raise AssertionError("Disabled Round Trip SOP changed the box primitive count")
    finally:
        geometry_container.destroy()


def validate_diagnostics() -> None:
    """Validate non-interactive package diagnostics."""
    diagnostics = houio_tools.package_diagnostics()
    required_true_values = (
        "package_root_exists",
        "converter_exists",
        "blosc_exists",
    )
    for key in required_true_values:
        if diagnostics.get(key) is not True:
            raise AssertionError(f"Package diagnostic failed: {key}")


def main() -> int:
    """Run package validation."""
    validate_environment()
    validate_shelf_tools()
    validate_roundtrip_sop()
    validate_diagnostics()
    print(
        "HouIO Houdini package passed in "
        f"Houdini {hou.applicationVersionString()} "
        f"({hou.licenseCategory()})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
