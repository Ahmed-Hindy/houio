#pragma once

#include <cstddef>
#include <functional>
#include <ostream>
#include <span>
#include <vector>

#include <houio/GeometryIO.h>
#include <houio/json.h>
#include <houio/types.h>

namespace houio
{
    /// Validates and converts Houdini's tiled native-VDB primitive payload.
    ///
    /// Houdini stores one standard OpenVDB stream as a metadata object followed
    /// by fixed-size byte-array tiles. This codec does not interpret the tree.
    class NativeVdbPayload final
    {
    public:
        static constexpr std::size_t defaultTileSize = 4096;

        NativeVdbPayload() = delete;

        using StreamWriter = std::function<GeometryWriteResult(std::ostream&)>;

        [[nodiscard]] static GeometryReadResult<json::ArrayPtr> encode(
            std::span<const ubyte> openvdb_stream,
            std::size_t tile_size = defaultTileSize);

        [[nodiscard]] static GeometryReadResult<json::ArrayPtr> encodeStream(
            const StreamWriter& writer,
            std::size_t tile_size = defaultTileSize);

        [[nodiscard]] static GeometryReadResult<std::vector<ubyte>> decode(
            const json::ArrayPtr& payload);

        [[nodiscard]] static bool hasOpenVdbMagic(
            std::span<const ubyte> bytes) noexcept;
    };
}
