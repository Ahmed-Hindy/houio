"""Validate the user-facing HouIO Houdini package tools."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import hou


def main() -> int:
    """Create and cook the packaged HouIO round-trip SOP."""
    parser = argparse.ArgumentParser()
    parser.add_argument("package_root", type=Path)
    arguments = parser.parse_args()

    package_root = arguments.package_root.resolve()
    scripts_root = package_root / "scripts" / "python"
    sys.path.insert(0, str(scripts_root))

    from houio_houdini_tools import create_roundtrip_sop

    object_context = hou.node("/obj")
    if object_context is None:
        raise AssertionError("Houdini object context is unavailable")

    container = object_context.createNode("geo", node_name="houio_package_test")
    try:
        source = container.createNode("box", node_name="source_box")
        source_geometry = source.geometry()
        result = create_roundtrip_sop(source)
        result_geometry = result.geometry()

        if result.input(0) != source:
            raise AssertionError("HouIO Python SOP was not connected to the source SOP")
        if len(result_geometry.points()) != len(source_geometry.points()):
            raise AssertionError("HouIO package changed box point count")
        if len(result_geometry.prims()) != len(source_geometry.prims()):
            raise AssertionError("HouIO package changed box primitive count")
        if result.errors() or result.warnings():
            raise AssertionError(
                f"HouIO package node was not clean: {result.errors()} {result.warnings()}"
            )
    finally:
        container.destroy()

    shelf_path = package_root / "toolbar" / "houio.shelf"
    if not shelf_path.is_file():
        raise AssertionError(f"Missing HouIO shelf: {shelf_path}")
    shelf_text = shelf_path.read_text(encoding="utf-8")
    for tool_name in ("houio_roundtrip_sop", "houio_convert_file"):
        if tool_name not in shelf_text:
            raise AssertionError(f"Missing shelf tool: {tool_name}")

    print(f"HouIO Houdini package tools passed in Houdini {hou.applicationVersionString()}")
    print(f"License category: {hou.licenseCategory()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
