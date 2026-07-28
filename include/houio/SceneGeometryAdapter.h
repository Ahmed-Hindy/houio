#pragma once

#include <houio/Geometry.h>
#include <houio/HouGeoAdapter.h>
#include <houio/SceneArchive.h>

namespace houio
{
    /// Convert the supported polygon/polyline subset of a HouGeo adapter into
    /// the dependency-neutral sample consumed by Alembic and USD writers.
    /// Unsupported attributes, groups, primitive families, and true spline
    /// curves fail explicitly rather than being discarded.
    [[nodiscard]] SceneGeometrySample adaptSceneGeometry(
        const HouGeoAdapter& geometry);

    /// Convert HouIO's simplified mesh model into a scene sample.
    [[nodiscard]] SceneGeometrySample adaptSceneGeometry(
        const Geometry& geometry);
}
