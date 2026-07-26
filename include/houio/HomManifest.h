#pragma once

#include <filesystem>

#include <houio/GeometryIO.h>

namespace houio
{
    /// Reader for the HouIO-owned HOM interchange manifest.
    ///
    /// The manifest is an implementation-neutral JSON representation extracted
    /// directly through HOM. It is not a Houdini geometry file and does not use
    /// Houdini's file writers. The resulting HouGeo can be serialized through
    /// Writer or GeometryIO.
    class HomManifest final
    {
    public:
        HomManifest() = delete;

        [[nodiscard]] static GeometryReadResult<HouGeo::Ptr> read(
            const std::filesystem::path &path,
            const json::ParserLimits &limits = {});
    };
}
