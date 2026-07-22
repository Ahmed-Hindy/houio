# HouIO for Houdini

After installation, open a SOP network and use the Tab menu.

- **HouIO > HouIO Round Trip** creates a configured Python SOP after the selected SOP.
- **HouIO > Convert Geometry File** converts a geometry file through HouIO.
- **HouIO > Package Diagnostics** reports the active package, converter, and Blosc paths.

The C++ converter runs as a separate process. Houdini loads only the Python bridge, avoiding Python ABI coupling between Houdini versions.

The Houdini bridge supports `.geo`, `.bgeo`, `.bgeo.sc`, and bounded 32-bit Float `.vdb` grids. Sparse VDB grids become dense Houdini volumes while retaining level-set or Fog semantics.
