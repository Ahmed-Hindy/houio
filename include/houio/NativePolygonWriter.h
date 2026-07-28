#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__cplusplus)
extern "C"
{
#endif

/// One polygon range in a flat topology array supplied by an HDK consumer.
struct HouIONativePolygon
{
    std::size_t vertex_offset;
    std::size_t vertex_count;
    std::uint8_t closed;
};

/// Dependency-neutral polygon payload accepted by HouIO's native bridge.
///
/// Positions contain four float components per point. Topology entries are
/// zero-based point indices. All pointer storage only needs to remain valid for
/// the duration of houio_write_native_polygons().
struct HouIONativePolygonWriteRequest
{
    const float* positions_xyzw;
    std::size_t point_count;
    const std::int32_t* topology;
    std::size_t vertex_count;
    const HouIONativePolygon* polygons;
    std::size_t polygon_count;
    const char* destination_utf8;
    const char* blosc_library_utf8;
    std::uint8_t create_parent_directories;
    std::uint8_t overwrite_existing;
    std::uint8_t atomic_replace;
};

/// Write polygons through HouIO without exposing C++20 or STL types to HDK.
///
/// Returns zero on success and nonzero on failure. When error_buffer is not
/// null and error_capacity is positive, a null-terminated diagnostic is
/// written and truncated safely when necessary.
int houio_write_native_polygons(
    const HouIONativePolygonWriteRequest* request,
    char* error_buffer,
    std::size_t error_capacity);

#if defined(__cplusplus)
}
#endif
