#include "TestSupport.h"

#include <houio/OpenVdbBackend.h>
#include <houio/SparseGrid.h>

#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

int main()
{
    houio::SparseFloatGrid emptyGrid;
    if( emptyGrid.activeBounds().has_value() )
        return houio::test::fail("empty sparse grid reported active bounds");

    houio::SparseFloatGrid grid(3.5f);
    grid.setName("density");
    grid.setGridClass(houio::SparseGridClass::fog_volume);
    grid.setMetadata("source", "unit_test");
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

    const houio::OpenVdbBackendInfo backend = houio::OpenVdbBackend::info();
#if HOUIO_HAS_OPENVDB
    if( !backend.compiled || backend.version.empty() )
        return houio::test::fail("compiled OpenVDB backend did not report its version");
    const std::filesystem::path vdbPath =
        std::filesystem::temp_directory_path() / "houio_sparse_grid_test.vdb";
    std::error_code removeError;
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
#endif

    return 0;
}
