#include "TestSupport.h"

#include <houio/NativeVdbPayload.h>
#include <houio/OpenVdbBackend.h>
#include <houio/SparseGrid.h>

#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v<
    decltype(std::declval<const houio::SparseFloatGrid&>().background()), float>);
static_assert(std::is_same_v<
    decltype(std::declval<const houio::SparseInt32Grid&>().background()), houio::sint32>);

int main()
{
    houio::SparseFloatGrid emptyGrid;
    if( emptyGrid.activeBounds().has_value() )
        return houio::test::fail("empty sparse grid reported active bounds");

    houio::SparseFloatGrid grid(3.5f);
    grid.setName("density");
    grid.setGridClass(houio::SparseGridClass::fog_volume);
    grid.setMetadata("source", "unit_test");
    grid.setMetadata("creator", "houio_test");
    const houio::math::M44f transform(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.75f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.25f, 0.0f,
        10.0f, -2.0f, 4.0f, 1.0f);
    grid.setIndexToWorld(transform);
    grid.setVoxel(houio::math::V3i(-2, 4, 1), 0.25f);
    grid.setVoxel(houio::math::V3i(3, -1, 7), -2.0f);
    grid.setVoxel(houio::math::V3i(0, 0, 0), 3.5f);

    if( grid.background() != 3.5f || grid.name() != "density"
        || grid.gridClass() != houio::SparseGridClass::fog_volume )
    {
        return houio::test::fail("sparse grid identity metadata changed");
    }
    if( grid.metadata("source") != std::optional<std::string>("unit_test") )
        return houio::test::fail("sparse grid metadata lookup failed");
    if( grid.indexToWorld().ma != transform.ma )
        return houio::test::fail("sparse grid transform changed");
    if( grid.activeVoxelCount() != 3 || !grid.isActive(houio::math::V3i(0, 0, 0)) )
        return houio::test::fail("active voxel topology is incorrect");
    if( grid.value(houio::math::V3i(10, 10, 10)) != 3.5f )
        return houio::test::fail("inactive voxels did not return the background value");

    houio::SparseFloatGrid tiledGrid(0.0f);
    tiledGrid.setName("tiles");
    tiledGrid.addActiveTile(
        houio::SparseIndexBounds{
            houio::math::V3i(-8, -8, -8),
            houio::math::V3i(-1, -1, -1)},
        2.0f);
    tiledGrid.setVoxel(houio::math::V3i(-4, -4, -4), 5.0f);
    if( tiledGrid.activeTileCount() != 1
        || !tiledGrid.isActive(houio::math::V3i(-8, -8, -8))
        || tiledGrid.value(houio::math::V3i(-2, -2, -2)) != 2.0f
        || tiledGrid.value(houio::math::V3i(-4, -4, -4)) != 5.0f )
    {
        return houio::test::fail("active tile lookup or voxel override is incorrect");
    }
    const auto tiledBounds = tiledGrid.activeBounds();
    if( !tiledBounds
        || tiledBounds->minimum != houio::math::V3i(-8, -8, -8)
        || tiledBounds->maximum != houio::math::V3i(-1, -1, -1) )
    {
        return houio::test::fail("active tile bounds are incorrect");
    }
    if( const int result = houio::test::expectThrows<std::invalid_argument>(
            [&]()
            {
                tiledGrid.addActiveTile(
                    houio::SparseIndexBounds{
                        houio::math::V3i(1, 0, 0),
                        houio::math::V3i(0, 0, 0)},
                    1.0f);
            },
            "unordered active tile bounds must be rejected"); result != 0 )
    {
        return result;
    }

    houio::SparseInt32Grid intGrid(-1);
    intGrid.setName("labels");
    intGrid.setGridClass(houio::SparseGridClass::unknown);
    intGrid.setMetadata("creator", "houio_test");
    intGrid.setIndexToWorld(transform);
    intGrid.addActiveTile(
        houio::SparseIndexBounds{
            houio::math::V3i(8, 8, 8),
            houio::math::V3i(15, 15, 15)},
        7);
    intGrid.setVoxel(houio::math::V3i(10, 10, 10), 42);
    intGrid.setVoxel(houio::math::V3i(-1, -2, -3), -9);
    if( intGrid.background() != -1
        || intGrid.activeTileCount() != 1
        || intGrid.activeVoxelCount() != 2
        || intGrid.value(houio::math::V3i(9, 9, 9)) != 7
        || intGrid.value(houio::math::V3i(10, 10, 10)) != 42
        || intGrid.value(houio::math::V3i(0, 0, 0)) != -1 )
    {
        return houio::test::fail("SparseInt32Grid topology or values are incorrect");
    }

    const auto bounds = grid.activeBounds();
    if( !bounds || bounds->minimum != houio::math::V3i(-2, -1, 0)
        || bounds->maximum != houio::math::V3i(3, 4, 7) )
    {
        return houio::test::fail("active voxel bounds are incorrect");
    }

    const auto voxels = grid.activeVoxels();
    if( voxels.size() != 3
        || voxels[0].index != houio::math::V3i(0, 0, 0)
        || voxels[1].index != houio::math::V3i(-2, 4, 1)
        || voxels[2].index != houio::math::V3i(3, -1, 7) )
    {
        return houio::test::fail("active voxel traversal is not deterministic");
    }
    std::vector<houio::SparseFloatVoxel> traversedVoxels;
    grid.forEachActiveVoxel(
        [&](const houio::SparseFloatVoxel& voxel) { traversedVoxels.push_back(voxel); });
    if( traversedVoxels.size() != voxels.size() )
        return houio::test::fail("zero-copy active voxel traversal lost entries");
    for( std::size_t index = 0; index < voxels.size(); ++index )
    {
        if( traversedVoxels[index].index != voxels[index].index
            || traversedVoxels[index].value != voxels[index].value )
        {
            return houio::test::fail(
                "zero-copy active voxel traversal changed ordering or values");
        }
    }

    if( !grid.eraseVoxel(houio::math::V3i(0, 0, 0))
        || grid.eraseVoxel(houio::math::V3i(0, 0, 0))
        || grid.activeVoxelCount() != 2 )
    {
        return houio::test::fail("active voxel removal is incorrect");
    }

    if( const int result = houio::test::expectThrows<std::invalid_argument>(
            [&]()
            {
                grid.setVoxel(houio::math::V3i(1, 2, 3),
                    std::numeric_limits<float>::infinity());
            },
            "non-finite voxel values must be rejected"); result != 0 )
    {
        return result;
    }

    if( const int result = houio::test::expectThrows<std::invalid_argument>(
            [&]()
            {
                houio::math::M44f invalidTransform = houio::math::M44f::identity();
                invalidTransform.ma[5] = std::numeric_limits<float>::quiet_NaN();
                grid.setIndexToWorld(invalidTransform);
            },
            "non-finite transforms must be rejected"); result != 0 )
    {
        return result;
    }

    const std::vector<houio::ubyte> streamBytes{
        0x20U, 0x42U, 0x44U, 0x56U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U};
    const auto encodedPayload = houio::NativeVdbPayload::encode(streamBytes, 4);
    if( !encodedPayload || !encodedPayload.value || encodedPayload.value->size() != 4 )
        return houio::test::fail("native VDB stream was not tiled correctly");
    const auto decodedPayload = houio::NativeVdbPayload::decode(encodedPayload.value);
    if( !decodedPayload || decodedPayload.value != streamBytes )
        return houio::test::fail("native VDB tiled payload changed stream bytes");
    const auto streamedPayload = houio::NativeVdbPayload::encodeStream(
        [&](std::ostream& output)
        {
            output.write(
                reinterpret_cast<const char*>(streamBytes.data()), 3);
            output.write(
                reinterpret_cast<const char*>(streamBytes.data() + 3),
                static_cast<std::streamsize>(streamBytes.size() - 3));
            houio::GeometryWriteResult result;
            result.succeeded = static_cast<bool>(output);
            return result;
        },
        4);
    const auto streamedRoundTrip = streamedPayload
        ? houio::NativeVdbPayload::decode(streamedPayload.value)
        : houio::GeometryReadResult<std::vector<houio::ubyte>>{};
    if( !streamedRoundTrip || streamedRoundTrip.value != streamBytes )
        return houio::test::fail("streamed native VDB tiling changed bytes");

    const auto invalidPayload = houio::NativeVdbPayload::encode(
        std::span<const houio::ubyte>(streamBytes).subspan(1), 4);
    if( invalidPayload || invalidPayload.diagnostics.empty()
        || invalidPayload.diagnostics.front().category
            != houio::DiagnosticCategory::malformed_input )
    {
        return houio::test::fail("invalid native VDB stream magic was accepted");
    }

    const houio::OpenVdbBackendInfo backend = houio::OpenVdbBackend::info();
#if HOUIO_HAS_OPENVDB
    if( !backend.compiled || backend.version.empty() )
        return houio::test::fail("compiled OpenVDB backend did not report its version");
    const auto emptyStream = houio::OpenVdbBackend::decodeFloatGrid({});
    if( emptyStream || emptyStream.diagnostics.empty()
        || emptyStream.diagnostics.front().category
            != houio::DiagnosticCategory::malformed_input )
    {
        return houio::test::fail("empty OpenVDB stream was not rejected explicitly");
    }
    const auto encodedInt32Stream = houio::OpenVdbBackend::encodeInt32Grid(intGrid);
    const auto int32RoundTrip = encodedInt32Stream
        ? houio::OpenVdbBackend::decodeInt32Grid(encodedInt32Stream.value, "labels")
        : houio::GeometryReadResult<houio::SparseInt32Grid>{};
    if( !int32RoundTrip )
        return houio::test::fail("compiled OpenVDB Int32Grid round-trip failed to decode");
    if( int32RoundTrip.value.background() != -1
        || int32RoundTrip.value.activeTileCount() == 0
        || int32RoundTrip.value.value(houio::math::V3i(9, 9, 9)) != 7
        || int32RoundTrip.value.value(houio::math::V3i(10, 10, 10)) != 42
        || int32RoundTrip.value.value(houio::math::V3i(-1, -2, -3)) != -9 )
    {
        return houio::test::fail(
            "compiled OpenVDB Int32Grid round-trip changed topology or values: background="
            + std::to_string(int32RoundTrip.value.background())
            + ", voxels=" + std::to_string(int32RoundTrip.value.activeVoxelCount())
            + ", tiles=" + std::to_string(int32RoundTrip.value.activeTileCount())
            + ", tile_value="
            + std::to_string(int32RoundTrip.value.value(houio::math::V3i(9, 9, 9)))
            + ", override="
            + std::to_string(int32RoundTrip.value.value(houio::math::V3i(10, 10, 10)))
            + ", isolated="
            + std::to_string(int32RoundTrip.value.value(houio::math::V3i(-1, -2, -3))));
    }

    const auto encodedTileStream = houio::OpenVdbBackend::encodeFloatGrid(tiledGrid);
    const auto tileRoundTrip = encodedTileStream
        ? houio::OpenVdbBackend::decodeFloatGrid(encodedTileStream.value, "tiles")
        : houio::GeometryReadResult<houio::SparseFloatGrid>{};
    if( !tileRoundTrip || tileRoundTrip.value.activeTileCount() == 0
        || !tileRoundTrip.value.isActive(houio::math::V3i(-8, -8, -8))
        || tileRoundTrip.value.value(houio::math::V3i(-2, -2, -2)) != 2.0f
        || tileRoundTrip.value.value(houio::math::V3i(-4, -4, -4)) != 5.0f )
    {
        return houio::test::fail("compiled OpenVDB active tile round-trip changed topology or values");
    }

    const auto encodedStream = houio::OpenVdbBackend::encodeFloatGrid(grid);
    if( !encodedStream || !houio::NativeVdbPayload::hasOpenVdbMagic(encodedStream.value) )
        return houio::test::fail("compiled OpenVDB backend failed in-memory encoding");
    const auto memoryRoundTrip = houio::OpenVdbBackend::decodeFloatGrid(
        encodedStream.value, "density");
    if( !memoryRoundTrip
        || memoryRoundTrip.value.activeVoxelCount() != grid.activeVoxelCount()
        || memoryRoundTrip.value.background() != grid.background()
        || memoryRoundTrip.value.metadata("creator") != grid.metadata("creator") )
    {
        return houio::test::fail("compiled OpenVDB in-memory round-trip changed grid data");
    }
    for( const houio::SparseFloatVoxel& voxel : grid.activeVoxels() )
    {
        if( !memoryRoundTrip.value.isActive(voxel.index)
            || memoryRoundTrip.value.value(voxel.index) != voxel.value )
        {
            return houio::test::fail(
                "compiled OpenVDB in-memory round-trip changed voxel topology or values");
        }
    }
    const auto streamedNativePayload = houio::NativeVdbPayload::encodeStream(
        [&](std::ostream& output)
        {
            return houio::OpenVdbBackend::encodeFloatGrid(output, grid);
        });
    const auto streamedNativeBytes = streamedNativePayload
        ? houio::NativeVdbPayload::decode(streamedNativePayload.value)
        : houio::GeometryReadResult<std::vector<houio::ubyte>>{};
    if( !streamedNativeBytes )
        return houio::test::fail("compiled OpenVDB streaming payload failed");
    const auto streamedNativeGrid = houio::OpenVdbBackend::decodeFloatGrid(
        streamedNativeBytes.value, "density");
    if( !streamedNativeGrid
        || streamedNativeGrid.value.activeVoxelCount() != grid.activeVoxelCount() )
    {
        return houio::test::fail("compiled OpenVDB streaming payload changed grid data");
    }

    const auto nativePayload = houio::NativeVdbPayload::encode(encodedStream.value);
    if( !nativePayload )
        return houio::test::fail("OpenVDB stream could not be wrapped as a native payload");
    const auto unwrappedStream = houio::NativeVdbPayload::decode(nativePayload.value);
    if( !unwrappedStream || unwrappedStream.value != encodedStream.value )
        return houio::test::fail("native Houdini payload wrapping changed OpenVDB bytes");

    const std::filesystem::path int32VdbPath =
        std::filesystem::temp_directory_path() / "houio_sparse_int32_grid_test.vdb";
    std::error_code removeError;
    std::filesystem::remove(int32VdbPath, removeError);
    const auto int32WriteResult =
        houio::OpenVdbBackend::writeInt32Grid(int32VdbPath, intGrid);
    if( !int32WriteResult )
        return houio::test::fail("compiled OpenVDB backend failed to write Int32Grid");
    const auto int32ReadResult =
        houio::OpenVdbBackend::readInt32Grid(int32VdbPath, "labels");
    std::filesystem::remove(int32VdbPath, removeError);
    if( !int32ReadResult
        || int32ReadResult.value.background() != intGrid.background()
        || int32ReadResult.value.activeTileCount() == 0
        || int32ReadResult.value.indexToWorld().ma != intGrid.indexToWorld().ma
        || int32ReadResult.value.value(houio::math::V3i(9, 9, 9)) != 7
        || int32ReadResult.value.value(houio::math::V3i(10, 10, 10)) != 42
        || int32ReadResult.value.value(houio::math::V3i(-1, -2, -3)) != -9 )
    {
        return houio::test::fail(
            "compiled OpenVDB Int32Grid file round-trip changed grid data");
    }

    const std::filesystem::path vdbPath =
        std::filesystem::temp_directory_path() / "houio_sparse_grid_test.vdb";
    std::filesystem::remove(vdbPath, removeError);
    const auto writeResult = houio::OpenVdbBackend::writeFloatGrid(vdbPath, grid);
    if( !writeResult )
        return houio::test::fail("compiled OpenVDB backend failed to write FloatGrid");
    const auto readResult = houio::OpenVdbBackend::readFloatGrid(vdbPath, "density");
    std::filesystem::remove(vdbPath, removeError);
    if( !readResult || readResult.value.name() != "density"
        || readResult.value.gridClass() != houio::SparseGridClass::fog_volume
        || readResult.value.background() != grid.background()
        || readResult.value.activeVoxelCount() != grid.activeVoxelCount()
        || readResult.value.indexToWorld().ma != grid.indexToWorld().ma )
    {
        return houio::test::fail(
            "compiled OpenVDB FloatGrid round-trip changed grid metadata");
    }
    for( const houio::SparseFloatVoxel& voxel : grid.activeVoxels() )
    {
        if( !readResult.value.isActive(voxel.index)
            || readResult.value.value(voxel.index) != voxel.value )
        {
            return houio::test::fail(
                "compiled OpenVDB FloatGrid round-trip changed voxel topology or values");
        }
    }
#else
    if( backend.compiled || !backend.version.empty() )
        return houio::test::fail("disabled OpenVDB backend reported itself as compiled");
    const auto readResult = houio::OpenVdbBackend::readFloatGrid(
        std::filesystem::path("missing.vdb"));
    if( readResult || readResult.diagnostics.empty()
        || readResult.diagnostics.front().category
            != houio::DiagnosticCategory::unsupported_input )
    {
        return houio::test::fail(
            "disabled OpenVDB read did not return an unsupported diagnostic");
    }
    const auto writeResult = houio::OpenVdbBackend::writeFloatGrid(
        std::filesystem::path("unused.vdb"), grid);
    if( writeResult || writeResult.diagnostics.empty()
        || writeResult.diagnostics.front().category
            != houio::DiagnosticCategory::unsupported_input )
    {
        return houio::test::fail(
            "disabled OpenVDB write did not return an unsupported diagnostic");
    }
    const auto int32ReadResult = houio::OpenVdbBackend::readInt32Grid(
        std::filesystem::path("missing.vdb"));
    if( int32ReadResult || int32ReadResult.diagnostics.empty()
        || int32ReadResult.diagnostics.front().category
            != houio::DiagnosticCategory::unsupported_input )
    {
        return houio::test::fail(
            "disabled OpenVDB Int32Grid read did not return an unsupported diagnostic");
    }
    const auto int32WriteResult = houio::OpenVdbBackend::writeInt32Grid(
        std::filesystem::path("unused_int32.vdb"), intGrid);
    if( int32WriteResult || int32WriteResult.diagnostics.empty()
        || int32WriteResult.diagnostics.front().category
            != houio::DiagnosticCategory::unsupported_input )
    {
        return houio::test::fail(
            "disabled OpenVDB Int32Grid write did not return an unsupported diagnostic");
    }
#endif

    return 0;
}
