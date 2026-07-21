"""Validate HouIO's HOM bridge in an installed Houdini version."""

from __future__ import annotations

import argparse
from pathlib import Path

import hou

from houio_hom import (
    convert_vdb_to_volume,
    convert_volume_to_vdb,
    convert_with_houio,
    geometry_from_bgeo_bytes,
    geometry_to_bgeo_bytes,
    load_for_houio,
    roundtrip_node_geometry,
    save_from_houio,
)


def build_dense_volume(visualization_mode: str = "smoke") -> hou.Geometry:
    """Create a deterministic scalar volume for bridge validation.

    Args:
        visualization_mode: ``smoke`` for fog semantics or ``iso`` for SDF
            semantics.
    """
    geometry = hou.Geometry()
    bounding_box = hou.BoundingBox(-2.0, -1.0, 3.0, 6.0, 5.0, 7.0)
    volume = geometry.createVolume(17, 2, 1, bounding_box)
    volume.setAllVoxels(tuple(float(x + y * 100) for y in range(2) for x in range(17)))
    volume.setIntrinsicValue("volumevisualmode", visualization_mode)
    return geometry


def volume_summary(
    geometry: hou.Geometry,
) -> tuple[tuple[int, int, int], tuple[float, ...], tuple[float, ...], str]:
    """Return comparable resolution, voxel, transform, and semantic data."""
    volumes = [
        primitive for primitive in geometry.prims() if isinstance(primitive, hou.Volume)
    ]
    if len(volumes) != 1:
        raise AssertionError("Expected exactly one dense scalar volume")
    volume = volumes[0]
    resolution = tuple(int(value) for value in volume.resolution())
    values = tuple(float(value) for value in volume.allVoxels())
    transform = tuple(
        float(value) for row in volume.transform().asTupleOfTuples() for value in row
    )
    visualization_mode = str(volume.intrinsicValue("volumevisualmode"))
    return resolution, values, transform, visualization_mode


def vdb_grid_class(geometry: hou.Geometry) -> str:
    """Return the class of a geometry containing exactly one VDB grid."""
    grids = [
        primitive for primitive in geometry.prims() if isinstance(primitive, hou.VDB)
    ]
    if len(grids) != 1:
        raise AssertionError("Expected exactly one VDB grid")
    return str(grids[0].intrinsicValue("vdb_class"))


def validate_volume_semantics(output_directory: Path) -> None:
    """Validate both fog and SDF classes through HOM and the C++ converter."""
    for label, visualization_mode, expected_class in (
        ("fog", "smoke", "fog volume"),
        ("sdf", "iso", "level set"),
    ):
        source = build_dense_volume(visualization_mode)
        source_summary = volume_summary(source)

        vdb_geometry = convert_volume_to_vdb(source)
        if vdb_grid_class(vdb_geometry) != expected_class:
            raise AssertionError(f"{label} dense volume produced the wrong VDB class")

        dense_geometry = convert_vdb_to_volume(vdb_geometry)
        if volume_summary(dense_geometry) != source_summary:
            raise AssertionError(f"{label} VDB densification changed its semantics")

        rebuilt_vdb = convert_volume_to_vdb(dense_geometry)
        if vdb_grid_class(rebuilt_vdb) != expected_class:
            raise AssertionError(
                f"{label} VDB class was not restored after densification"
            )

        source_path = output_directory / f"{label}_source.bgeo"
        result_path = output_directory / f"{label}_converter.bgeo.sc"
        rebuilt_path = output_directory / f"{label}_converter.vdb"
        source.saveToFile(str(source_path))
        convert_with_houio(source_path, result_path)
        converter_result = load_for_houio(result_path, convert_vdb=False)
        if volume_summary(converter_result) != source_summary:
            raise AssertionError(
                f"{label} C++ round-trip changed dense-volume visualization semantics"
            )
        convert_with_houio(result_path, rebuilt_path)
        rebuilt_from_converter = load_for_houio(rebuilt_path, convert_vdb=False)
        if vdb_grid_class(rebuilt_from_converter) != expected_class:
            raise AssertionError(f"{label} C++ round-trip produced the wrong VDB class")

    combined_vdb = hou.Geometry()
    combined_vdb.merge(convert_volume_to_vdb(build_dense_volume("iso")))
    combined_vdb.merge(convert_volume_to_vdb(build_dense_volume("smoke")))
    combined_dense = convert_vdb_to_volume(combined_vdb)
    combined_modes = sorted(
        str(primitive.intrinsicValue("volumevisualmode"))
        for primitive in combined_dense.prims()
        if isinstance(primitive, hou.Volume) and not isinstance(primitive, hou.VDB)
    )
    combined_classes = sorted(
        str(primitive.attribValue("houio_vdb_class"))
        for primitive in combined_dense.prims()
        if isinstance(primitive, hou.Volume) and not isinstance(primitive, hou.VDB)
    )
    if combined_modes != ["iso", "smoke"]:
        raise AssertionError("Mixed SDF/Fog VDB densification lost visualization modes")
    if combined_classes != ["fog volume", "level set"]:
        raise AssertionError("Mixed SDF/Fog VDB densification lost class metadata")
    combined_rebuilt = convert_volume_to_vdb(combined_dense)
    rebuilt_classes = sorted(
        str(primitive.intrinsicValue("vdb_class"))
        for primitive in combined_rebuilt.prims()
        if isinstance(primitive, hou.VDB)
    )
    if rebuilt_classes != ["fog volume", "level set"]:
        raise AssertionError("Mixed SDF/Fog dense volumes rebuilt with wrong classes")


def validate(output_directory: Path) -> None:
    """Run all HOM bridge round-trips."""
    output_directory.mkdir(parents=True, exist_ok=True)
    validate_volume_semantics(output_directory)
    source = build_dense_volume()
    source_summary = volume_summary(source)

    bgeo_bytes = geometry_to_bgeo_bytes(source)
    loaded_bytes = geometry_from_bgeo_bytes(bgeo_bytes)
    if volume_summary(loaded_bytes) != source_summary:
        raise AssertionError("Bgeo byte bridge changed the source volume")

    vdb_geometry = convert_volume_to_vdb(source)
    if len(vdb_geometry.prims()) != 1 or not isinstance(
        vdb_geometry.prims()[0], hou.VDB
    ):
        raise AssertionError("Dense volume conversion did not produce a VDB")
    if vdb_geometry.prims()[0].vdbType() != hou.vdbType.Float:
        raise AssertionError("Dense volume conversion did not produce a Float VDB")

    mixed_geometry = hou.Geometry()
    polygon_points = mixed_geometry.createPoints(
        ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
    )
    polygon = mixed_geometry.createPolygon()
    for point in polygon_points:
        polygon.addVertex(point)
    mixed_geometry.merge(vdb_geometry)
    mixed_dense = convert_vdb_to_volume(mixed_geometry)
    if (
        sum(isinstance(primitive, hou.Polygon) for primitive in mixed_dense.prims())
        != 1
    ):
        raise AssertionError("Mixed VDB conversion did not preserve polygon geometry")
    if volume_summary(mixed_dense) != source_summary:
        raise AssertionError("Mixed VDB conversion changed the dense volume")

    try:
        convert_volume_to_vdb(mixed_dense)
    except ValueError:
        pass
    else:
        raise AssertionError("Mixed .vdb output was not rejected before mesh data loss")

    integer_verb = hou.sopNodeTypeCategory().nodeVerb("convertvdb")
    if integer_verb is None:
        raise AssertionError("The Convert VDB SOP verb is unavailable")
    integer_verb.setParms({"conversion": 1, "vdbtype": "int"})
    integer_vdb = hou.Geometry()
    integer_verb.execute(integer_vdb, [source])
    try:
        convert_vdb_to_volume(integer_vdb)
    except ValueError as error:
        if "32-bit float" not in str(error):
            raise
    else:
        raise AssertionError("Non-Float VDB densification was not rejected")

    vdb_path = output_directory / "density.vdb"
    vdb_geometry.saveToFile(str(vdb_path))
    loaded_vdb = load_for_houio(vdb_path)
    if volume_summary(loaded_vdb) != source_summary:
        raise AssertionError(
            "VDB to dense-volume conversion changed values or transform"
        )

    scf_path = output_directory / "density.bgeo.sc"
    save_from_houio(bgeo_bytes, scf_path)
    loaded_scf = load_for_houio(scf_path, convert_vdb=False)
    if volume_summary(loaded_scf) != source_summary:
        raise AssertionError("HOM bgeo.sc bridge changed values or transform")

    vdb_from_bytes_path = output_directory / "density_from_houio.vdb"
    save_from_houio(bgeo_bytes, vdb_from_bytes_path)
    converted_back = convert_vdb_to_volume(
        load_for_houio(vdb_from_bytes_path, convert_vdb=False)
    )
    if volume_summary(converted_back) != source_summary:
        raise AssertionError(
            "HouIO bytes to VDB round-trip changed values or transform"
        )

    converter_scf_path = output_directory / "density_converter.bgeo.sc"
    convert_with_houio(vdb_path, converter_scf_path)
    converter_result = load_for_houio(converter_scf_path, convert_vdb=False)
    if volume_summary(converter_result) != source_summary:
        raise AssertionError("HOM subprocess bridge changed values or transform")

    converter_vdb_path = output_directory / "density_converter.vdb"
    convert_with_houio(scf_path, converter_vdb_path)
    converter_vdb_result = load_for_houio(converter_vdb_path)
    if volume_summary(converter_vdb_result) != source_summary:
        raise AssertionError("HouIO subprocess VDB output changed values or transform")

    object_context = hou.node("/obj")
    if object_context is None:
        raise AssertionError("Houdini object context is unavailable")
    geometry_container = object_context.createNode("geo", node_name="houio_hom_test")
    try:
        box_node = geometry_container.createNode("box")
        source_box = box_node.geometry()
        processed_box = roundtrip_node_geometry(box_node, timeout_seconds=60.0)
        if len(processed_box.points()) != len(source_box.points()) or len(
            processed_box.prims()
        ) != len(source_box.prims()):
            raise AssertionError("Cooked SOP round-trip changed box counts")

        file_node = geometry_container.createNode("file")
        file_node.parm("file").set(str(vdb_path))
        processed_vdb_node = roundtrip_node_geometry(file_node, timeout_seconds=60.0)
        if volume_summary(processed_vdb_node) != source_summary:
            raise AssertionError(
                "Cooked VDB SOP round-trip changed values or transform"
            )

        dense_file_node = geometry_container.createNode("file")
        dense_file_node.parm("file").set(str(converter_scf_path))
        dense_file_node.cook(force=True)
        if dense_file_node.errors():
            raise AssertionError(
                "HouIO dense-volume SCF produced File SOP errors: "
                + "; ".join(dense_file_node.errors())
            )
        if dense_file_node.warnings():
            raise AssertionError(
                "HouIO dense-volume SCF produced File SOP warnings: "
                + "; ".join(dense_file_node.warnings())
            )
    finally:
        geometry_container.destroy()


def main() -> int:
    """Run the HOM bridge validation."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    arguments = parser.parse_args()
    validate(arguments.output_directory)
    print(f"HOM bridge passed in Houdini {hou.applicationVersionString()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
