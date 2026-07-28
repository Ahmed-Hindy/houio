#pragma once

#include <cstddef>
#include <cstdint>

#include <houio/NativePolygonWriter.h>

#if defined(__cplusplus)
extern "C"
{
#endif

struct HouIONativeSceneArchiveOptions
{
    const char* destination_utf8;
    double frames_per_second;
    double start_frame;
    std::uint8_t create_parent_directories;
    std::uint8_t overwrite_existing;
    std::uint8_t atomic_replace;
};

struct HouIONativeSceneArchive;

/// Open one Alembic or USD archive through HouIO's standalone scene writer.
/// Returns null on failure and writes a truncated null-terminated diagnostic
/// when an error buffer is supplied.
HouIONativeSceneArchive* houio_create_native_scene_archive(
    const HouIONativeSceneArchiveOptions* options,
    char* error_buffer,
    std::size_t error_capacity);

/// Append one polygon/polyline sample to an open scene archive.
int houio_write_native_scene_sample(
    HouIONativeSceneArchive* archive,
    const HouIONativePolygonWriteRequest* sample,
    double frame,
    char* error_buffer,
    std::size_t error_capacity);

/// Finalize and publish the archive.
int houio_finish_native_scene_archive(
    HouIONativeSceneArchive* archive,
    char* error_buffer,
    std::size_t error_capacity);

/// Destroy an archive handle. Unfinished atomic output is discarded.
void houio_destroy_native_scene_archive(HouIONativeSceneArchive* archive);

#if defined(__cplusplus)
}
#endif
