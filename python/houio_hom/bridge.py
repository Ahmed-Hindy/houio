"""HOM adapters between Houdini-supported files and HouIO-compatible bgeo data."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Optional, Union

import hou

PathLike = Union[str, Path]


def _resolve_converter(executable: Optional[PathLike]) -> Path:
    """Resolve the installed or locally built HouIO converter executable."""
    candidates: list[Path] = []
    if executable is not None:
        candidates.append(Path(executable))
    environment_executable = os.environ.get("HOUIO_CONVERT_EXECUTABLE")
    if environment_executable:
        candidates.append(Path(environment_executable))
    path_executable = shutil.which("houio_convert") or shutil.which("houio_convert.exe")
    if path_executable:
        candidates.append(Path(path_executable))

    repository_root = os.environ.get("HOUIO_ROOT")
    if repository_root:
        root = Path(repository_root)
        candidates.extend(
            (
                root / "build" / "windows-msvc-release" / "houio_convert.exe",
                root
                / "build"
                / "windows-msvc-release"
                / "Release"
                / "houio_convert.exe",
                root / "build" / "linux-gcc-release" / "houio_convert",
            )
        )

    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError(
        "Could not find houio_convert. Pass executable=..., set "
        "HOUIO_CONVERT_EXECUTABLE, or install houio_convert on PATH."
    )


def _copy_geometry(geometry: hou.Geometry) -> hou.Geometry:
    """Return an independent copy of a Houdini geometry object."""
    result = hou.Geometry()
    result.merge(geometry)
    return result


def _primitive_counts(geometry: hou.Geometry) -> tuple[int, int, int]:
    """Return polygon, dense-volume, and VDB primitive counts."""
    polygon_count = 0
    volume_count = 0
    vdb_count = 0
    for primitive in geometry.prims():
        if isinstance(primitive, hou.VDB):
            vdb_count += 1
        elif isinstance(primitive, hou.Volume):
            volume_count += 1
        elif isinstance(primitive, hou.Polygon):
            polygon_count += 1
    return polygon_count, volume_count, vdb_count


def convert_vdb_to_volume(geometry: hou.Geometry) -> hou.Geometry:
    """Convert every VDB primitive to a dense Houdini volume.

    Args:
        geometry: Source geometry containing one or more VDB primitives.

    Returns:
        Independent geometry with VDB primitives converted to dense volumes.

    Raises:
        ValueError: If the source contains no VDB primitive or conversion does
            not produce dense volumes.
        hou.OperationFailed: If Houdini cannot execute the Convert VDB verb.
    """
    polygon_count, volume_count, vdb_count = _primitive_counts(geometry)
    if vdb_count == 0:
        raise ValueError("Geometry contains no VDB primitives")
    if polygon_count != 0 or volume_count != 0 or vdb_count != len(geometry.prims()):
        raise ValueError(
            "VDB conversion requires geometry containing only VDB primitives"
        )

    verb = hou.sopNodeTypeCategory().nodeVerb("convertvdb")
    if verb is None:
        raise hou.OperationFailed("The Convert VDB SOP verb is unavailable")
    verb.setParms({"conversion": 0})

    result = hou.Geometry()
    verb.execute(result, [geometry])
    result_polygon_count, result_volume_count, remaining_vdb_count = _primitive_counts(
        result
    )
    if (
        result_polygon_count != 0
        or result_volume_count != len(result.prims())
        or remaining_vdb_count != 0
    ):
        raise ValueError("Convert VDB did not produce a pure dense-volume result")
    return result


def convert_volume_to_vdb(geometry: hou.Geometry) -> hou.Geometry:
    """Convert every dense Houdini volume to an OpenVDB primitive.

    Args:
        geometry: Source geometry containing one or more dense volumes.

    Returns:
        Independent geometry containing VDB primitives suitable for `.vdb`
        output.

    Raises:
        ValueError: If the source contains no dense volume or conversion leaves
            non-VDB primitives in the result.
        hou.OperationFailed: If Houdini cannot execute the Convert VDB verb.
    """
    polygon_count, volume_count, vdb_count = _primitive_counts(geometry)
    if volume_count == 0:
        if vdb_count > 0 and polygon_count == 0 and vdb_count == len(geometry.prims()):
            return _copy_geometry(geometry)
        raise ValueError("Geometry contains no dense volume primitives")
    if polygon_count != 0 or vdb_count != 0 or volume_count != len(geometry.prims()):
        raise ValueError("VDB output requires geometry containing only dense volumes")

    verb = hou.sopNodeTypeCategory().nodeVerb("convertvdb")
    if verb is None:
        raise hou.OperationFailed("The Convert VDB SOP verb is unavailable")
    verb.setParms({"conversion": 1})

    result = hou.Geometry()
    verb.execute(result, [geometry])
    _, remaining_volume_count, result_vdb_count = _primitive_counts(result)
    if remaining_volume_count != 0 or result_vdb_count != len(result.prims()):
        raise ValueError(".vdb output requires all primitives to be VDB grids")
    return result


def geometry_from_bgeo_bytes(data: bytes) -> hou.Geometry:
    """Create a Houdini geometry object from uncompressed bgeo bytes.

    Args:
        data: Binary bgeo payload produced by HouIO or `hou.Geometry.data()`.

    Returns:
        Newly allocated Houdini geometry.
    """
    geometry = hou.Geometry()
    geometry.load(data)
    return geometry


def geometry_to_bgeo_bytes(geometry: hou.Geometry) -> bytes:
    """Return an uncompressed bgeo representation of a Houdini geometry.

    Args:
        geometry: Geometry to serialize.

    Returns:
        Binary bgeo data accepted by HouIO's stream parser.
    """
    return geometry.data()


def load_for_houio(path: PathLike, *, convert_vdb: bool = True) -> hou.Geometry:
    """Load any Houdini-supported geometry file for HouIO consumption.

    `.bgeo.sc` is transparently decompressed by Houdini. When `convert_vdb` is
    enabled, OpenVDB primitives are converted to dense volumes because HouIO's
    native VDB primitive model is not implemented yet.

    Args:
        path: Geometry file supported by Houdini's File SOP.
        convert_vdb: Convert VDB primitives to dense volumes.

    Returns:
        Independent geometry suitable for `geometry_to_bgeo_bytes()` or an
        uncompressed `.bgeo` intermediate.
    """
    geometry = hou.Geometry()
    geometry.loadFromFile(str(Path(path)))
    _, _, vdb_count = _primitive_counts(geometry)
    if vdb_count > 0 and convert_vdb:
        return convert_vdb_to_volume(geometry)
    return geometry


def save_from_houio(
    data: bytes, path: PathLike, *, convert_volume_for_vdb: bool = True
) -> None:
    """Save HouIO-produced bgeo bytes through Houdini's format writers.

    Args:
        data: Uncompressed bgeo bytes produced by HouIO.
        path: Destination file. The extension selects Houdini's writer.
        convert_volume_for_vdb: Convert dense volumes before `.vdb` output.
    """
    destination = Path(path)
    geometry = geometry_from_bgeo_bytes(data)
    if destination.suffix.lower() == ".vdb" and convert_volume_for_vdb:
        geometry = convert_volume_to_vdb(geometry)
    destination.parent.mkdir(parents=True, exist_ok=True)
    geometry.saveToFile(str(destination))


def load_into_current_sop(path: PathLike, *, convert_vdb: bool = True) -> hou.Geometry:
    """Replace the current Python SOP geometry with a file's contents.

    Args:
        path: Geometry file to load.
        convert_vdb: Convert VDB primitives to dense volumes for HouIO parity.

    Returns:
        The current SOP's modifiable geometry.

    Raises:
        hou.GeometryPermissionError: If called outside a writable Python SOP
            cook context.
    """
    source = load_for_houio(path, convert_vdb=convert_vdb)
    target = hou.pwd().geometry()
    target.clear()
    target.merge(source)
    return target


def save_node_geometry(
    node: hou.SopNode, path: PathLike, *, convert_volume_for_vdb: bool = True
) -> None:
    """Save a cooked SOP node using Houdini's format writers.

    Args:
        node: SOP node whose cooked geometry should be saved.
        path: Destination `.bgeo`, `.bgeo.sc`, `.geo`, or `.vdb` path.
        convert_volume_for_vdb: Convert dense volumes before `.vdb` output.
    """
    destination = Path(path)
    geometry = _copy_geometry(node.geometry())
    if destination.suffix.lower() == ".vdb" and convert_volume_for_vdb:
        geometry = convert_volume_to_vdb(geometry)
    destination.parent.mkdir(parents=True, exist_ok=True)
    geometry.saveToFile(str(destination))


def convert_with_houio(
    input_path: PathLike,
    output_path: PathLike,
    *,
    executable: Optional[PathLike] = None,
) -> None:
    """Run the HouIO converter from a Houdini Python session.

    VDB input is converted to a temporary dense-volume bgeo before invoking
    HouIO. VDB output is written by Houdini after HouIO produces an
    uncompressed bgeo intermediate. `.bgeo.sc` can be passed directly because
    `houio_convert` supports the SCF wrapper natively.

    Args:
        input_path: Input `.geo`, `.bgeo`, `.bgeo.sc`, or scalar `.vdb` file.
        output_path: Output `.bgeo`, `.bgeo.sc`, or `.vdb` file.
        executable: Optional explicit path to `houio_convert`.

    Raises:
        FileNotFoundError: If `houio_convert` cannot be resolved.
        subprocess.CalledProcessError: If HouIO reports a conversion failure.
    """
    source = Path(input_path)
    destination = Path(output_path)
    converter = _resolve_converter(executable)
    destination.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="houio_hom_") as temporary_directory:
        temporary_root = Path(temporary_directory)
        converter_input = source
        converter_output = destination

        if source.suffix.lower() == ".vdb":
            converter_input = temporary_root / "input.bgeo"
            geometry = load_for_houio(source, convert_vdb=True)
            converter_input.write_bytes(geometry_to_bgeo_bytes(geometry))

        if destination.suffix.lower() == ".vdb":
            converter_output = temporary_root / "output.bgeo"

        completed = subprocess.run(
            (str(converter), str(converter_input), str(converter_output)),
            check=False,
            capture_output=True,
            text=True,
        )
        if completed.returncode != 0:
            raise subprocess.CalledProcessError(
                completed.returncode,
                completed.args,
                output=completed.stdout,
                stderr=completed.stderr,
            )

        if destination.suffix.lower() == ".vdb":
            save_from_houio(converter_output.read_bytes(), destination)


def roundtrip_node_geometry(
    node: hou.SopNode, *, executable: Optional[PathLike] = None
) -> hou.Geometry:
    """Round-trip a cooked SOP node through HouIO and return new geometry.

    Args:
        node: SOP node whose cooked geometry should be processed.
        executable: Optional explicit path to `houio_convert`.

    Returns:
        Newly allocated geometry loaded from HouIO's output.
    """
    converter = _resolve_converter(executable)
    with tempfile.TemporaryDirectory(prefix="houio_node_") as temporary_directory:
        temporary_root = Path(temporary_directory)
        input_path = temporary_root / "input.bgeo"
        output_path = temporary_root / "output.bgeo"
        input_path.write_bytes(geometry_to_bgeo_bytes(node.geometry()))
        convert_with_houio(input_path, output_path, executable=converter)
        return geometry_from_bgeo_bytes(output_path.read_bytes())


def roundtrip_current_sop(*, executable: Optional[PathLike] = None) -> hou.Geometry:
    """Replace the current writable Python SOP geometry with HouIO output.

    Args:
        executable: Optional explicit path to `houio_convert`.

    Returns:
        The current Python SOP's modifiable geometry.

    Raises:
        hou.GeometryPermissionError: If called outside a writable Python SOP
            cook context.
    """
    current_node = hou.pwd()
    processed = roundtrip_node_geometry(current_node, executable=executable)
    target = current_node.geometry()
    target.clear()
    target.merge(processed)
    return target
