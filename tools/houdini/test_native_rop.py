"""Validate the native HouIO Geometry ROP inside Houdini."""

from __future__ import annotations

import argparse
from pathlib import Path

import hou


def parse_arguments() -> argparse.Namespace:
    """Parse the output directory used by the native ROP test."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    return parser.parse_args()


def load_geometry(path: Path) -> hou.Geometry:
    """Load a geometry file into an owned HOM detail."""
    geometry = hou.Geometry()
    geometry.loadFromFile(str(path))
    return geometry


def flatten_templates(
    templates: tuple[hou.ParmTemplate, ...],
) -> list[hou.ParmTemplate]:
    """Return parameter templates recursively in dialog order."""
    flattened: list[hou.ParmTemplate] = []
    for template in templates:
        flattened.append(template)
        if isinstance(template, hou.FolderParmTemplate):
            flattened.extend(flatten_templates(template.parmTemplates()))
    return flattened


def validate_parameter_layout(node_type: hou.NodeType) -> None:
    """Validate the native ROP dialog structure and symbol uniqueness."""
    group = node_type.parmTemplateGroup()
    group.asDialogScript()
    top_level = group.parmTemplates()
    if [template.name() for template in top_level[:2]] != [
        "execute",
        "renderdialog",
    ]:
        raise AssertionError("Native ROP render controls are not first in the dialog")

    folders = [
        template
        for template in top_level
        if isinstance(template, hou.FolderParmTemplate)
    ]
    if [folder.label() for folder in folders] != ["Export", "Scripts"]:
        raise AssertionError(
            "Native ROP tabs are not arranged as Export and Scripts: "
            + repr([folder.label() for folder in folders])
        )

    names = [template.name() for template in flatten_templates(top_level)]
    for expected_name in ("execute", "renderdialog", "trange", "f", "take"):
        if names.count(expected_name) != 1:
            raise AssertionError(
                f"Parameter symbol {expected_name!r} appears {names.count(expected_name)} times"
            )


def validate(output_directory: Path) -> None:
    """Create, render, and validate the native HouIO Geometry ROP."""
    output_directory.mkdir(parents=True, exist_ok=True)
    node_types = hou.ropNodeTypeCategory().nodeTypes()
    if "houio::geometry" not in node_types:
        raise AssertionError(
            "The native houio::geometry ROP was not registered. Available HouIO types: "
            + repr(sorted(name for name in node_types if "houio" in name.lower()))
        )
    validate_parameter_layout(node_types["houio::geometry"])

    object_context = hou.node("/obj")
    output_context = hou.node("/out")
    if object_context is None or output_context is None:
        raise AssertionError("Houdini object or output context is unavailable")

    container = object_context.createNode("geo", "houio_native_rop_source")
    rop = output_context.createNode("houio::geometry", "houio_native_export")
    try:
        box = container.createNode("box", "box")
        line = container.createNode("line", "open_polyline")
        merge = container.createNode("merge", "polygon_source")
        merge.setInput(0, box)
        merge.setInput(1, line)
        transform = container.createNode("xform", "animated_transform")
        transform.setInput(0, merge)
        translate_x = transform.parm("tx")
        if translate_x is None:
            raise AssertionError("Transform SOP has no tx parameter")
        translate_x.setExpression("$F", language=hou.exprLanguage.Hscript)

        sop_path = rop.parm("soppath")
        output_path = rop.parm("sopoutput")
        if sop_path is None or output_path is None:
            raise AssertionError("Native HouIO ROP is missing path parameters")
        for parameter_name in ("execute", "trange", "f", "take", "createdirs", "overwrite", "atomic"):
            if rop.parm(parameter_name) is None and rop.parmTuple(parameter_name) is None:
                raise AssertionError(
                    f"Native HouIO ROP is missing expected parameter {parameter_name!r}"
                )

        sop_path.set(transform.path())
        sequence_pattern = output_directory / "native_rop.$F4.bgeo.sc"
        output_path.set(str(sequence_pattern))
        rop.parm("createdirs").set(1)
        rop.parm("overwrite").set(1)
        rop.parm("atomic").set(1)
        rop.render(frame_range=(1, 2, 1), verbose=True, output_progress=True)

        first_path = output_directory / "native_rop.0001.bgeo.sc"
        second_path = output_directory / "native_rop.0002.bgeo.sc"
        if not first_path.is_file() or not second_path.is_file():
            raise AssertionError("Native HouIO ROP did not produce its frame sequence")

        first = load_geometry(first_path)
        second = load_geometry(second_path)
        hou.setFrame(1)
        source_geometry = transform.geometry()
        if len(first.points()) != len(source_geometry.points()):
            raise AssertionError("Native HouIO ROP changed the source point count")
        if len(first.prims()) != len(source_geometry.prims()):
            raise AssertionError("Native HouIO ROP changed the source primitive count")
        if sum(1 for primitive in first.prims() if not primitive.isClosed()) != 1:
            raise AssertionError("Native HouIO ROP did not preserve the open polyline")

        first_min_x = min(float(point.position()[0]) for point in first.points())
        second_min_x = min(float(point.position()[0]) for point in second.points())
        if abs((second_min_x - first_min_x) - 1.0) > 1.0e-6:
            raise AssertionError("Native HouIO ROP did not evaluate the SOP at each frame")

        sphere = container.createNode("sphere", "unsupported_sphere")
        sphere.parm("type").set("prim")
        unsupported_path = output_directory / "unsupported.bgeo"
        unsupported_path.unlink(missing_ok=True)
        sop_path.set(sphere.path())
        output_path.set(str(unsupported_path))
        try:
            rop.render(frame_range=(1, 1, 1))
        except hou.Error:
            pass
        else:
            raise AssertionError("Native HouIO ROP silently accepted an unsupported sphere")
        error_text = "\n".join(rop.errors())
        if "only polygon and polyline" not in error_text:
            raise AssertionError(
                "Unsupported primitive failure was not actionable: " + error_text
            )
        if unsupported_path.exists():
            raise AssertionError("Unsupported primitive failure left a partial output")

        misnamed_path = output_directory / "unsupported_output.obj"
        misnamed_path.unlink(missing_ok=True)
        sop_path.set(transform.path())
        output_path.set(str(misnamed_path))
        try:
            rop.render(frame_range=(1, 1, 1))
        except hou.Error:
            pass
        else:
            raise AssertionError("Native HouIO ROP accepted an unsupported .obj output")
        extension_error = "\n".join(rop.errors())
        if ".abc, .usd, .usda, and .usdc" not in extension_error:
            raise AssertionError(
                "Unsupported output extension failure was not actionable: "
                + extension_error
            )
        if misnamed_path.exists():
            raise AssertionError("Unsupported .obj output left a partial file")
    finally:
        rop.destroy()
        container.destroy()


def main() -> int:
    """Run the native ROP validation."""
    arguments = parse_arguments()
    validate(arguments.output_directory.resolve())
    print(
        "Native HouIO ROP passed in Houdini "
        + hou.applicationVersionString()
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
