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
DEFAULT_CONVERTER_TIMEOUT_SECONDS = 300.0
VDB_CLASS_ATTRIBUTE = "houio_vdb_class"
VDB_LEVEL_SET_CLASS = "level set"
VDB_FOG_VOLUME_CLASS = "fog volume"
_SUPPORTED_VDB_CLASSES = frozenset((VDB_LEVEL_SET_CLASS, VDB_FOG_VOLUME_CLASS))


class HouIOConverterError(subprocess.CalledProcessError):
    """Converter failure that includes captured HouIO diagnostics in its message."""

    def __str__(self) -> str:
        message = super().__str__()
        details = (self.stderr or self.output or "").strip()
        return f"{message}\n{details}" if details else message


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
                root / "bin" / "houio_convert.exe",
                root / "bin" / "houio_convert",
                root / "build" / "windows-msvc-release" / "houio_convert.exe",
                root
                / "build"
                / "windows-msvc-release"
                / "Release"
                / "houio_convert.exe",
                root / "build" / "windows-gcc-mingw" / "houio_convert.exe",
                root / "build" / "windows-gcc-mingw" / "Release" / "houio_convert.exe",
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


def _ensure_vdb_class_attribute(geometry: hou.Geometry) -> hou.Attrib:
    """Return the primitive attribute used to preserve VDB grid classes."""
    attribute = geometry.findPrimAttrib(VDB_CLASS_ATTRIBUTE)
    if attribute is None:
        attribute = geometry.addAttrib(hou.attribType.Prim, VDB_CLASS_ATTRIBUTE, "")
    return attribute


def _validated_vdb_class(value: object, context: str) -> str:
    """Validate and normalize a supported VDB grid class.

    Args:
        value: Candidate Houdini VDB class value.
        context: Human-readable primitive description for diagnostics.

    Returns:
        Canonical Houdini VDB class string.

    Raises:
        ValueError: If the class is neither level set nor fog volume.
    """
    grid_class = str(value)
    if grid_class not in _SUPPORTED_VDB_CLASSES:
        raise ValueError(
            f"{context} has unsupported VDB class {grid_class!r}; "
            "HouIO supports level set and fog volume grids"
        )
    return grid_class


def _tag_vdb_classes(geometry: hou.Geometry) -> int:
    """Store each VDB primitive's authoritative class as a string attribute."""
    class_attribute = _ensure_vdb_class_attribute(geometry)
    tagged_count = 0
    for primitive in geometry.prims():
        if not isinstance(primitive, hou.VDB):
            continue
        grid_class = _validated_vdb_class(
            primitive.intrinsicValue("vdb_class"),
            f"VDB primitive {primitive.number()}",
        )
        primitive.setAttribValue(class_attribute, grid_class)
        tagged_count += 1
    return tagged_count


def _dense_volume_class(
    primitive: hou.Volume, class_attribute: Optional[hou.Attrib]
) -> str:
    """Resolve a dense volume's SDF or fog semantic class."""
    if class_attribute is not None:
        tagged_class = primitive.attribValue(class_attribute)
        if tagged_class:
            return _validated_vdb_class(
                tagged_class,
                f"Dense volume primitive {primitive.number()}",
            )

    visualization_mode = str(primitive.intrinsicValue("volumevisualmode"))
    if visualization_mode == "iso":
        return VDB_LEVEL_SET_CLASS
    if visualization_mode == "smoke":
        return VDB_FOG_VOLUME_CLASS
    raise ValueError(
        f"Dense volume primitive {primitive.number()} has visualization mode "
        f"{visualization_mode!r}; use iso for SDF or smoke for fog semantics"
    )


def _tag_dense_volume_classes(geometry: hou.Geometry) -> int:
    """Tag dense volumes using explicit metadata or their visualization mode."""
    class_attribute = _ensure_vdb_class_attribute(geometry)
    tagged_count = 0
    for primitive in geometry.prims():
        if not isinstance(primitive, hou.Volume) or isinstance(primitive, hou.VDB):
            continue
        grid_class = _dense_volume_class(primitive, class_attribute)
        primitive.setAttribValue(class_attribute, grid_class)
        tagged_count += 1
    return tagged_count


def _apply_dense_volume_semantics(geometry: hou.Geometry, expected_count: int) -> None:
    """Restore dense-volume visualization from preserved VDB class metadata."""
    class_attribute = geometry.findPrimAttrib(VDB_CLASS_ATTRIBUTE)
    restored_count = 0
    if class_attribute is not None:
        for primitive in geometry.prims():
            if not isinstance(primitive, hou.Volume) or isinstance(primitive, hou.VDB):
                continue
            tagged_class = primitive.attribValue(class_attribute)
            if not tagged_class:
                continue
            grid_class = _validated_vdb_class(
                tagged_class,
                f"Dense volume primitive {primitive.number()}",
            )
            visualization_mode = "iso" if grid_class == VDB_LEVEL_SET_CLASS else "smoke"
            primitive.setIntrinsicValue("volumevisualmode", visualization_mode)
            restored_count += 1
    if restored_count != expected_count:
        raise ValueError(
            "Convert VDB did not preserve class metadata for every converted grid"
        )


def _restore_vdb_classes(geometry: hou.Geometry, expected_count: int) -> None:
    """Restore VDB class intrinsics after converting tagged dense volumes."""
    class_attribute = geometry.findPrimAttrib(VDB_CLASS_ATTRIBUTE)
    restored_count = 0
    if class_attribute is not None:
        for primitive in geometry.prims():
            if not isinstance(primitive, hou.VDB):
                continue
            grid_class = _validated_vdb_class(
                primitive.attribValue(class_attribute),
                f"VDB primitive {primitive.number()}",
            )
            primitive.setIntrinsicValue("vdb_class", grid_class)
            restored_count += 1
    if restored_count != expected_count:
        raise ValueError(
            "Convert VDB did not preserve class metadata for every dense volume"
        )


def convert_vdb_to_volume(geometry: hou.Geometry) -> hou.Geometry:
    """Convert every VDB primitive to a dense Houdini volume.

    Args:
        geometry: Source geometry containing one or more 32-bit Float VDB
            primitives. Unrelated primitives are preserved.

    Returns:
        Independent geometry with Float VDB primitives converted to dense volumes.
        Level-set grids become iso volumes and fog grids become smoke volumes.
        The original class is retained in ``houio_vdb_class`` metadata.

    Raises:
        ValueError: If the source contains no VDB primitive, contains another VDB
            storage type, or conversion does not produce the expected volumes.
        hou.OperationFailed: If Houdini cannot execute the Convert VDB verb.
    """
    polygon_count, volume_count, vdb_count = _primitive_counts(geometry)
    if vdb_count == 0:
        raise ValueError("Geometry contains no VDB primitives")
    unsupported_types = sorted(
        {
            str(primitive.intrinsicValue("vdb_value_type"))
            for primitive in geometry.prims()
            if isinstance(primitive, hou.VDB)
            and primitive.dataType() != hou.vdbData.Float
        }
    )
    if unsupported_types:
        raise ValueError(
            "HouIO densification supports only 32-bit float VDB grids; found "
            + ", ".join(unsupported_types)
        )

    tagged_geometry = _copy_geometry(geometry)
    if _tag_dense_volume_classes(tagged_geometry) != volume_count:
        raise ValueError("Could not classify every existing dense volume")
    if _tag_vdb_classes(tagged_geometry) != vdb_count:
        raise ValueError("Could not preserve every VDB grid class before conversion")

    verb = hou.sopNodeTypeCategory().nodeVerb("convertvdb")
    if verb is None:
        raise hou.OperationFailed("The Convert VDB SOP verb is unavailable")
    verb.setParms({"conversion": 0})

    result = hou.Geometry()
    verb.execute(result, [tagged_geometry])
    result_polygon_count, result_volume_count, remaining_vdb_count = _primitive_counts(
        result
    )
    if (
        result_polygon_count != polygon_count
        or result_volume_count != volume_count + vdb_count
        or remaining_vdb_count != 0
        or len(result.prims()) != len(geometry.prims())
    ):
        raise ValueError(
            "Convert VDB did not preserve non-VDB primitives or produce the expected dense volumes"
        )
    _apply_dense_volume_semantics(result, volume_count + vdb_count)
    return result


def convert_volume_to_vdb(geometry: hou.Geometry) -> hou.Geometry:
    """Convert every dense Houdini volume to an OpenVDB primitive.

    Args:
        geometry: Source geometry containing one or more dense volumes.

    Returns:
        Independent geometry containing Float VDB primitives suitable for `.vdb`
        output. Iso dense volumes become level sets and smoke volumes become fog
        grids unless explicit ``houio_vdb_class`` metadata overrides the mode.

    Raises:
        ValueError: If the source contains no dense volume or conversion leaves
            non-VDB primitives in the result.
        hou.OperationFailed: If Houdini cannot execute the Convert VDB verb.
    """
    polygon_count, volume_count, vdb_count = _primitive_counts(geometry)
    if volume_count == 0:
        if vdb_count > 0 and polygon_count == 0 and vdb_count == len(geometry.prims()):
            result = _copy_geometry(geometry)
            for primitive in result.prims():
                if not isinstance(primitive, hou.VDB):
                    continue
                if primitive.dataType() != hou.vdbData.Float:
                    raise ValueError(
                        "HouIO VDB output supports only 32-bit float grids"
                    )
                _validated_vdb_class(
                    primitive.intrinsicValue("vdb_class"),
                    f"VDB primitive {primitive.number()}",
                )
            return result
        raise ValueError("Geometry contains no dense volume primitives")
    if polygon_count != 0 or vdb_count != 0 or volume_count != len(geometry.prims()):
        raise ValueError("VDB output requires geometry containing only dense volumes")

    tagged_geometry = _copy_geometry(geometry)
    if _tag_dense_volume_classes(tagged_geometry) != volume_count:
        raise ValueError("Could not classify every dense volume before VDB conversion")

    verb = hou.sopNodeTypeCategory().nodeVerb("convertvdb")
    if verb is None:
        raise hou.OperationFailed("The Convert VDB SOP verb is unavailable")
    verb.setParms({"conversion": 1})

    result = hou.Geometry()
    verb.execute(result, [tagged_geometry])
    _, remaining_volume_count, result_vdb_count = _primitive_counts(result)
    if remaining_volume_count != 0 or result_vdb_count != len(result.prims()):
        raise ValueError(".vdb output requires all primitives to be VDB grids")
    _restore_vdb_classes(result, volume_count)
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
    enabled, 32-bit Float VDB primitives are converted to dense volumes because
    HouIO's native VDB primitive model is not implemented yet. Unrelated Houdini
    primitives are preserved.

    Args:
        path: Geometry file supported by Houdini's File SOP.
        convert_vdb: Convert VDB primitives to dense volumes.

    Returns:
        Independent geometry suitable for `geometry_to_bgeo_bytes()` or an
        uncompressed `.bgeo` intermediate.
    """
    geometry = hou.Geometry()
    geometry.loadFromFile(str(Path(path)))
    _, volume_count, vdb_count = _primitive_counts(geometry)
    if vdb_count > 0 and convert_vdb:
        geometry = convert_vdb_to_volume(geometry)
        _, volume_count, _ = _primitive_counts(geometry)
    if volume_count > 0:
        _tag_dense_volume_classes(geometry)
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
    timeout_seconds: Optional[float] = DEFAULT_CONVERTER_TIMEOUT_SECONDS,
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
        timeout_seconds: Maximum converter runtime, or `None` to disable the timeout.

    Raises:
        FileNotFoundError: If `houio_convert` cannot be resolved.
        subprocess.CalledProcessError: If HouIO reports a conversion failure.
        subprocess.TimeoutExpired: If the converter exceeds `timeout_seconds`.
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
            timeout=timeout_seconds,
        )
        if completed.returncode != 0:
            raise HouIOConverterError(
                completed.returncode,
                completed.args,
                output=completed.stdout,
                stderr=completed.stderr,
            )

        if destination.suffix.lower() == ".vdb":
            save_from_houio(converter_output.read_bytes(), destination)


def roundtrip_node_geometry(
    node: hou.SopNode,
    *,
    executable: Optional[PathLike] = None,
    timeout_seconds: Optional[float] = DEFAULT_CONVERTER_TIMEOUT_SECONDS,
) -> hou.Geometry:
    """Round-trip a cooked SOP node through HouIO and return new geometry.

    Args:
        node: SOP node whose cooked geometry should be processed.
        executable: Optional explicit path to `houio_convert`.
        timeout_seconds: Maximum converter runtime, or `None` to disable the timeout.

    Returns:
        Newly allocated geometry loaded from HouIO's output. Float VDB grids are
        returned as dense volumes because HouIO has no sparse VDB model. Level
        sets retain iso/SDF semantics and fog grids retain smoke/fog semantics.
    """
    converter = _resolve_converter(executable)
    source_geometry = _copy_geometry(node.geometry())
    _, volume_count, vdb_count = _primitive_counts(source_geometry)
    if vdb_count > 0:
        source_geometry = convert_vdb_to_volume(source_geometry)
        _, volume_count, _ = _primitive_counts(source_geometry)
    if volume_count > 0:
        _tag_dense_volume_classes(source_geometry)

    with tempfile.TemporaryDirectory(prefix="houio_node_") as temporary_directory:
        temporary_root = Path(temporary_directory)
        input_path = temporary_root / "input.bgeo"
        output_path = temporary_root / "output.bgeo"
        input_path.write_bytes(geometry_to_bgeo_bytes(source_geometry))
        convert_with_houio(
            input_path,
            output_path,
            executable=converter,
            timeout_seconds=timeout_seconds,
        )
        return geometry_from_bgeo_bytes(output_path.read_bytes())


def roundtrip_current_sop(
    *,
    executable: Optional[PathLike] = None,
    timeout_seconds: Optional[float] = DEFAULT_CONVERTER_TIMEOUT_SECONDS,
) -> hou.Geometry:
    """Replace the current writable Python SOP geometry with HouIO output.

    Args:
        executable: Optional explicit path to `houio_convert`.
        timeout_seconds: Maximum converter runtime, or `None` to disable the timeout.

    Returns:
        The current Python SOP's modifiable geometry. Float VDB primitives are
        replaced with dense volumes while preserving SDF or fog semantics.

    Raises:
        hou.GeometryPermissionError: If called outside a writable Python SOP
            cook context.
    """
    current_node = hou.pwd()
    processed = roundtrip_node_geometry(
        current_node,
        executable=executable,
        timeout_seconds=timeout_seconds,
    )
    target = current_node.geometry()
    target.clear()
    target.merge(processed)
    return target
