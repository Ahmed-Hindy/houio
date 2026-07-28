"""Validate the native HouIO ROP inside a genuine non-commercial HIP session."""

from __future__ import annotations

import argparse
from pathlib import Path

import hou


def parse_arguments() -> argparse.Namespace:
    """Parse the source HIPNC and isolated output directory."""
    parser = argparse.ArgumentParser()
    parser.add_argument("source_hipnc", type=Path)
    parser.add_argument("output_directory", type=Path)
    return parser.parse_args()


def destroy_if_present(path: str) -> None:
    """Destroy a test node left by an earlier run."""
    node = hou.node(path)
    if node is not None:
        node.destroy()


def load_geometry(path: Path) -> hou.Geometry:
    """Load a generated geometry file into an owned detail."""
    geometry = hou.Geometry()
    geometry.loadFromFile(str(path))
    return geometry


def validate(source_hipnc: Path, output_directory: Path) -> None:
    """Load a HIPNC, render through HouIO, and save an isolated HIPNC copy."""
    source_hipnc = source_hipnc.resolve()
    output_directory = output_directory.resolve()
    if source_hipnc.suffix.lower() != ".hipnc" or not source_hipnc.is_file():
        raise AssertionError(f"Expected an existing .hipnc source: {source_hipnc}")

    output_directory.mkdir(parents=True, exist_ok=True)
    hou.hipFile.load(str(source_hipnc), suppress_save_prompt=True)
    if not hou.isApprentice():
        raise AssertionError(
            "Loading the supplied HIPNC did not enter Houdini Apprentice mode: "
            + str(hou.licenseCategory())
        )

    node_types = hou.ropNodeTypeCategory().nodeTypes()
    if "houio::geometry" not in node_types:
        raise AssertionError("The native houio::geometry ROP is unavailable in Apprentice mode")
    node_types["houio::geometry"].parmTemplateGroup().asDialogScript()

    object_context = hou.node("/obj")
    output_context = hou.node("/out")
    if object_context is None or output_context is None:
        raise AssertionError("The loaded HIPNC has no /obj or /out context")

    destroy_if_present("/obj/HOUIO_NONCOMMERCIAL_TEST")
    destroy_if_present("/out/HOUIO_NONCOMMERCIAL_EXPORT")

    container = object_context.createNode("geo", "HOUIO_NONCOMMERCIAL_TEST")
    box = container.createNode("box", "polygon_box")
    line = container.createNode("line", "open_polyline")
    merge = container.createNode("merge", "OUT_HOUIO_NONCOMMERCIAL")
    merge.setInput(0, box)
    merge.setInput(1, line)
    merge.setDisplayFlag(True)
    merge.setRenderFlag(True)

    rop = output_context.createNode("houio::geometry", "HOUIO_NONCOMMERCIAL_EXPORT")
    output_pattern = output_directory / "noncommercial.$F4.bgeo.sc"
    rop.parm("soppath").set(merge.path())
    rop.parm("sopoutput").set(str(output_pattern))
    rop.parm("createdirs").set(1)
    rop.parm("overwrite").set(1)
    rop.parm("atomic").set(1)
    rop.render(frame_range=(1, 2, 1), verbose=True, output_progress=True)

    for frame in (1, 2):
        path = output_directory / f"noncommercial.{frame:04d}.bgeo.sc"
        if not path.is_file():
            raise AssertionError(f"Non-commercial ROP render did not create {path}")
        geometry = load_geometry(path)
        if len(geometry.prims()) != 7:
            raise AssertionError(
                f"Unexpected primitive count in non-commercial output: {len(geometry.prims())}"
            )
        if sum(1 for primitive in geometry.prims() if not primitive.isClosed()) != 1:
            raise AssertionError("Non-commercial output lost the open polyline")

    saved_copy = output_directory / "houio_native_rop_noncommercial_test.hipnc"
    hou.hipFile.save(file_name=str(saved_copy), save_to_recent_files=False)
    if not saved_copy.is_file() or saved_copy.suffix.lower() != ".hipnc":
        raise AssertionError("The isolated non-commercial HIP copy was not saved")

    print("Native HouIO ROP passed in genuine Apprentice mode")
    print("Source HIPNC: " + str(source_hipnc))
    print("Saved HIPNC: " + str(saved_copy))
    print("License: " + str(hou.licenseCategory()))


def main() -> int:
    """Run the non-commercial native ROP validation."""
    arguments = parse_arguments()
    validate(arguments.source_hipnc, arguments.output_directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
