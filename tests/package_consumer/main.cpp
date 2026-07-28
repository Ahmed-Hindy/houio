#include <houio/GeometryIO.h>
#include <houio/GeometryModels.h>
#include <houio/HomManifest.h>
#include <houio/NativeVdbPayload.h>
#include <houio/OpenVdbBackend.h>
#include <houio/SparseGrid.h>
#include <houio/Writer.h>

#include <filesystem>
#include <iostream>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<houio::HoudiniGeometry, houio::HouGeo>);
static_assert(std::is_same_v<houio::SimplifiedMesh, houio::Geometry>);

int main()
{
    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "houio_package_consumer.bgeo";

    houio::SimplifiedMesh::Ptr sourceGeometry =
        houio::SimplifiedMesh::createQuad(houio::SimplifiedMesh::QUAD);
    const houio::WriteResult writeResult =
        houio::Writer::write(outputPath, sourceGeometry);
    if (!sourceGeometry || !writeResult)
    {
        std::cerr << "failed to export geometry through installed HouIO package\n";
        return 1;
    }

    const houio::GeometryReadResult<houio::SimplifiedMesh::Ptr> readResult =
        houio::GeometryIO::readGeometry(outputPath);
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    const auto packedCapability =
        houio::Writer::capability(houio::WriterDataType::packed_geometry);
    const auto fragmentCapability =
        houio::Writer::capability(houio::WriterDataType::packed_fragment);
    const auto diskCapability =
        houio::Writer::capability(houio::WriterDataType::packed_disk);
    const auto sequenceCapability =
        houio::Writer::capability(houio::WriterDataType::packed_disk_sequence);
    const auto vdbCapability =
        houio::Writer::capability(houio::WriterDataType::sparse_openvdb);
    const auto curveCapability =
        houio::Writer::capability(houio::WriterDataType::curves);
    const auto quadricCapability =
        houio::Writer::capability(houio::WriterDataType::quadrics);
    houio::SparseFloatGrid sparseGrid(0.0f);
    sparseGrid.addActiveTile(
        houio::SparseIndexBounds{
            houio::math::V3i(-8, -8, -8),
            houio::math::V3i(-1, -1, -1)},
        2.0f);
    sparseGrid.setVoxel(houio::math::V3i(1, 2, 3), 4.0f);
    houio::SparseInt32Grid sparseIntGrid(-1);
    sparseIntGrid.setName("package_labels");
    sparseIntGrid.addActiveTile(
        houio::SparseIndexBounds{
            houio::math::V3i(8, 8, 8),
            houio::math::V3i(15, 15, 15)},
        7);
    sparseIntGrid.setVoxel(houio::math::V3i(16, 16, 16), 42);
    houio::SparseVec3fGrid sparseVecGrid(houio::math::V3f(0.0f));
    sparseVecGrid.setName("package_velocity");
    sparseVecGrid.setGridClass(houio::SparseGridClass::staggered);
    sparseVecGrid.setVectorType(houio::SparseVectorType::contravariant_relative);
    sparseVecGrid.addActiveTile(
        houio::SparseIndexBounds{
            houio::math::V3i(24, 24, 24),
            houio::math::V3i(31, 31, 31)},
        houio::math::V3f(1.0f, 2.0f, 3.0f));
    sparseVecGrid.setVoxel(
        houio::math::V3i(40, 40, 40),
        houio::math::V3f(-1.0f, 0.5f, 4.0f));
    const std::vector<houio::ubyte> sampleStream{
        0x20U, 0x42U, 0x44U, 0x56U, 0x01U, 0x02U, 0x03U};
    const auto samplePayload = houio::NativeVdbPayload::encode(sampleStream, 4);
    const auto sampleRoundTrip = samplePayload
        ? houio::NativeVdbPayload::decode(samplePayload.value)
        : houio::GeometryReadResult<std::vector<houio::ubyte>>{};
    const houio::OpenVdbBackendInfo openVdb = houio::OpenVdbBackend::info();
    const bool openVdbMetadataIsValid =
        openVdb.compiled ? !openVdb.version.empty() : openVdb.version.empty();
    bool compiledBackendWorks = true;
    if( openVdb.compiled )
    {
        sparseGrid.setName("package_consumer");
        const auto encodedGrid = houio::OpenVdbBackend::encodeFloatGrid(sparseGrid);
        const auto decodedGrid = encodedGrid
            ? houio::OpenVdbBackend::decodeFloatGrid(encodedGrid.value, "package_consumer")
            : houio::GeometryReadResult<houio::SparseFloatGrid>{};
        const auto encodedIntGrid = houio::OpenVdbBackend::encodeInt32Grid(sparseIntGrid);
        const auto decodedIntGrid = encodedIntGrid
            ? houio::OpenVdbBackend::decodeInt32Grid(
                encodedIntGrid.value, "package_labels")
            : houio::GeometryReadResult<houio::SparseInt32Grid>{};
        const auto encodedVecGrid = houio::OpenVdbBackend::encodeVec3fGrid(sparseVecGrid);
        const auto decodedVecGrid = encodedVecGrid
            ? houio::OpenVdbBackend::decodeVec3fGrid(
                encodedVecGrid.value, "package_velocity")
            : houio::GeometryReadResult<houio::SparseVec3fGrid>{};
        compiledBackendWorks = decodedGrid
            && decodedGrid.value.activeVoxelCount() == 1
            && decodedGrid.value.activeTileCount() != 0
            && decodedGrid.value.value(houio::math::V3i(-4, -4, -4)) == 2.0f
            && decodedGrid.value.value(houio::math::V3i(1, 2, 3)) == 4.0f
            && decodedIntGrid
            && decodedIntGrid.value.activeTileCount() != 0
            && decodedIntGrid.value.value(houio::math::V3i(9, 9, 9)) == 7
            && decodedIntGrid.value.value(houio::math::V3i(16, 16, 16)) == 42
            && decodedVecGrid
            && decodedVecGrid.value.gridClass() == houio::SparseGridClass::staggered
            && decodedVecGrid.value.vectorType()
                == houio::SparseVectorType::contravariant_relative
            && decodedVecGrid.value.activeTileCount() != 0
            && decodedVecGrid.value.value(houio::math::V3i(25, 25, 25))
                == houio::math::V3f(1.0f, 2.0f, 3.0f)
            && decodedVecGrid.value.value(houio::math::V3i(40, 40, 40))
                == houio::math::V3f(-1.0f, 0.5f, 4.0f);
    }
    if (!readResult || readResult.value->primitiveCount() != 1
        || !packedCapability || !packedCapability->writable
        || !fragmentCapability || !fragmentCapability->writable
        || !diskCapability || !diskCapability->writable
        || !sequenceCapability || !sequenceCapability->writable
        || !vdbCapability || !vdbCapability->writable
        || !curveCapability || !curveCapability->readable || !curveCapability->writable
        || !quadricCapability || !quadricCapability->readable || !quadricCapability->writable
        || sparseGrid.activeVoxelCount() != 1
        || sparseGrid.activeTileCount() != 1
        || sparseIntGrid.activeVoxelCount() != 1
        || sparseIntGrid.activeTileCount() != 1
        || sparseIntGrid.value(houio::math::V3i(16, 16, 16)) != 42
        || sparseVecGrid.activeVoxelCount() != 1
        || sparseVecGrid.activeTileCount() != 1
        || sparseVecGrid.vectorType()
            != houio::SparseVectorType::contravariant_relative
        || !sampleRoundTrip || sampleRoundTrip.value != sampleStream
        || !compiledBackendWorks
        || !openVdbMetadataIsValid
        || openVdb.detail.empty())
    {
        std::cerr << "failed to import geometry through installed HouIO package\n";
        return 1;
    }

    return 0;
}
