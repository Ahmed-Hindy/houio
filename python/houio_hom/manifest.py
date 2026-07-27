"""Direct HOM extraction into the HouIO-owned interchange manifest."""

from __future__ import annotations

import json
import math
import struct
from pathlib import Path
from typing import Any, Iterable, Union

import hou

PathLike = Union[str, Path]
_SCHEMA = "houio.hom/1"
_MAX_VDB_SCAN_VOXELS = 16 * 1024 * 1024


class UnsupportedHOMDataError(ValueError):
    """Raised when live geometry contains a record the manifest cannot represent."""


def _flatten(value: Any) -> list[Any]:
    """Return one scalar or tuple-like value as a flat list."""
    if isinstance(value, (str, bytes, dict)):
        return [value]
    try:
        return list(value)
    except TypeError:
        return [value]


def _float16_bits(values: Iterable[float]) -> list[int]:
    """Encode Python floats as IEEE-754 binary16 bit patterns."""
    return [struct.unpack("<H", struct.pack("<e", float(value)))[0] for value in values]


def _numeric_storage(attribute: hou.Attrib) -> str:
    """Map a HOM numeric attribute type to HouIO storage.

    Houdini 20.0 does not expose ``numericDataType``. In that version, use
    Float64 or Int64 so values are not narrowed based on an unverifiable guess.
    """
    if not hasattr(attribute, "numericDataType"):
        data_type = str(attribute.dataType())
        if data_type.endswith("Float"):
            return "float64"
        if data_type.endswith("Int"):
            return "int64"
        raise UnsupportedHOMDataError(
            f"Attribute {attribute.name()!r} is not a supported numeric attribute"
        )
    numeric_type = str(attribute.numericDataType())
    if numeric_type.endswith("Float16"):
        return "float16"
    if numeric_type.endswith("Float64"):
        return "float64"
    if numeric_type.endswith("Float32"):
        return "float32"
    if numeric_type.endswith("Int64"):
        return "int64"
    if numeric_type.endswith(("Int8", "Int16", "Int32")):
        return "int32"
    raise UnsupportedHOMDataError(
        f"Attribute {attribute.name()!r} uses unsupported numeric type {numeric_type}"
    )


def _domain_elements(geometry: hou.Geometry, domain: str) -> tuple[Any, ...]:
    """Return HOM elements for one attribute domain."""
    if domain == "point":
        return tuple(geometry.points())
    if domain == "vertex":
        return tuple(
            vertex
            for primitive in geometry.prims()
            for vertex in primitive.vertices()
        )
    if domain == "primitive":
        return tuple(geometry.prims())
    if domain == "global":
        return (geometry,)
    raise ValueError(f"Unknown attribute domain: {domain}")


def _domain_element_count(geometry: hou.Geometry, domain: str) -> int:
    """Return one attribute-domain count without materializing its elements."""
    if domain == "point":
        return len(geometry.points())
    if domain == "vertex":
        return _vertex_count(geometry)
    if domain == "primitive":
        return len(geometry.prims())
    if domain == "global":
        return 1
    raise ValueError(f"Unknown attribute domain: {domain}")


def _numeric_values(
    geometry: hou.Geometry, attribute: hou.Attrib, domain: str
) -> list[int | float]:
    """Extract flattened numeric values without using a Houdini file writer."""
    name = attribute.name()
    data_type = str(attribute.dataType())
    if domain == "global":
        return _flatten(geometry.attribValue(attribute))

    prefix = {
        "point": "point",
        "vertex": "vertex",
        "primitive": "prim",
    }[domain]
    if data_type.endswith("Float"):
        values = getattr(geometry, f"{prefix}FloatAttribValues")(name)
    elif data_type.endswith("Int"):
        values = getattr(geometry, f"{prefix}IntAttribValues")(name)
    else:
        raise UnsupportedHOMDataError(
            f"Attribute {name!r} is not a supported numeric attribute"
        )
    return list(values)


def _string_values(
    geometry: hou.Geometry, attribute: hou.Attrib, domain: str
) -> list[str]:
    """Extract flattened string values from one HOM domain."""
    if domain == "global":
        return [str(value) for value in _flatten(geometry.attribValue(attribute))]
    prefix = {
        "point": "point",
        "vertex": "vertex",
        "primitive": "prim",
    }[domain]
    return list(getattr(geometry, f"{prefix}StringAttribValues")(attribute.name()))


def _dictionary_values(
    geometry: hou.Geometry, attribute: hou.Attrib, domain: str
) -> list[dict[str, Any]]:
    """Extract dictionary values element by element."""
    values: list[dict[str, Any]] = []
    for element in _domain_elements(geometry, domain):
        value = element.attribValue(attribute)
        if not isinstance(value, dict):
            raise UnsupportedHOMDataError(
                f"Dictionary attribute {attribute.name()!r} returned {type(value).__name__}"
            )
        values.append(value)
    return values


def _attribute_manifest(
    geometry: hou.Geometry,
    attribute: hou.Attrib,
    domain: str,
) -> dict[str, Any]:
    """Convert one HOM attribute definition and payload to manifest data."""
    if attribute.isArrayType():
        raise UnsupportedHOMDataError(
            f"Array attribute {attribute.name()!r} is not supported by the HOM manifest"
        )

    definition: dict[str, Any] = {
        "name": attribute.name(),
        "tuple_size": int(attribute.size()),
        "element_count": _domain_element_count(geometry, domain),
        "scope": "public",
    }
    data_type = str(attribute.dataType())
    if data_type.endswith(("Float", "Int")):
        storage = _numeric_storage(attribute)
        values = _numeric_values(geometry, attribute, domain)
        definition.update({"kind": "numeric", "storage": storage})
        definition["values"] = (
            _float16_bits(values) if storage == "float16" else values
        )
    elif data_type.endswith("String"):
        definition.update(
            {
                "kind": "string",
                "values": _string_values(geometry, attribute, domain),
            }
        )
    elif data_type.endswith("Dict"):
        if attribute.size() != 1:
            raise UnsupportedHOMDataError(
                f"Dictionary tuple attribute {attribute.name()!r} is unsupported"
            )
        definition.update(
            {
                "kind": "dictionary",
                "values": _dictionary_values(geometry, attribute, domain),
            }
        )
    else:
        raise UnsupportedHOMDataError(
            f"Attribute {attribute.name()!r} uses unsupported data type {data_type}"
        )
    return definition


def _position_manifest(geometry: hou.Geometry) -> dict[str, Any]:
    """Return canonical float32 four-tuple point positions.

    HOM point positions are intentionally normalized to Houdini's canonical
    ``P`` interchange representation, even when the source attribute reports a
    wider numeric storage type.
    """
    positions: list[float] = []
    points = geometry.points()
    for point in points:
        position = point.position()
        positions.extend(
            (float(position[0]), float(position[1]), float(position[2]), 1.0)
        )
    return {
        "name": "P",
        "kind": "numeric",
        "storage": "float32",
        "tuple_size": 4,
        "element_count": len(points),
        "scope": "public",
        "values": positions,
    }


def _attributes(geometry: hou.Geometry) -> dict[str, list[dict[str, Any]]]:
    """Extract all maintained attribute domains."""
    result: dict[str, list[dict[str, Any]]] = {
        "point": [],
        "vertex": [],
        "primitive": [],
        "global": [],
    }
    domain_attributes = {
        "point": geometry.pointAttribs(),
        "vertex": geometry.vertexAttribs(),
        "primitive": geometry.primAttribs(),
        "global": geometry.globalAttribs(),
    }
    for domain, attributes in domain_attributes.items():
        for attribute in attributes:
            if domain == "point" and attribute.name() == "P":
                result[domain].append(_position_manifest(geometry))
                continue
            result[domain].append(_attribute_manifest(geometry, attribute, domain))
    if not any(attribute["name"] == "P" for attribute in result["point"]):
        result["point"].insert(0, _position_manifest(geometry))
    return result


def _vertex_count(geometry: hou.Geometry) -> int:
    """Return the geometry-wide vertex count across maintained Houdini versions."""
    if hasattr(geometry, "vertexCount"):
        return int(geometry.vertexCount())
    return sum(len(primitive.vertices()) for primitive in geometry.prims())


def _topology(geometry: hou.Geometry) -> list[int]:
    """Extract the point reference for every global vertex number."""
    topology = [-1] * _vertex_count(geometry)
    for primitive in geometry.prims():
        for vertex in primitive.vertices():
            topology[vertex.linearNumber()] = vertex.point().number()
    if any(point_number < 0 for point_number in topology):
        raise ValueError("HOM geometry contains an unassigned global vertex")
    return topology


def _volume_local_to_world(primitive: hou.Volume) -> list[float]:
    """Reconstruct HouIO's normalized-local volume transform from HOM data."""
    vertices = primitive.vertices()
    if len(vertices) != 1:
        raise ValueError(
            f"Dense volume primitive {primitive.number()} does not have one topology vertex"
        )
    center = vertices[0].point().position()
    matrix = (
        hou.hmath.buildScale(2.0, 2.0, 2.0)
        * hou.hmath.buildTranslate(-1.0, -1.0, -1.0)
        * hou.Matrix4(primitive.transform())
        * hou.hmath.buildTranslate(center)
    )
    return [float(value) for value in matrix.asTuple()]


def _sparse_vdb_manifest(primitive: hou.VDB, vertex_offset: int) -> dict[str, Any]:
    """Extract one exact scalar Float VDB without serializing through Houdini."""
    value_type = str(primitive.intrinsicValue("vdb_value_type"))
    if value_type != "float":
        raise UnsupportedHOMDataError(
            f"Native VDB value type {value_type!r} is not a scalar FloatGrid"
        )
    if not math.isclose(float(primitive.intrinsicValue("taper")), 1.0):
        raise UnsupportedHOMDataError(
            "Tapered/frustum VDB transforms are not representable by SparseFloatGrid"
        )
    if bool(primitive.intrinsicValue("vdb_is_local_space")):
        raise UnsupportedHOMDataError(
            "Local-space VDB metadata is not yet represented by SparseFloatGrid"
        )
    if bool(primitive.intrinsicValue("vdb_is_saved_as_half_float")):
        raise UnsupportedHOMDataError(
            "Half-float VDB serialization policy is not yet represented by SparseFloatGrid"
        )

    background = float(primitive.intrinsicValue("background"))
    if not math.isfinite(background):
        raise UnsupportedHOMDataError("Native VDB background must be finite")
    transform = [float(value) for value in primitive.intrinsicValue("transform")]
    if len(transform) != 16 or any(not math.isfinite(value) for value in transform):
        raise UnsupportedHOMDataError(
            "Native VDB transform must be a finite 4x4 linear matrix"
        )

    active_indices: list[int] = []
    active_values: list[float] = []
    active_count = int(primitive.activeVoxelCount())
    if active_count:
        bounds = primitive.activeVoxelBoundingBox()
        minimum = [int(value) for value in bounds.minvec()]
        maximum = [int(value) for value in bounds.maxvec()]
        dimensions = [maximum[index] - minimum[index] for index in range(3)]
        if any(value <= 0 for value in dimensions):
            raise UnsupportedHOMDataError(
                "Native VDB active bounding box is inconsistent with its active count"
            )
        scan_count = dimensions[0] * dimensions[1] * dimensions[2]
        if scan_count > _MAX_VDB_SCAN_VOXELS:
            raise UnsupportedHOMDataError(
                "Native VDB active bounding box exceeds the direct extraction limit "
                f"({_MAX_VDB_SCAN_VOXELS} voxels)"
            )
        values = tuple(float(value) for value in primitive.voxelRangeAsFloat(bounds))
        if len(values) != scan_count:
            raise UnsupportedHOMDataError(
                "Native VDB voxel range size does not match its active bounding box"
            )
        value_offset = 0
        for z_index in range(minimum[2], maximum[2]):
            for y_index in range(minimum[1], maximum[1]):
                for x_index in range(minimum[0], maximum[0]):
                    value = values[value_offset]
                    value_offset += 1
                    if not math.isfinite(value):
                        raise UnsupportedHOMDataError(
                            "Native VDB contains a non-finite voxel value"
                        )
                    if value != background:
                        active_indices.extend((x_index, y_index, z_index))
                        active_values.append(value)
        if len(active_values) != active_count:
            raise UnsupportedHOMDataError(
                "HOM cannot expose this VDB's exact active topology: the number of "
                "voxels differing from the background does not equal activeVoxelCount. "
                "This includes grids with active background-valued voxels, inactive "
                "interior values, or active tiles. Use file-level payload preservation."
            )

    class_name = str(primitive.intrinsicValue("vdb_class"))
    class_map = {
        "fog volume": "fog_volume",
        "level set": "level_set",
        "unknown": "unknown",
    }
    if class_name not in class_map:
        raise UnsupportedHOMDataError(
            f"Native VDB class {class_name!r} is unsupported"
        )

    name = ""
    name_attribute = primitive.geometry().findPrimAttrib("name")
    if name_attribute is not None:
        name = str(primitive.stringAttribValue("name"))
    metadata: dict[str, str] = {}
    creator = str(primitive.intrinsicValue("vdb_creator"))
    if creator:
        metadata["creator"] = creator

    return {
        "type": "sparse_float_vdb",
        "vertex_offset": vertex_offset,
        "name": name,
        "grid_class": class_map[class_name],
        "background": background,
        "index_to_world": transform,
        "active_indices": active_indices,
        "active_values": active_values,
        "metadata": metadata,
    }


def _primitive_manifest(primitive: hou.Prim) -> dict[str, Any]:
    """Extract one supported primitive without invoking a Houdini file writer."""
    vertices = primitive.vertices()
    vertex_offset = vertices[0].linearNumber() if vertices else 0
    type_name = primitive.type().name()
    packed_type_name = (
        str(primitive.intrinsicValue("packedtypename"))
        if "packedtypename" in primitive.intrinsicNames()
        else type_name
    )
    if packed_type_name == "PackedDiskSequence":
        filenames = [str(value) for value in primitive.intrinsicValue("filenames")]
        if not filenames or any(not filename for filename in filenames):
            raise UnsupportedHOMDataError(
                "hou.PackedPrim PackedDiskSequence has no valid sample filenames"
            )
        return {
            "type": "packed_disk_sequence",
            "vertex_offset": vertex_offset,
            "filenames": filenames,
            "index": float(primitive.intrinsicValue("index")),
            "wrap": str(primitive.intrinsicValue("wrap")),
            "pivot": [float(value) for value in primitive.intrinsicValue("pivot")],
            "transform": [
                float(value) for value in primitive.intrinsicValue("transform")
            ],
            "viewport_lod": str(primitive.intrinsicValue("viewportlod")),
            "point_instance_transform": bool(
                primitive.intrinsicValue("pointinstancetransform")
            ),
        }
    if packed_type_name == "PackedDisk":
        authored_filename = str(primitive.intrinsicValue("unexpandedfilename"))
        if not authored_filename:
            authored_filename = str(primitive.intrinsicValue("filename"))
        if not authored_filename:
            raise UnsupportedHOMDataError(
                "hou.PackedPrim PackedDisk has no authored filename"
            )
        return {
            "type": "packed_disk",
            "vertex_offset": vertex_offset,
            "filename": authored_filename,
            "expand_frame": float(primitive.intrinsicValue("expandframe")),
            "expand_filename": bool(primitive.intrinsicValue("expandfilename")),
            "pivot": [float(value) for value in primitive.intrinsicValue("pivot")],
            "transform": [
                float(value) for value in primitive.intrinsicValue("transform")
            ],
            "viewport_lod": str(primitive.intrinsicValue("viewportlod")),
            "point_instance_transform": bool(
                primitive.intrinsicValue("pointinstancetransform")
            ),
            "treat_as_folder": bool(primitive.intrinsicValue("treatasfolder")),
        }
    if isinstance(primitive, hou.PackedFragment):
        raise UnsupportedHOMDataError(
            "hou.PackedFragment file records are supported by HouIO, but HOM does "
            "not expose the fragment's embedded source detail for lossless direct "
            "manifest extraction; write an existing BGEO record or construct a "
            "houio.hom/1 packed_fragment manifest explicitly"
        )
    if isinstance(primitive, hou.PackedGeometry):
        return {
            "type": "packed_geometry",
            "vertex_offset": vertex_offset,
            "pivot": [float(value) for value in primitive.intrinsicValue("pivot")],
            "transform": [
                float(value) for value in primitive.intrinsicValue("transform")
            ],
            "viewport_lod": str(primitive.intrinsicValue("viewportlod")),
            "point_instance_transform": bool(
                primitive.intrinsicValue("pointinstancetransform")
            ),
            "treat_as_folder": bool(primitive.intrinsicValue("treatasfolder")),
            "embedded_manifest": geometry_manifest(primitive.getEmbeddedGeometry()),
        }
    if isinstance(primitive, hou.VDB):
        return _sparse_vdb_manifest(primitive, vertex_offset)
    if isinstance(primitive, hou.Volume):
        return {
            "type": "dense_volume",
            "vertex_offset": vertex_offset,
            "resolution": [int(value) for value in primitive.resolution()],
            "local_to_world": _volume_local_to_world(primitive),
            "voxels": [float(value) for value in primitive.allVoxels()],
            "visualization": {
                "mode": str(primitive.intrinsicValue("volumevisualmode")),
                "iso": float(primitive.intrinsicValue("volumevisualiso")),
                "density": float(primitive.intrinsicValue("volumevisualdensity")),
            },
        }
    if isinstance(primitive, hou.Polygon):
        return {
            "type": "polygon",
            "vertex_offset": vertex_offset,
            "vertex_count": len(vertices),
            "closed": bool(primitive.isClosed()),
        }

    if "Packed" in packed_type_name or "packed" in packed_type_name:
        raise UnsupportedHOMDataError(
            f"Packed primitive {packed_type_name!r} is recognized, but direct HOM extraction "
            "supports embedded hou.PackedGeometry, PackedDisk references, and "
            "PackedDiskSequence references; "
            "PackedFragment is supported at the file and explicit-manifest layers"
        )
    raise UnsupportedHOMDataError(
        f"Primitive {primitive.number()} uses unsupported type {type_name!r}"
    )


def _group_indices(group: Any, domain: str) -> list[int]:
    """Return sorted element indices for one HOM group."""
    if domain == "point":
        return sorted(point.number() for point in group.points())
    if domain == "vertex":
        return sorted(vertex.linearNumber() for vertex in group.vertices())
    if domain == "primitive":
        return sorted(primitive.number() for primitive in group.prims())
    raise ValueError(f"Unknown group domain: {domain}")


def _groups(geometry: hou.Geometry) -> dict[str, dict[str, list[int]]]:
    """Extract maintained point, vertex, and primitive groups."""
    result: dict[str, dict[str, list[int]]] = {
        "point": {},
        "vertex": {},
        "primitive": {},
    }
    domain_groups = {
        "point": geometry.pointGroups(),
        "vertex": geometry.vertexGroups(),
        "primitive": geometry.primGroups(),
    }
    for domain, groups in domain_groups.items():
        for group in groups:
            result[domain][group.name()] = _group_indices(group, domain)
    return result


def geometry_manifest(geometry: hou.Geometry) -> dict[str, Any]:
    """Extract supported live HOM geometry into the HouIO manifest schema.

    Args:
        geometry: Cooked HOM geometry to inspect directly.

    Returns:
        JSON-serializable HouIO manifest data.

    Raises:
        UnsupportedHOMDataError: If an attribute or primitive record cannot be
            represented without loss.
    """
    primitives = [_primitive_manifest(primitive) for primitive in geometry.prims()]
    return {
        "schema": _SCHEMA,
        "point_count": len(geometry.points()),
        "vertex_count": _vertex_count(geometry),
        "primitive_count": len(geometry.prims()),
        "topology": _topology(geometry),
        "primitives": primitives,
        "attributes": _attributes(geometry),
        "groups": _groups(geometry),
    }


def write_manifest(geometry: hou.Geometry, path: PathLike) -> Path:
    """Write a direct-HOM manifest atomically.

    Args:
        geometry: Cooked HOM geometry.
        path: Destination JSON path.

    Returns:
        Resolved manifest path.
    """
    destination = Path(path).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    temporary.write_text(
        json.dumps(geometry_manifest(geometry), separators=(",", ":")),
        encoding="utf-8",
    )
    temporary.replace(destination)
    return destination
