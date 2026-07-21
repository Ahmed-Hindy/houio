"""Build a self-contained HouIO runtime and Houdini 22 manual-test bundle."""

from __future__ import annotations

import argparse
import os
import shutil
import sys
import textwrap
from pathlib import Path
from typing import Optional

import hou


POLYGON_FIXTURE_CODE = r"""
import hou

geometry = hou.pwd().geometry()
geometry.clear()

positions = (
    (-1.0, 0.0, -1.0),
    (1.0, 0.0, -1.0),
    (1.0, 0.0, 1.0),
    (-1.0, 0.0, 1.0),
    (0.0, 1.5, 0.0),
)
points = []
for position in positions:
    point = geometry.createPoint()
    point.setPosition(position)
    points.append(point)

point_id = geometry.addAttrib(hou.attribType.Point, "id", 0)
point_weight = geometry.addAttrib(hou.attribType.Point, "weight", 0.0)
point_color = geometry.addAttrib(hou.attribType.Point, "Cd", (0.0, 0.0, 0.0))
for index, point in enumerate(points):
    point.setAttribValue(point_id, index)
    point.setAttribValue(point_weight, index * 0.25)
    point.setAttribValue(
        point_color,
        (index / 4.0, 1.0 - index / 4.0, 0.25 + index * 0.1),
    )

primitive_name = geometry.addAttrib(hou.attribType.Prim, "name", "")
primitive_piece = geometry.addAttrib(hou.attribType.Prim, "piece", 0)
vertex_uv = geometry.addAttrib(hou.attribType.Vertex, "uv", (0.0, 0.0, 0.0))

polygon_specs = (
    ((0, 1, 2, 3), "base_quad", 10),
    ((1, 0, 4), "side_triangle", 20),
)
for primitive_index, (point_indices, name, piece) in enumerate(polygon_specs):
    polygon = geometry.createPolygon()
    for local_index, point_index in enumerate(point_indices):
        vertex = polygon.addVertex(points[point_index])
        vertex.setAttribValue(
            vertex_uv,
            (
                local_index / max(1, len(point_indices) - 1),
                float(primitive_index),
                0.0,
            ),
        )
    polygon.setAttribValue(primitive_name, name)
    polygon.setAttribValue(primitive_piece, piece)

geometry.addAttrib(hou.attribType.Global, "fixture", "")
geometry.addAttrib(hou.attribType.Global, "fixture_version", 0)
geometry.setGlobalAttribValue("fixture", "houio_manual_test")
geometry.setGlobalAttribValue("fixture_version", 1)

primitive_group = geometry.createPrimGroup("main_surfaces")
primitive_group.add(geometry.prims())
point_group = geometry.createPointGroup("tip")
point_group.add(points[-1])
""".strip()


BOOTSTRAP_CODE = r"""
from pathlib import Path
import os
import sys
import hou

bundle_root = Path(hou.text.expandString("$HIP")).parent
python_root = bundle_root / "share" / "houio" / "python"
converter = bundle_root / "bin" / "houio_convert.exe"
blosc = Path(hou.text.expandString("$HFS")) / "bin" / "blosc.dll"

if str(python_root) not in sys.path:
    sys.path.insert(0, str(python_root))
os.environ["HOUIO_ROOT"] = str(bundle_root)
os.environ["HOUIO_PYTHON_ROOT"] = str(python_root)
os.environ["HOUIO_CONVERT_EXECUTABLE"] = str(converter)
os.environ["HOUIO_BLOSC_LIBRARY"] = str(blosc)
""".strip()


ROUNDTRIP_CODE = (
    BOOTSTRAP_CODE
    + "\n\nfrom houio_hom import roundtrip_current_sop\n\nroundtrip_current_sop()"
)


VALIDATE_POLYGON_CODE = r"""
import hou

node = hou.pwd()
source = node.inputs()[0].geometry()
result = node.inputs()[1].geometry()
issues = []


def normalize(value):
    if isinstance(value, float):
        return round(value, 7)
    if isinstance(value, (tuple, list)):
        return tuple(normalize(item) for item in value)
    return value


def point_values(geometry, name):
    return tuple(normalize(point.attribValue(name)) for point in geometry.points())


def primitive_values(geometry, name):
    return tuple(normalize(primitive.attribValue(name)) for primitive in geometry.prims())


def vertex_values(geometry, name):
    return tuple(
        tuple(normalize(vertex.attribValue(name)) for vertex in primitive.vertices())
        for primitive in geometry.prims()
    )


def topology(geometry):
    return tuple(
        tuple(vertex.point().number() for vertex in primitive.vertices())
        for primitive in geometry.prims()
    )


def point_groups(geometry):
    return {
        group.name(): tuple(point.number() for point in group.points())
        for group in geometry.pointGroups()
    }


def primitive_groups(geometry):
    return {
        group.name(): tuple(primitive.number() for primitive in group.prims())
        for group in geometry.primGroups()
    }


if len(source.points()) != len(result.points()):
    issues.append("point count changed")
if len(source.prims()) != len(result.prims()):
    issues.append("primitive count changed")
if topology(source) != topology(result):
    issues.append("polygon topology changed")

for attribute_name in ("P", "id", "weight", "Cd"):
    if source.findPointAttrib(attribute_name) is None or result.findPointAttrib(attribute_name) is None:
        issues.append(f"missing point attribute {attribute_name}")
    elif point_values(source, attribute_name) != point_values(result, attribute_name):
        issues.append(f"point attribute {attribute_name} changed")

if source.findVertexAttrib("uv") is None or result.findVertexAttrib("uv") is None:
    issues.append("missing vertex attribute uv")
elif vertex_values(source, "uv") != vertex_values(result, "uv"):
    issues.append("vertex attribute uv changed")

for attribute_name in ("name", "piece"):
    if source.findPrimAttrib(attribute_name) is None or result.findPrimAttrib(attribute_name) is None:
        issues.append(f"missing primitive attribute {attribute_name}")
    elif primitive_values(source, attribute_name) != primitive_values(result, attribute_name):
        issues.append(f"primitive attribute {attribute_name} changed")

for attribute_name in ("fixture", "fixture_version"):
    if source.findGlobalAttrib(attribute_name) is None or result.findGlobalAttrib(attribute_name) is None:
        issues.append(f"missing detail attribute {attribute_name}")
    elif normalize(source.attribValue(attribute_name)) != normalize(result.attribValue(attribute_name)):
        issues.append(f"detail attribute {attribute_name} changed")

if point_groups(source) != point_groups(result):
    issues.append("point groups changed")
if primitive_groups(source) != primitive_groups(result):
    issues.append("primitive groups changed")

output = node.geometry()
output.clear()
output.merge(result)
output.addAttrib(hou.attribType.Global, "houio_test_status", "")
output.addAttrib(hou.attribType.Global, "houio_test_message", "")
output.addAttrib(hou.attribType.Global, "houio_source_point_count", 0)
output.addAttrib(hou.attribType.Global, "houio_result_point_count", 0)
output.addAttrib(hou.attribType.Global, "houio_source_primitive_count", 0)
output.addAttrib(hou.attribType.Global, "houio_result_primitive_count", 0)
output.setGlobalAttribValue("houio_source_point_count", len(source.points()))
output.setGlobalAttribValue("houio_result_point_count", len(result.points()))
output.setGlobalAttribValue("houio_source_primitive_count", len(source.prims()))
output.setGlobalAttribValue("houio_result_primitive_count", len(result.prims()))

if issues:
    message = "; ".join(issues)
    output.setGlobalAttribValue("houio_test_status", "FAIL")
    output.setGlobalAttribValue("houio_test_message", message)
    raise hou.NodeError("HouIO polygon validation failed: " + message)

output.setGlobalAttribValue("houio_test_status", "PASS")
output.setGlobalAttribValue(
    "houio_test_message",
    "Topology, point/vertex/primitive/detail attributes, and groups match.",
)
""".strip()


README_TEXT = r"""# HouIO Houdini 22 Windows runtime

This bundle was built from HouIO commit `{commit}` for Houdini 22 on Windows.

## Fastest start

1. Extract the bundle to a writable folder.
2. Open `manual-tests/houio_manual_tests.hip` in Houdini 22.
3. Inspect and cook the nodes in:
   - `/obj/HOUiO_POLYGON_ROUNDTRIP`
   - `/obj/HOUiO_VDB_ROUNDTRIP`
   - `/obj/HOUiO_DISK_FIXTURES`

In the polygon network, `RESULT_POLYGON` is intentionally identical to the
source when the round-trip succeeds. `VALIDATE_SOURCE_RESULT` checks topology,
point/vertex/primitive/detail attributes, and groups. Inspect its detail
attributes for `houio_test_status=PASS`. `VIEW_SOURCE_AND_RESULT` displays the
source and a translated result side by side so both are visible at once.

The HIP file bootstraps the bundled Python package and converter relative to its
own location. Installing the Houdini package is optional for this HIP file.

## Optional package installation

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\INSTALL_HOUDINI_22.ps1
```

Restart Houdini after installation. The installer writes `houio.json` under the
Windows Documents known folder in `houdini22.0/packages`.

## Automated smoke test

```powershell
& "C:\Program Files\Side Effects Software\Houdini 22.0.368\bin\hython.exe" `
  .\manual-tests\run_smoke_tests.py
```

## Included runtime

- `bin/houio_convert.exe`
- `share/houio/python/houio_hom`
- `share/houio/houdini/install_hom_bridge.ps1`
- ready Houdini HIP and source/result cache files

## Important limitations

- Float VDB grids are densified before HouIO processes them.
- Sparse OpenVDB topology is not preserved.
- VDB output must contain volumes only; mixed polygon/VDB output is rejected.
- Best-supported standalone geometry is polygons and dense scalar volumes.
"""


INSTALL_SCRIPT = r"""[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$BundleRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Installer = Join-Path $BundleRoot "share\houio\houdini\install_hom_bridge.ps1"
$Converter = Join-Path $BundleRoot "bin\houio_convert.exe"

powershell -NoProfile -ExecutionPolicy Bypass -File $Installer `
    -HoudiniVersions 22.0 `
    -RepositoryRoot $BundleRoot `
    -ConverterExecutable $Converter
"""


OPEN_SCRIPT = r"""[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$BundleRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$HipFile = Join-Path $BundleRoot "manual-tests\houio_manual_tests.hip"
$HoudiniRoot = Get-ChildItem "C:\Program Files\Side Effects Software" -Directory |
    Where-Object { $_.Name -like "Houdini 22.0.*" } |
    Sort-Object Name -Descending |
    Select-Object -First 1

if (-not $HoudiniRoot) {
    throw "Could not find a Houdini 22.0 installation."
}

$Houdini = Join-Path $HoudiniRoot.FullName "bin\houdini.exe"
Start-Process -FilePath $Houdini -ArgumentList @($HipFile)
"""


SMOKE_TEST_SCRIPT = r'''"""Run the bundled HouIO Houdini 22 smoke tests."""

from pathlib import Path
import os
import sys

import hou

bundle_root = Path(__file__).resolve().parents[1]
python_root = bundle_root / "share" / "houio" / "python"
os.environ["HOUIO_ROOT"] = str(bundle_root)
os.environ["HOUIO_PYTHON_ROOT"] = str(python_root)
os.environ["HOUIO_CONVERT_EXECUTABLE"] = str(bundle_root / "bin" / "houio_convert.exe")
os.environ["HOUIO_BLOSC_LIBRARY"] = str(Path(hou.text.expandString("$HFS")) / "bin" / "blosc.dll")
sys.path.insert(0, str(python_root))

hip_path = bundle_root / "manual-tests" / "houio_manual_tests.hip"
hou.hipFile.load(str(hip_path), suppress_save_prompt=True)

polygon_source = hou.node("/obj/HOUiO_POLYGON_ROUNDTRIP/SOURCE_POLYGON").geometry()
polygon_result = hou.node("/obj/HOUiO_POLYGON_ROUNDTRIP/RESULT_POLYGON").geometry()
polygon_validation = hou.node(
    "/obj/HOUiO_POLYGON_ROUNDTRIP/VALIDATE_SOURCE_RESULT"
).geometry()
polygon_offset = hou.node(
    "/obj/HOUiO_POLYGON_ROUNDTRIP/RESULT_POLYGON_OFFSET"
).geometry()
polygon_view = hou.node(
    "/obj/HOUiO_POLYGON_ROUNDTRIP/VIEW_SOURCE_AND_RESULT"
).geometry()
assert len(polygon_source.points()) == len(polygon_result.points()) == 5
assert len(polygon_source.prims()) == len(polygon_result.prims()) == 2
assert polygon_validation.attribValue("houio_test_status") == "PASS"
assert len(polygon_view.points()) == 10
assert len(polygon_view.prims()) == 4
assert polygon_offset.boundingBox().minvec()[0] > polygon_source.boundingBox().maxvec()[0]
for attribute_name in ("id", "weight", "Cd"):
    assert polygon_result.findPointAttrib(attribute_name) is not None
for attribute_name in ("name", "piece"):
    assert polygon_result.findPrimAttrib(attribute_name) is not None
assert polygon_result.findVertexAttrib("uv") is not None

vdb_source = hou.node("/obj/HOUiO_VDB_ROUNDTRIP/SOURCE_VDB").geometry()
dense_result = hou.node("/obj/HOUiO_VDB_ROUNDTRIP/RESULT_DENSE_VOLUME").geometry()
rebuilt_vdb = hou.node("/obj/HOUiO_VDB_ROUNDTRIP/RESULT_REBUILT_VDB").geometry()
assert any(isinstance(primitive, hou.VDB) for primitive in vdb_source.prims())
assert any(isinstance(primitive, hou.Volume) for primitive in dense_result.prims())
assert any(isinstance(primitive, hou.VDB) for primitive in rebuilt_vdb.prims())

cache_root = bundle_root / "manual-tests" / "cache"
required_files = (
    "polygon_source.bgeo.sc",
    "polygon_houio.bgeo.sc",
    "density_source.vdb",
    "density_houio.bgeo.sc",
    "density_rebuilt.vdb",
)
for file_name in required_files:
    assert (cache_root / file_name).is_file(), file_name

for node_name in (
    "POLYGON_SOURCE_FILE",
    "POLYGON_HOUiO_FILE",
    "VDB_SOURCE_FILE",
    "DENSE_HOUiO_FILE",
    "VDB_REBUILT_FILE",
):
    file_node = hou.node(f"/obj/HOUiO_DISK_FIXTURES/{node_name}")
    file_node.cook(force=True)
    assert not file_node.errors(), (node_name, file_node.errors())
    assert not file_node.warnings(), (node_name, file_node.warnings())

print("HouIO Houdini 22 smoke tests: PASS")
print(f"HIP: {hip_path}")
print(f"Polygon points/primitives: {len(polygon_result.points())}/{len(polygon_result.prims())}")
print(f"Dense volume primitives: {len(dense_result.prims())}")
'''


def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--install-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    return parser.parse_args()


def write_text(path: Path, text: str) -> None:
    """Write UTF-8 text after creating the parent directory."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(text).lstrip(), encoding="utf-8")


def add_note(
    parent: hou.Node, text: str, bounds: tuple[float, float, float, float]
) -> None:
    """Add a sticky note when supported by the current network."""
    try:
        note = parent.createStickyNote()
        note.setText(text)
        note.setBounds(hou.BoundingRect(*bounds))
        note.setTextSize(0.55)
    except hou.Error:
        pass


def create_python_node(
    parent: hou.Node, name: str, code: str, input_node: Optional[hou.Node] = None
) -> hou.SopNode:
    """Create and configure a Python SOP."""
    node = parent.createNode("python", name)
    node.parm("python").set(code)
    if input_node is not None:
        node.setInput(0, input_node)
    return node


def create_polygon_network(obj: hou.Node) -> hou.Node:
    """Create the polygon round-trip test network."""
    network = obj.createNode("geo", "HOUiO_POLYGON_ROUNDTRIP", run_init_scripts=False)
    source_builder = create_python_node(
        network, "BUILD_POLYGON_FIXTURE", POLYGON_FIXTURE_CODE
    )
    source = network.createNode("null", "SOURCE_POLYGON")
    source.setInput(0, source_builder)
    roundtrip = create_python_node(network, "HOUiO_ROUNDTRIP", ROUNDTRIP_CODE, source)
    result = network.createNode("null", "RESULT_POLYGON")
    result.setInput(0, roundtrip)

    validator = create_python_node(
        network, "VALIDATE_SOURCE_RESULT", VALIDATE_POLYGON_CODE
    )
    validator.setInput(0, source)
    validator.setInput(1, result)
    validator.setColor(hou.Color((0.25, 0.65, 0.25)))

    offset = network.createNode("xform", "RESULT_POLYGON_OFFSET")
    offset.setInput(0, validator)
    offset.parm("tx").set(3.5)

    view = network.createNode("merge", "VIEW_SOURCE_AND_RESULT")
    view.setInput(0, source)
    view.setInput(1, offset)
    view.setDisplayFlag(True)
    view.setRenderFlag(True)

    source_builder.setPosition(hou.Vector2(-6.0, 0.0))
    source.setPosition(hou.Vector2(-4.0, 0.0))
    roundtrip.setPosition(hou.Vector2(-2.0, 0.0))
    result.setPosition(hou.Vector2(0.0, 0.0))
    validator.setPosition(hou.Vector2(2.0, 0.0))
    offset.setPosition(hou.Vector2(4.0, 0.0))
    view.setPosition(hou.Vector2(6.0, 0.0))
    add_note(
        network,
        "POLYGON TEST\n\nRESULT_POLYGON should be identical to SOURCE_POLYGON. Equal counts are expected.\nVALIDATE_SOURCE_RESULT performs a semantic comparison and writes houio_test_status=PASS.\nRESULT_POLYGON_OFFSET moves the validated result right for visual comparison.\nVIEW_SOURCE_AND_RESULT displays source and result together.\nRecook HOUiO_ROUNDTRIP to launch the bundled converter.",
        (-6.5, 1.0, 13.5, 5.2),
    )
    return network


def create_vdb_network(obj: hou.Node) -> hou.Node:
    """Create the Float VDB to dense-volume round-trip test network."""
    network = obj.createNode("geo", "HOUiO_VDB_ROUNDTRIP", run_init_scripts=False)
    sphere = network.createNode("sphere", "POLYGON_SPHERE")
    sphere.parm("type").set(1)
    sphere.parm("rows").set(24)
    sphere.parm("cols").set(48)
    vdb = network.createNode("vdbfrompolygons", "CREATE_FLOAT_VDB")
    vdb.setInput(0, sphere)
    vdb.parm("voxelsize").set(0.1)
    source = network.createNode("null", "SOURCE_VDB")
    source.setInput(0, vdb)
    roundtrip = create_python_node(
        network, "HOUiO_VDB_TO_DENSE", ROUNDTRIP_CODE, source
    )
    dense_result = network.createNode("null", "RESULT_DENSE_VOLUME")
    dense_result.setInput(0, roundtrip)
    rebuild = network.createNode("convertvdb", "DENSE_TO_VDB")
    rebuild.setInput(0, dense_result)
    rebuild.parm("conversion").set(1)
    vdb_result = network.createNode("null", "RESULT_REBUILT_VDB")
    vdb_result.setInput(0, rebuild)
    vdb_result.setDisplayFlag(True)
    vdb_result.setRenderFlag(True)
    for index, node in enumerate(
        (sphere, vdb, source, roundtrip, dense_result, rebuild, vdb_result)
    ):
        node.setPosition(hou.Vector2(-6.0 + index * 2.0, 0.0))
    add_note(
        network,
        "VDB TEST\n\nSOURCE_VDB is a sparse Float SDF.\nHOUiO_VDB_TO_DENSE explicitly densifies it and round-trips through HouIO.\nDENSE_TO_VDB converts the result back to VDB for visual comparison.\nExpect sparse topology to change.",
        (-6.5, 1.0, 8.5, 4.5),
    )
    return network


def create_disk_network(obj: hou.Node) -> hou.Node:
    """Create file nodes for pre-generated source and result fixtures."""
    network = obj.createNode("geo", "HOUiO_DISK_FIXTURES", run_init_scripts=False)
    file_specs = (
        ("POLYGON_SOURCE_FILE", "$HIP/cache/polygon_source.bgeo.sc"),
        ("POLYGON_HOUiO_FILE", "$HIP/cache/polygon_houio.bgeo.sc"),
        ("VDB_SOURCE_FILE", "$HIP/cache/density_source.vdb"),
        ("DENSE_HOUiO_FILE", "$HIP/cache/density_houio.bgeo.sc"),
        ("VDB_REBUILT_FILE", "$HIP/cache/density_rebuilt.vdb"),
    )
    for index, (name, file_path) in enumerate(file_specs):
        node = network.createNode("file", name)
        node.parm("file").set(file_path)
        node.setPosition(hou.Vector2((index % 2) * 4.0, -(index // 2) * 2.0))
    add_note(
        network,
        "PRE-GENERATED FILES\n\nEach File SOP loads a fixture generated while packaging.\nUse the Geometry Spreadsheet, middle-mouse info, and volume/VDB visualizers to compare source and HouIO outputs.",
        (-1.0, 1.0, 9.0, 4.0),
    )
    return network


def configure_bundle_environment(bundle_root: Path) -> None:
    """Configure the current hython process to use the staged runtime."""
    python_root = bundle_root / "share" / "houio" / "python"
    sys.path.insert(0, str(python_root))
    os.environ["HOUIO_ROOT"] = str(bundle_root)
    os.environ["HOUIO_PYTHON_ROOT"] = str(python_root)
    os.environ["HOUIO_CONVERT_EXECUTABLE"] = str(
        bundle_root / "bin" / "houio_convert.exe"
    )
    os.environ["HOUIO_BLOSC_LIBRARY"] = str(
        Path(hou.text.expandString("$HFS")) / "bin" / "blosc.dll"
    )


def create_bundle(install_root: Path, output_root: Path, commit: str) -> None:
    """Create the runtime, fixtures, HIP file, and ZIP archive."""
    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True)

    shutil.copytree(install_root / "bin", output_root / "bin")
    shutil.copytree(install_root / "share", output_root / "share")
    write_text(output_root / "README.md", README_TEXT.format(commit=commit))
    write_text(output_root / "INSTALL_HOUDINI_22.ps1", INSTALL_SCRIPT)
    write_text(output_root / "OPEN_IN_HOUDINI_22.ps1", OPEN_SCRIPT)
    write_text(output_root / "manual-tests" / "run_smoke_tests.py", SMOKE_TEST_SCRIPT)

    configure_bundle_environment(output_root)
    from houio_hom import convert_with_houio

    hou.hipFile.clear(suppress_save_prompt=True)
    obj = hou.node("/obj")
    polygon_network = create_polygon_network(obj)
    vdb_network = create_vdb_network(obj)
    create_disk_network(obj)
    polygon_network.setPosition(hou.Vector2(-8.0, 0.0))
    vdb_network.setPosition(hou.Vector2(0.0, 0.0))
    hou.node("/obj/HOUiO_DISK_FIXTURES").setPosition(hou.Vector2(8.0, 0.0))
    add_note(
        obj,
        "HOUiO MANUAL TEST SCENE\n\n1. Start with HOUiO_POLYGON_ROUNDTRIP.\n2. Inspect HOUiO_VDB_ROUNDTRIP and watch the sparse-to-dense boundary.\n3. Open HOUiO_DISK_FIXTURES to compare pre-generated files.\n\nThe Python SOPs bootstrap the runtime relative to this HIP file.",
        (-9.0, 3.0, 18.0, 8.0),
    )

    manual_test_root = output_root / "manual-tests"
    cache_root = manual_test_root / "cache"
    cache_root.mkdir(parents=True, exist_ok=True)
    hip_path = manual_test_root / "houio_manual_tests.hip"
    hou.hipFile.save(str(hip_path))

    polygon_source_path = cache_root / "polygon_source.bgeo.sc"
    polygon_result_path = cache_root / "polygon_houio.bgeo.sc"
    vdb_source_path = cache_root / "density_source.vdb"
    dense_result_path = cache_root / "density_houio.bgeo.sc"
    rebuilt_vdb_path = cache_root / "density_rebuilt.vdb"

    hou.node("/obj/HOUiO_POLYGON_ROUNDTRIP/SOURCE_POLYGON").geometry().saveToFile(
        str(polygon_source_path)
    )
    convert_with_houio(polygon_source_path, polygon_result_path)
    hou.node("/obj/HOUiO_VDB_ROUNDTRIP/SOURCE_VDB").geometry().saveToFile(
        str(vdb_source_path)
    )
    convert_with_houio(vdb_source_path, dense_result_path)
    convert_with_houio(dense_result_path, rebuilt_vdb_path)

    hou.hipFile.save(str(hip_path))
    archive_base = output_root.parent / output_root.name
    shutil.make_archive(
        str(archive_base), "zip", root_dir=output_root.parent, base_dir=output_root.name
    )


def main() -> int:
    """Build the requested bundle."""
    arguments = parse_arguments()
    create_bundle(
        arguments.install_root.resolve(),
        arguments.output_root.resolve(),
        arguments.commit,
    )
    print(arguments.output_root.resolve())
    print(arguments.output_root.with_suffix(".zip").resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
