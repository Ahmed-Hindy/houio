#pragma once

#include <houio/Geometry.h>
#include <houio/HouGeo.h>

namespace houio
{
    /// Houdini-oriented geometry preserving supported point, vertex, primitive,
    /// global, group, topology, and primitive-record domains.
    using HoudiniGeometry = HouGeo;

    /// Render-oriented convenience mesh. Conversion from HoudiniGeometry may
    /// split points and omit domains that this simplified model cannot express.
    using SimplifiedMesh = Geometry;
}
