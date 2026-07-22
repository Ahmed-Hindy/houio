# HouIO for Houdini

HouIO supports Houdini 20.0 or newer.

For transient evaluation, extract the archive and run:

```powershell
.\bootstrap_houdini_package.ps1 -HoudiniVersion 20.0
```

This launches Houdini with isolated package and preference directories. It does not install package files or modify Houdini user folders.

Open a SOP network and use **Tab > HouIO**:

- **HouIO Round Trip** creates a configured Python SOP after the selected node.
- **Convert Geometry File** runs the bundled converter for `.geo`, `.bgeo`, and `.bgeo.sc` files.
- **Package Diagnostics** reports the active package, converter, and C-Blosc paths.

The converter runs as a separate process. Houdini loads only the Python bridge, avoiding Python ABI coupling between Houdini versions.

The bridge preserves Houdini Volume Visualization detail metadata across the Houdini 20.x scalar layout and the Houdini 21.x+ dictionary layout. It also supports bounded 32-bit Float SDF and Fog VDB grids through explicit dense-volume conversion. Sparse VDB topology is not preserved.
