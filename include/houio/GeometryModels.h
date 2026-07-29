#pragma once

#include <houio/Geometry.h>
#include <houio/HouGeo.h>

namespace houio
{
    /// Houdini-oriented geometry preserving supported point, vertex, primitive,
    /// global, group, topology, and primitive-record domains.
    using HoudiniGeometry = HouGeo;

    /// Render-oriented convenience mesh preserving supported numeric point,
    /// vertex, primitive, and global domains for one polygon primitive family.
    /// Strings, dictionaries, groups, and mixed primitive families remain on
    /// the Houdini-oriented model.
    using SimplifiedMesh = Geometry;
}
