# Native Houdini Geometry ROP

HouIO includes an optional HDK plugin that registers a genuine **HouIO Geometry** output driver in Houdini's `/out` context. It is a compiled `ROP_Node`, not an HDA, Python SOP, or HOM-generated operator.

The native path is:

```text
cooked SOP GU_Detail
  -> HDK polygon extraction
  -> dependency-neutral C ABI
  -> HouIO C++ geometry model
  -> HouIO serializer
  -> .bgeo or .bgeo.sc
```

It does not call `hou.Geometry.saveToFile()`, create a temporary HOM manifest, start `houio.exe`, or use Houdini's geometry file writer.

## Current scope

The first native vertical slice preserves:

- polygon and polyline primitives;
- polygon closure;
- point order and vertex-to-point topology;
- canonical point position `P` as four-component Float32 data;
- current-frame and frame-range evaluation through the standard ROP lifecycle;
- output path expansion such as `$HIP`, `$OS`, `$F`, and `$F4`;
- create-directory, overwrite, and atomic-replacement policies;
- standard pre-render, pre-frame, post-frame, and post-render scripts;
- Houdini cancellation and node error reporting.

To prevent silent data loss, this initial implementation rejects:

- non-polygon primitives;
- public point attributes other than `P`;
- public vertex, primitive, or detail attributes;
- point, vertex, or primitive groups.

This is deliberately narrower than the standalone HouIO writer and HOM package. Native attribute, group, packed-primitive, curve, quadric, volume, and VDB adapters should be added incrementally with exact HDK-backed tests.

## Build one Houdini version

The DSO must be built against the exact Houdini SDK line that will load it:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" `
  -Arch amd64 -HostArch amd64

cmake -S . -B build/native-rop/22.0.368 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DHOUIO_BUILD_TESTS=OFF `
  -DHOUIO_BUILD_EXAMPLES=OFF `
  -DHOUIO_BUILD_TOOLS=OFF `
  -DHOUIO_BUILD_HOUDINI_PLUGIN=ON `
  "-DHOUIO_HOUDINI_ROOT=C:/Program Files/Side Effects Software/Houdini 22.0.368"

cmake --build build/native-rop/22.0.368 `
  --target houio_houdini_rop --parallel
```

Generated plugin:

```text
build/native-rop/22.0.368/houdini/hdk/dso/ROP_HouIO.dll
```

The plugin target follows the C++ language level selected by each SideFX SDK. HouIO's C++20 core is linked behind `NativePolygonWriter`'s C ABI so standard-library objects never cross the HDK boundary.

## Load for development

Set a process-local DSO path before launching the matching Houdini build:

```powershell
$Version = "22.0.368"
$env:HOUDINI_DSO_PATH = "$PWD/build/native-rop/$Version/houdini/hdk/dso;&"
$env:HOUIO_BLOSC_LIBRARY = `
  "C:/Program Files/Side Effects Software/Houdini $Version/bin/blosc.dll"

& "C:/Program Files/Side Effects Software/Houdini $Version/bin/houdini.exe"
```

Inside Houdini:

1. Create the SOP node that produces the geometry.
2. Enter `/out`.
3. Press **Tab** and create **HouIO Geometry**.
4. Use the **Export** tab to set frame range, take, SOP path, output path, and file policies.
5. Use the **Scripts** tab only when render lifecycle hooks are required.
6. Render with the single standard **Render to Disk** button.

The operator owns the standard ROP templates directly instead of inheriting a second copy from `ROP_Node::getROPbaseTemplate()`. This avoids duplicate `execute`, `renderdialog`, `trange`, `f`, and `take` symbols and keeps the dialog arranged as one render-control row followed by **Export** and **Scripts** tabs.

## Explorable demo HIP

Generate a Houdini 20.0-compatible demo scene after building the version-matched DSO:

```powershell
$Version = "20.0.653"
$env:HOUDINI_DSO_PATH = "$PWD/build/native-rop/$Version/houdini/hdk/dso;&"
$env:HOUIO_BLOSC_LIBRARY = `
  "C:/Program Files/Side Effects Software/Houdini $Version/bin/blosc.dll"

& "C:/Program Files/Side Effects Software/Houdini $Version/bin/hython.exe" `
  .\tools\houdini\generate_native_rop_demo.py `
  .\build\native-rop-demo\houio_native_rop_demo.hip
```

The scene contains an animated supported polygon/polyline source, a cache read-back network, configured successful and failing ROPs, and comments describing the expected behavior.

## Maintained validation matrix

Build and execute the native ROP in every maintained Houdini version:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_native_rop_matrix.ps1
```

The matrix currently covers Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368. For each version it:

- compiles a version-specific DSO;
- verifies registration of `houio::geometry` in the ROP category;
- validates one render-control row and the **Export**/**Scripts** folder layout;
- rejects `OPUI_DialogPRM2` and duplicate-symbol warnings;
- renders an animated box for two frames;
- loads the generated files back through Houdini;
- verifies geometry counts and per-frame SOP evaluation;
- confirms that an unsupported native sphere fails with an actionable node error.

## Non-commercial HIP validation

Use a genuine existing `.hipnc` as the source fixture. The test never modifies that file; it creates its own geometry and ROP nodes, renders two compressed frames, and saves an isolated non-commercial copy under the requested output directory.

```powershell
$Version = "22.0.368"
$env:HOUDINI_DSO_PATH = "$PWD/build/native-rop/$Version/houdini/hdk/dso;&"
$env:HOUIO_BLOSC_LIBRARY = `
  "C:/Program Files/Side Effects Software/Houdini $Version/bin/blosc.dll"

& "C:/Program Files/Side Effects Software/Houdini $Version/bin/hython.exe" `
  .\tools\houdini\test_native_rop_noncommercial.py `
  "G:/path/to/a/genuine_scene.hipnc" `
  .\build\native-rop-test\noncommercial
```

The test requires loading the source file to switch the process to `licenseCategoryType.Apprentice`, verifies `hou.isApprentice()`, renders through `houio::geometry`, and saves `houio_native_rop_noncommercial_test.hipnc`.

## Distribution status

The existing HOM package archive does not yet bundle the native DSO. HDK binaries are ABI-specific and need a version-aware package layout and installer. Until that packaging phase is complete, the native ROP is a source-built development feature.
