"""Compare selected geometry attributes through Houdini."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

import hou


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Reference geometry file")
    parser.add_argument("candidate", type=Path, help="Geometry file to compare")
    parser.add_argument("--point-attribute", action="append", default=[])
    parser.add_argument("--vertex-attribute", action="append", default=[])
    parser.add_argument("--primitive-string-attribute", action="append", default=[])
    parser.add_argument("--primitive-int-attribute", action="append", default=[])
    parser.add_argument("--topology", action="store_true", help="Compare polygon topology and closed state")
    parser.add_argument("--tolerance", type=float, default=0.0)
    return parser.parse_args()


def load_geometry(path: Path, node_name: str) -> hou.Geometry:
    """Load a geometry file through a Houdini File SOP.

    Args:
        path: Geometry file path.
        node_name: Unique node-name prefix.

    Returns:
        Cooked Houdini geometry.

    Raises:
        FileNotFoundError: If the geometry file does not exist.
        RuntimeError: If Houdini cannot load the geometry.
    """
    resolved_path = path.resolve()
    if not resolved_path.is_file():
        raise FileNotFoundError(resolved_path)

    object_context = hou.node("/obj")
    if object_context is None:
        raise RuntimeError("Houdini object context /obj is unavailable.")

    geometry_container = object_context.createNode("geo", node_name, run_init_scripts=False)
    file_node = geometry_container.createNode("file", f"{node_name}_file")
    file_node.parm("file").set(str(resolved_path))
    try:
        file_node.cook(force=True)
    except hou.OperationFailed as error:
        details = "\n".join(file_node.errors()) or str(error)
        raise RuntimeError(f"Houdini failed to load {resolved_path}:\n{details}") from error

    return file_node.geometry().freeze()


def compare_float_values(domain: str, attribute_name: str, source_values: Sequence[float],
                         candidate_values: Sequence[float], tolerance: float) -> dict[str, object]:
    """Compare two flat floating-point attribute arrays.

    Args:
        domain: Houdini attribute domain name.
        attribute_name: Attribute to compare.
        source_values: Reference values.
        candidate_values: Candidate values.
        tolerance: Maximum permitted absolute difference.

    Returns:
        Comparison summary.

    Raises:
        RuntimeError: If array lengths differ or a value exceeds tolerance.
    """
    if len(source_values) != len(candidate_values):
        raise RuntimeError(
            f"{domain} attribute {attribute_name!r} length mismatch: "
            f"{len(source_values)} != {len(candidate_values)}"
        )

    maximum_difference = 0.0
    maximum_index = -1
    for index, (source_value, candidate_value) in enumerate(
        zip(source_values, candidate_values)
    ):
        difference = abs(source_value - candidate_value)
        if difference > maximum_difference:
            maximum_difference = difference
            maximum_index = index

    if maximum_difference > tolerance:
        raise RuntimeError(
            f"{domain} attribute {attribute_name!r} differs at flat index {maximum_index}: "
            f"maximum absolute difference {maximum_difference} exceeds {tolerance}"
        )

    return {
        "domain": domain,
        "attribute": attribute_name,
        "value_count": len(source_values),
        "maximum_absolute_difference": maximum_difference,
    }


def compare_exact_values(domain: str, attribute_name: str, source_values: Sequence[object],
                         candidate_values: Sequence[object]) -> dict[str, object]:
    """Compare two attribute arrays exactly.

    Args:
        domain: Houdini attribute domain name.
        attribute_name: Attribute to compare.
        source_values: Reference values.
        candidate_values: Candidate values.

    Returns:
        Comparison summary.

    Raises:
        RuntimeError: If array lengths or values differ.
    """
    if len(source_values) != len(candidate_values):
        raise RuntimeError(
            f"{domain} attribute {attribute_name!r} length mismatch: "
            f"{len(source_values)} != {len(candidate_values)}"
        )

    for index, (source_value, candidate_value) in enumerate(
        zip(source_values, candidate_values)
    ):
        if source_value != candidate_value:
            raise RuntimeError(
                f"{domain} attribute {attribute_name!r} differs at index {index}: "
                f"{source_value!r} != {candidate_value!r}"
            )

    return {
        "domain": domain,
        "attribute": attribute_name,
        "value_count": len(source_values),
        "exact_match": True,
    }


def compare_topology(source_geometry: hou.Geometry, candidate_geometry: hou.Geometry) -> dict[str, object]:
    """Compare primitive topology and closed state exactly.

    Args:
        source_geometry: Reference geometry.
        candidate_geometry: Candidate geometry.

    Returns:
        Topology comparison summary.

    Raises:
        RuntimeError: If primitive types, closed state, or point indices differ.
    """
    source_primitives = source_geometry.prims()
    candidate_primitives = candidate_geometry.prims()
    if len(source_primitives) != len(candidate_primitives):
        raise RuntimeError(
            f"Primitive topology count mismatch: {len(source_primitives)} != "
            f"{len(candidate_primitives)}"
        )

    vertex_count = 0
    for primitive_index, (source_primitive, candidate_primitive) in enumerate(
        zip(source_primitives, candidate_primitives)
    ):
        source_type = source_primitive.type().name()
        candidate_type = candidate_primitive.type().name()
        if source_type != candidate_type:
            raise RuntimeError(
                f"Primitive {primitive_index} type mismatch: {source_type} != {candidate_type}"
            )
        if source_primitive.isClosed() != candidate_primitive.isClosed():
            raise RuntimeError(
                f"Primitive {primitive_index} closed-state mismatch: "
                f"{source_primitive.isClosed()} != {candidate_primitive.isClosed()}"
            )

        source_points = [vertex.point().number() for vertex in source_primitive.vertices()]
        candidate_points = [vertex.point().number() for vertex in candidate_primitive.vertices()]
        if source_points != candidate_points:
            raise RuntimeError(
                f"Primitive {primitive_index} point topology mismatch: "
                f"{source_points!r} != {candidate_points!r}"
            )
        vertex_count += len(source_points)

    return {
        "domain": "topology",
        "primitive_count": len(source_primitives),
        "vertex_count": vertex_count,
        "exact_match": True,
    }


def float_attribute_values(geometry: hou.Geometry, domain: str,
                           attribute_name: str) -> Sequence[float]:
    """Read flat floating-point values from a Houdini attribute domain.

    Args:
        geometry: Houdini geometry.
        domain: Supported domain, either point or vertex.
        attribute_name: Attribute name.

    Returns:
        Flat attribute values.

    Raises:
        RuntimeError: If the attribute is missing or not floating point.
        ValueError: If the domain is unsupported.
    """
    if domain == "point":
        attribute = geometry.findPointAttrib(attribute_name)
        value_reader = geometry.pointFloatAttribValues
    elif domain == "vertex":
        attribute = geometry.findVertexAttrib(attribute_name)
        value_reader = geometry.vertexFloatAttribValues
    else:
        raise ValueError(f"Unsupported floating-point attribute domain: {domain}")

    if attribute is None:
        raise RuntimeError(f"Missing {domain} attribute: {attribute_name}")
    if attribute.dataType() != hou.attribData.Float:
        raise RuntimeError(f"{domain} attribute {attribute_name!r} is not floating point")
    return value_reader(attribute_name)


def primitive_attribute_values(geometry: hou.Geometry, attribute_name: str,
                               data_type: hou.attribData) -> Sequence[object]:
    """Read flat primitive attribute values of an exact Houdini data type.

    Args:
        geometry: Houdini geometry.
        attribute_name: Primitive attribute name.
        data_type: Expected Houdini attribute data type.

    Returns:
        Flat primitive attribute values.

    Raises:
        RuntimeError: If the attribute is missing or has the wrong type.
        ValueError: If the requested type is unsupported.
    """
    attribute = geometry.findPrimAttrib(attribute_name)
    if attribute is None:
        raise RuntimeError(f"Missing primitive attribute: {attribute_name}")
    if attribute.dataType() != data_type:
        raise RuntimeError(
            f"Primitive attribute {attribute_name!r} has type {attribute.dataType()}, "
            f"expected {data_type}"
        )

    if data_type == hou.attribData.String:
        return geometry.primStringAttribValues(attribute_name)
    if data_type == hou.attribData.Int:
        return geometry.primIntAttribValues(attribute_name)
    raise ValueError(f"Unsupported primitive attribute type: {data_type}")


def main() -> int:
    """Compare selected geometry attributes.

    Returns:
        Process exit code.
    """
    args = parse_args()
    if args.tolerance < 0.0:
        raise ValueError("Tolerance cannot be negative.")

    hou.hipFile.clear(suppress_save_prompt=True)
    hou.setFrame(1)
    source_geometry = load_geometry(args.source, "source_geometry")
    candidate_geometry = load_geometry(args.candidate, "candidate_geometry")

    source_counts = (
        len(source_geometry.points()),
        sum(len(primitive.vertices()) for primitive in source_geometry.prims()),
        len(source_geometry.prims()),
    )
    candidate_counts = (
        len(candidate_geometry.points()),
        sum(len(primitive.vertices()) for primitive in candidate_geometry.prims()),
        len(candidate_geometry.prims()),
    )
    if source_counts != candidate_counts:
        raise RuntimeError(f"Geometry count mismatch: {source_counts} != {candidate_counts}")

    comparisons = []
    for domain, attribute_names in (
        ("point", args.point_attribute),
        ("vertex", args.vertex_attribute),
    ):
        for attribute_name in attribute_names:
            comparisons.append(
                compare_float_values(
                    domain,
                    attribute_name,
                    float_attribute_values(source_geometry, domain, attribute_name),
                    float_attribute_values(candidate_geometry, domain, attribute_name),
                    args.tolerance,
                )
            )

    for attribute_name in args.primitive_string_attribute:
        comparisons.append(
            compare_exact_values(
                "primitive",
                attribute_name,
                primitive_attribute_values(
                    source_geometry, attribute_name, hou.attribData.String
                ),
                primitive_attribute_values(
                    candidate_geometry, attribute_name, hou.attribData.String
                ),
            )
        )

    for attribute_name in args.primitive_int_attribute:
        comparisons.append(
            compare_exact_values(
                "primitive",
                attribute_name,
                primitive_attribute_values(
                    source_geometry, attribute_name, hou.attribData.Int
                ),
                primitive_attribute_values(
                    candidate_geometry, attribute_name, hou.attribData.Int
                ),
            )
        )

    if args.topology:
        comparisons.append(compare_topology(source_geometry, candidate_geometry))

    summary = {
        "houdini_version": hou.applicationVersionString(),
        "source": str(args.source.resolve()),
        "candidate": str(args.candidate.resolve()),
        "counts": {
            "points": source_counts[0],
            "vertices": source_counts[1],
            "primitives": source_counts[2],
        },
        "comparisons": comparisons,
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
