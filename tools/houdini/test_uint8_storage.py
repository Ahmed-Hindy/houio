"""Validate Houdini UInt8 attribute round trips through HouIO."""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path

import hou

from houio_hom import write_geometry


EXPECTED_VALUES = (0, 128, 255, 255, 255, 255, 255, 255)


def parse_arguments() -> argparse.Namespace:
    """Parse the test output directory."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    return parser.parse_args()


def validate_mask(geometry: hou.Geometry, description: str) -> None:
    """Validate UInt8 storage and point values."""
    attribute = geometry.findPointAttrib("mask")
    if attribute is None:
        raise AssertionError(f"{description} is missing point attribute mask")
    if attribute.numericDataType() != hou.numericData.UInt8:
        raise AssertionError(
            f"{description} changed mask storage to {attribute.numericDataType()}"
        )
    values = tuple(int(point.attribValue(attribute)) for point in geometry.points())
    if values != EXPECTED_VALUES:
        raise AssertionError(f"{description} changed mask values: {values}")


def create_houdini_source(
    path: Path,
    direct_path: Path,
    executable: Path,
) -> None:
    """Author UInt8 geometry and write native and direct-HOM fixtures."""
    object_context = hou.node("/obj")
    if object_context is None:
        raise RuntimeError("Houdini object context is unavailable")

    container = object_context.createNode("geo", "houio_uint8_source")
    try:
        for child in container.children():
            child.destroy()

        box = container.createNode("box", "source")
        wrangle = container.createNode("attribwrangle", "values")
        wrangle.setInput(0, box)
        wrangle.parm("snippet").set(
            "i@mask = @ptnum == 0 ? 0 : (@ptnum == 1 ? 128 : 255);"
        )

        cast = container.createNode("attribcast", "uint8")
        cast.setInput(0, wrangle)
        cast.parm("class1").set("point")
        cast.parm("attribs1").set("mask")
        cast.parm("precision1").set("uint8")
        cast.cook(force=True)
        if cast.errors():
            raise AssertionError("; ".join(cast.errors()))

        geometry = cast.geometry()
        validate_mask(geometry, "Houdini-authored geometry")
        geometry.saveToFile(str(path))
        write_result = write_geometry(
            geometry,
            direct_path,
            executable=executable,
        )
        if not write_result.success:
            raise AssertionError(
                "Direct HOM UInt8 write failed:\n"
                f"stdout:\n{write_result.stdout}\n"
                f"stderr:\n{write_result.stderr}"
            )
    finally:
        container.destroy()


def convert_with_houio(source: Path, destination: Path) -> None:
    """Convert the Houdini-authored BGEO through the standalone HouIO CLI."""
    executable = Path(os.environ["HOUIO_EXECUTABLE"]).resolve()
    completed = subprocess.run(
        [str(executable), "convert", str(source), str(destination), "--json"],
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if completed.returncode != 0:
        raise AssertionError(
            "HouIO UInt8 conversion failed:\n"
            f"stdout:\n{completed.stdout}\n"
            f"stderr:\n{completed.stderr}"
        )


def main() -> int:
    """Run the Houdini-to-HouIO-to-Houdini UInt8 round trip."""
    arguments = parse_arguments()
    output_directory = arguments.output_directory.resolve()
    output_directory.mkdir(parents=True, exist_ok=True)

    native_path = output_directory / "houdini_uint8.bgeo"
    houio_path = output_directory / "houio_uint8.bgeo"
    direct_path = output_directory / "houio_direct_uint8.bgeo"
    executable = Path(os.environ["HOUIO_EXECUTABLE"]).resolve()
    for path in (native_path, houio_path, direct_path):
        path.unlink(missing_ok=True)

    create_houdini_source(native_path, direct_path, executable)
    convert_with_houio(native_path, houio_path)

    for path, description in (
        (houio_path, "HouIO file-conversion geometry"),
        (direct_path, "HouIO direct-HOM geometry"),
    ):
        result = hou.Geometry()
        result.loadFromFile(str(path))
        validate_mask(result, description)
    print(f"HouIO UInt8 round trips passed: {output_directory}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
