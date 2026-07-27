#include <houio/NativeVdbPayload.h>
#include <houio/OpenVdbBackend.h>
#include <houio/Writer.h>

#include "TestSupport.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

static_assert(static_cast<int>(houio::WriterDataType::sparse_openvdb) == 6);
static_assert(static_cast<int>(houio::WriterDataType::packed_disk_sequence) == 7);

namespace
{
    using houio::test::fail;

    houio::HouGeo::Ptr createHouGeo()
    {
        houio::Attribute::Ptr positions = houio::Attribute::createV4f();
        positions->appendElement(houio::math::V4f(1.0f, 2.0f, 3.0f, 1.0f));
        houio::HouGeo::Ptr geometry = houio::HouGeo::create();
        geometry->setPointAttribute(
            std::make_shared<houio::HouGeo::HouAttribute>("P", positions));
        return geometry;
    }

    bool containsMessage(const houio::DiagnosticList &diagnostics, const std::string &needle)
    {
        for( const houio::Diagnostic &diagnostic : diagnostics )
        {
            if( diagnostic.message.find(needle) != std::string::npos )
                return true;
        }
        return false;
    }

    int verifyRequestValidation()
    {
        const houio::WriteResult emptyDestination = houio::Writer::write(houio::WriteRequest{});
        if( emptyDestination || !containsMessage(emptyDestination.diagnostics, "destination") )
            return fail("Writer did not reject an empty destination");

        houio::WriteRequest emptySource;
        emptySource.destination = "unused.bgeo";
        const houio::WriteResult result = houio::Writer::write(emptySource);
        if( result || !containsMessage(result.diagnostics, "source") )
            return fail("Writer did not reject an empty source");
        return 0;
    }

    int verifyHouGeoWrite(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "nested" / "points.bgeo";
        const houio::WriteResult result = houio::Writer::write(
            path, std::static_pointer_cast<houio::HouGeoAdapter>(createHouGeo()));
        if( !result )
            return fail("Writer failed to serialize HouGeo");
        const auto readResult = houio::GeometryIO::readHouGeo(path);
        if( !readResult || readResult.value->pointCount() != 1 )
            return fail("Writer HouGeo output did not round-trip");
        return 0;
    }

    int verifyWritePolicies(const std::filesystem::path &directory)
    {
        const std::filesystem::path existing = directory / "existing.bgeo";
        if( !houio::Writer::write(
                existing, std::static_pointer_cast<houio::HouGeoAdapter>(createHouGeo())) )
        {
            return fail("Writer could not create overwrite-policy fixture");
        }

        houio::GeometryWriteOptions noOverwrite;
        noOverwrite.overwriteExisting = false;
        const houio::WriteResult rejected = houio::Writer::write(
            existing,
            std::static_pointer_cast<houio::HouGeoAdapter>(createHouGeo()),
            noOverwrite);
        if( rejected || !containsMessage(rejected.diagnostics, "already exists") )
            return fail("Writer did not enforce no-overwrite policy");
        if( !houio::GeometryIO::readHouGeo(existing) )
            return fail("No-overwrite failure damaged the existing output");

        houio::GeometryWriteOptions noDirectories;
        noDirectories.createParentDirectories = false;
        const houio::WriteResult missingDirectory = houio::Writer::write(
            directory / "missing" / "points.bgeo",
            std::static_pointer_cast<houio::HouGeoAdapter>(createHouGeo()),
            noDirectories);
        if( missingDirectory || !containsMessage(missingDirectory.diagnostics, "does not exist") )
            return fail("Writer did not enforce parent-directory policy");
        return 0;
    }

    int verifyConvenienceSources(const std::filesystem::path &directory)
    {
        houio::Geometry::Ptr mesh = houio::Geometry::createPointGeometry();
        mesh->attribute("P")->appendElement(houio::math::V3f(4.0f, 5.0f, 6.0f));
        const std::filesystem::path meshPath = directory / "mesh.bgeo";
        if( !houio::Writer::write(meshPath, mesh) )
            return fail("Writer failed to adapt simplified mesh source");
        const auto meshResult = houio::GeometryIO::readHouGeo(meshPath);
        if( !meshResult || meshResult.value->pointCount() != 1 )
            return fail("Simplified mesh source did not round-trip");

        houio::ScalarField::Ptr field = houio::ScalarField::create(houio::math::V3i(1, 1, 1));
        field->voxel(0, 0, 0) = 7.0f;
        const std::filesystem::path volumePath = directory / "volume.bgeo";
        if( !houio::Writer::write(volumePath, field) )
            return fail("Writer failed to adapt dense scalar volume source");
        const auto volumeResult = houio::GeometryIO::readVolume(volumePath);
        if( !volumeResult || volumeResult.value->voxel(0, 0, 0) != 7.0f )
            return fail("Dense scalar volume source did not round-trip");
        return 0;
    }

    int verifyAdvancedPrimitiveSources(const std::filesystem::path &directory)
    {
        houio::HouGeo::Ptr embedded = createHouGeo();
        houio::HouGeo::Ptr packedGeometry = createHouGeo();
        auto packedTopology = std::make_shared<houio::HouGeo::HouTopology>();
        packedTopology->appendIndex(0);
        packedGeometry->setTopology(packedTopology);
        auto packed = std::make_shared<houio::HouGeo::HouPackedGeometry>();
        packed->setEmbeddedGeometry(embedded);
        packed->setTopologyVertex(0);
        packed->setPivot(houio::math::V3f(0.25f, 0.5f, 0.75f));
        packed->setTreatAsFolder(true);
        packedGeometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::PackedGeometryPrimitive>(packed));

        const std::filesystem::path packedPath = directory / "packed.bgeo";
        if( !houio::Writer::write(
                packedPath,
                std::static_pointer_cast<houio::HouGeoAdapter>(packedGeometry)) )
        {
            return fail("Writer failed to serialize packed geometry");
        }
        const auto packedResult = houio::GeometryIO::readHouGeo(packedPath);
        if( !packedResult || packedResult.value->primitiveCount() != 1 )
            return fail("Packed geometry output did not round-trip");
        const auto packedPrimitive = std::dynamic_pointer_cast<houio::HouGeo::HouPackedGeometry>(
            packedResult.value->primitives().front());
        if( !packedPrimitive )
            return fail("Packed geometry record changed type");
        if( !packedPrimitive->embeddedGeometry() )
            return fail("Packed geometry embedded payload is missing");
        if( packedPrimitive->embeddedGeometry()->pointCount() != 1 )
        {
            return fail(
                "Packed geometry embedded point count changed to "
                + std::to_string(packedPrimitive->embeddedGeometry()->pointCount()));
        }
        if( packedPrimitive->pivot() != houio::math::V3f(0.25f, 0.5f, 0.75f) )
            return fail("Packed geometry pivot changed");
        if( !packedPrimitive->treatAsFolder() )
            return fail("Packed geometry folder flag changed");

        houio::HouGeo::Ptr fragmentGeometry = createHouGeo();
        auto fragmentTopology = std::make_shared<houio::HouGeo::HouTopology>();
        fragmentTopology->appendIndex(0);
        fragmentGeometry->setTopology(fragmentTopology);
        auto fragment = std::make_shared<houio::HouGeo::HouPackedFragment>();
        fragment->setEmbeddedGeometry(createHouGeo());
        fragment->setTopologyVertex(0);
        fragment->setFragmentAttribute("name");
        fragment->setFragmentName("piece0");
        fragment->setPivot(houio::math::V3f(0.5f, 0.25f, -0.75f));
        fragment->setTransform(houio::math::M33f(
            2.0f, 0.0f, 0.0f,
            0.0f, 3.0f, 0.0f,
            0.0f, 0.0f, 4.0f));
        fragment->setBounds({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
        fragment->setCachedBounds({-1.0f, 2.0f, 1.0f, 4.0f, 3.0f, 6.0f});
        fragmentGeometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::PackedFragmentPrimitive>(fragment));

        const std::filesystem::path fragmentPath = directory / "packed_fragment.bgeo";
        if( !houio::Writer::write(
                fragmentPath,
                std::static_pointer_cast<houio::HouGeoAdapter>(fragmentGeometry)) )
        {
            return fail("Writer failed to serialize packed fragment");
        }
        const auto fragmentResult = houio::GeometryIO::readHouGeo(fragmentPath);
        if( !fragmentResult || fragmentResult.value->primitiveCount() != 1 )
            return fail("Packed fragment output did not round-trip");
        const auto fragmentPrimitive =
            std::dynamic_pointer_cast<houio::HouGeo::HouPackedFragment>(
                fragmentResult.value->primitives().front());
        if( !fragmentPrimitive )
            return fail("Packed fragment record changed type");
        if( fragmentPrimitive->fragmentAttribute() != "name"
            || fragmentPrimitive->fragmentName() != "piece0" )
        {
            return fail("Packed fragment identity changed");
        }
        if( fragmentPrimitive->bounds()
                != houio::HouGeoAdapter::PackedFragmentPrimitive::Bounds{
                    0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f}
            || fragmentPrimitive->cachedBounds()
                != houio::HouGeoAdapter::PackedFragmentPrimitive::Bounds{
                    -1.0f, 2.0f, 1.0f, 4.0f, 3.0f, 6.0f} )
        {
            return fail("Packed fragment bounds changed");
        }

        houio::HouGeo::Ptr diskGeometry = createHouGeo();
        auto diskTopology = std::make_shared<houio::HouGeo::HouTopology>();
        diskTopology->appendIndex(0);
        diskGeometry->setTopology(diskTopology);
        auto packedDisk = std::make_shared<houio::HouGeo::HouPackedDisk>();
        packedDisk->setTopologyVertex(0);
        packedDisk->setFilename("$HIP/missing_payload.$F4.bgeo");
        packedDisk->setExpandFrame(24.5f);
        packedDisk->setExpandFilename(true);
        packedDisk->setPivot(houio::math::V3f(-0.5f, 0.25f, 1.5f));
        packedDisk->setTransform(houio::math::M33f(
            1.0f, 0.0f, 0.0f,
            0.0f, 2.0f, 0.0f,
            0.0f, 0.0f, 3.0f));
        packedDisk->setViewportLod("box");
        packedDisk->setPointInstanceTransform(true);
        packedDisk->setTreatAsFolder(true);
        diskGeometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::PackedDiskPrimitive>(packedDisk));

        const std::filesystem::path diskPath = directory / "packed_disk.bgeo";
        if( !houio::Writer::write(
                diskPath,
                std::static_pointer_cast<houio::HouGeoAdapter>(diskGeometry)) )
        {
            return fail("Writer failed to serialize packed disk reference");
        }
        const auto diskResult = houio::GeometryIO::readHouGeo(diskPath);
        if( !diskResult || diskResult.value->primitiveCount() != 1 )
            return fail("Packed disk reference did not round-trip");
        const auto diskPrimitive = std::dynamic_pointer_cast<houio::HouGeo::HouPackedDisk>(
            diskResult.value->primitives().front());
        if( !diskPrimitive )
            return fail("Packed disk record changed type");
        if( diskPrimitive->filename() != "$HIP/missing_payload.$F4.bgeo"
            || diskPrimitive->expandFrame() != 24.5f
            || !diskPrimitive->expandFilename() )
        {
            return fail("Packed disk path expansion metadata changed");
        }
        if( diskPrimitive->pivot() != houio::math::V3f(-0.5f, 0.25f, 1.5f)
            || diskPrimitive->viewportLod() != "box"
            || !diskPrimitive->pointInstanceTransform()
            || !diskPrimitive->treatAsFolder() )
        {
            return fail("Packed disk transform or flags changed");
        }

        houio::HouGeo::Ptr sequenceGeometry = createHouGeo();
        auto sequenceTopology = std::make_shared<houio::HouGeo::HouTopology>();
        sequenceTopology->appendIndex(0);
        sequenceGeometry->setTopology(sequenceTopology);
        auto sequence = std::make_shared<houio::HouGeo::HouPackedDiskSequence>();
        sequence->setTopologyVertex(0);
        sequence->setFilenames({"cache.0001.bgeo", "cache.0002.bgeo", "cache.0003.bgeo"});
        sequence->setIndex(1.25f);
        sequence->setWrapMode(
            houio::HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::mirror);
        sequence->setPivot(houio::math::V3f(0.25f, -0.5f, 1.0f));
        sequence->setViewportLod("box");
        sequence->setPointInstanceTransform(true);
        sequenceGeometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::PackedDiskSequencePrimitive>(sequence));
        const std::filesystem::path sequencePath = directory / "packed_disk_sequence.bgeo";
        if( !houio::Writer::write(
                sequencePath,
                std::static_pointer_cast<houio::HouGeoAdapter>(sequenceGeometry)) )
        {
            return fail("Writer failed to serialize packed disk sequence");
        }
        const auto sequenceResult = houio::GeometryIO::readHouGeo(sequencePath);
        if( !sequenceResult || sequenceResult.value->primitiveCount() != 1 )
            return fail("Packed disk sequence did not round-trip");
        const auto sequencePrimitive =
            std::dynamic_pointer_cast<houio::HouGeo::HouPackedDiskSequence>(
                sequenceResult.value->primitives().front());
        if( !sequencePrimitive
            || sequencePrimitive->filenames()
                != std::vector<std::string>{"cache.0001.bgeo", "cache.0002.bgeo", "cache.0003.bgeo"}
            || sequencePrimitive->index() != 1.25f
            || sequencePrimitive->wrapMode()
                != houio::HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::mirror )
        {
            return fail("Packed disk sequence metadata changed");
        }

        houio::HouGeo::Ptr vdbGeometry = createHouGeo();
        auto vdbTopology = std::make_shared<houio::HouGeo::HouTopology>();
        vdbTopology->appendIndex(0);
        vdbGeometry->setTopology(vdbTopology);
        auto payload = houio::json::Array::create();
        auto metadata = houio::json::Object::create();
        metadata->appendValue<houio::sint32>("tilesize", 4096);
        payload->append(metadata);
        auto opaqueBytes = houio::json::Array::create();
        opaqueBytes->appendValue<houio::sint32>(32);
        opaqueBytes->appendValue<houio::sint32>(66);
        opaqueBytes->appendValue<houio::sint32>(68);
        opaqueBytes->appendValue<houio::sint32>(86);
        payload->append(opaqueBytes);
        auto nativeVdb = std::make_shared<houio::HouGeo::HouVdb>();
        nativeVdb->setTopologyVertex(0);
        nativeVdb->setSerializedPayload(payload);
        vdbGeometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::NativeVdbPrimitive>(nativeVdb));

        const std::filesystem::path vdbPath = directory / "native_vdb.bgeo";
        if( !houio::Writer::write(
                vdbPath,
                std::static_pointer_cast<houio::HouGeoAdapter>(vdbGeometry)) )
        {
            return fail("Writer failed to serialize native VDB payload");
        }
        const auto vdbResult = houio::GeometryIO::readHouGeo(vdbPath);
        if( !vdbResult || vdbResult.value->primitiveCount() != 1 )
            return fail("Native VDB payload did not round-trip");
        const auto vdbPrimitive = std::dynamic_pointer_cast<houio::HouGeo::HouVdb>(
            vdbResult.value->primitives().front());
        if( !vdbPrimitive || !vdbPrimitive->serializedPayload()
            || vdbPrimitive->serializedPayload()->size() != 2 )
        {
            return fail("Native VDB serialized payload changed");
        }

        houio::HouGeo::Ptr sparseVdbGeometry = createHouGeo();
        auto sparseVdbTopology = std::make_shared<houio::HouGeo::HouTopology>();
        sparseVdbTopology->appendIndex(0);
        sparseVdbGeometry->setTopology(sparseVdbTopology);
        houio::SparseFloatGrid sparseGrid(0.0f);
        sparseGrid.setName("density");
        sparseGrid.setGridClass(houio::SparseGridClass::fog_volume);
        const houio::math::M44f sparseVdbTransform(
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            -0.75f, -0.75f, -0.75f, 1.0f);
        sparseGrid.setIndexToWorld(sparseVdbTransform);
        sparseGrid.setVoxel(houio::math::V3i(1, 1, 1), 1.0f);
        sparseGrid.setVoxel(houio::math::V3i(2, 1, 1), 2.0f);
        auto sparseVdb = std::make_shared<houio::HouGeo::HouSparseVdb>();
        sparseVdb->setTopologyVertex(0);
        sparseVdb->setSparseGrid(std::move(sparseGrid));
        sparseVdbGeometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::SparseVdbPrimitive>(sparseVdb));
        const std::filesystem::path sparseVdbPath = directory / "constructed_vdb.bgeo";
        const houio::WriteResult sparseWrite = houio::Writer::write(
            sparseVdbPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(sparseVdbGeometry));
#if HOUIO_HAS_OPENVDB
        if( !sparseWrite )
            return fail("OpenVDB-enabled writer failed to construct native VDB payload");
        const auto sparseResult = houio::GeometryIO::readHouGeo(sparseVdbPath);
        if( !sparseResult || sparseResult.value->primitiveCount() != 1 )
            return fail("Constructed native VDB payload did not round-trip");
        const auto constructedVdb = std::dynamic_pointer_cast<houio::HouGeo::HouVdb>(
            sparseResult.value->primitives().front());
        if( !constructedVdb || !constructedVdb->serializedPayload() )
            return fail("Constructed native VDB payload is missing");
        const auto stream = houio::NativeVdbPayload::decode(
            constructedVdb->serializedPayload());
        if( !stream )
            return fail("Constructed native VDB payload could not be decoded");
        const auto decodedGrid = houio::OpenVdbBackend::decodeFloatGrid(stream.value, "density");
        if( !decodedGrid || decodedGrid.value.activeVoxelCount() != 2
            || decodedGrid.value.gridClass() != houio::SparseGridClass::fog_volume
            || decodedGrid.value.indexToWorld().ma != sparseVdbTransform.ma
            || decodedGrid.value.value(houio::math::V3i(1, 1, 1)) != 1.0f
            || decodedGrid.value.value(houio::math::V3i(2, 1, 1)) != 2.0f )
        {
            return fail("Constructed native VDB payload changed sparse voxel data");
        }
#else
        if( sparseWrite || sparseWrite.diagnostics.empty()
            || sparseWrite.diagnostics.front().category
                != houio::DiagnosticCategory::unsupported_input )
        {
            return fail("Backend-off sparse VDB write did not return an unsupported diagnostic");
        }
        if( std::filesystem::exists(sparseVdbPath) )
            return fail("Backend-off sparse VDB write created a partial file");
#endif

        houio::HouGeo::Ptr cyclicGeometry = createHouGeo();
        auto cyclicTopology = std::make_shared<houio::HouGeo::HouTopology>();
        cyclicTopology->appendIndex(0);
        cyclicGeometry->setTopology(cyclicTopology);
        auto cyclicPacked = std::make_shared<houio::HouGeo::HouPackedGeometry>();
        cyclicPacked->setEmbeddedGeometry(cyclicGeometry);
        cyclicPacked->setTopologyVertex(0);
        cyclicGeometry->addPrimitive(
            std::static_pointer_cast<houio::HouGeoAdapter::PackedGeometryPrimitive>(cyclicPacked));
        const std::filesystem::path cyclicPath = directory / "cyclic_packed.bgeo";
        const houio::WriteResult cyclicResult = houio::Writer::write(
            cyclicPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(cyclicGeometry));
        cyclicPacked->setEmbeddedGeometry(createHouGeo());
        if( cyclicResult || !containsMessage(cyclicResult.diagnostics, "cyclic") )
            return fail("Writer did not reject cyclic packed geometry");
        if( std::filesystem::exists(cyclicPath) )
            return fail("Cyclic packed geometry created a partial output file");
        return 0;
    }

    int verifyCapabilities()
    {
        const auto &capabilities = houio::Writer::capabilities();
        if( capabilities.size() < 7 )
            return fail("Writer capability table is incomplete");
        const auto geometry = houio::Writer::capability(
            houio::WriterDataType::houdini_geometry);
        if( !geometry || geometry->level != houio::WriterCapabilityLevel::supported
            || !geometry->writable )
        {
            return fail("Houdini geometry capability is not reported as writable");
        }
        const auto packed = houio::Writer::capability(
            houio::WriterDataType::packed_geometry);
        if( !packed || packed->level != houio::WriterCapabilityLevel::supported
            || !packed->readable || !packed->writable )
        {
            return fail("Packed geometry capability contract is incorrect");
        }
        const auto fragment = houio::Writer::capability(
            houio::WriterDataType::packed_fragment);
        if( !fragment || fragment->level != houio::WriterCapabilityLevel::supported
            || !fragment->readable || !fragment->writable )
        {
            return fail("Packed fragment capability contract is incorrect");
        }
        const auto disk = houio::Writer::capability(
            houio::WriterDataType::packed_disk);
        if( !disk || disk->level != houio::WriterCapabilityLevel::supported
            || !disk->readable || !disk->writable )
        {
            return fail("Packed disk capability contract is incorrect");
        }
        const auto sequence = houio::Writer::capability(
            houio::WriterDataType::packed_disk_sequence);
        if( !sequence || sequence->level != houio::WriterCapabilityLevel::supported
            || !sequence->readable || !sequence->writable )
        {
            return fail("Packed disk sequence capability contract is incorrect");
        }
        const auto vdb = houio::Writer::capability(
            houio::WriterDataType::sparse_openvdb);
        if( !vdb || vdb->level != houio::WriterCapabilityLevel::supported
            || !vdb->readable || !vdb->writable
            || vdb->detail.find("active-tile") == std::string::npos )
        {
            return fail("Sparse OpenVDB capability contract is incorrect");
        }
        return 0;
    }
}

int main()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "houio_writer_test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    if( error )
        return fail("Could not create Writer test directory");

    if( const int result = verifyRequestValidation(); result != 0 )
        return result;
    if( const int result = verifyHouGeoWrite(directory); result != 0 )
        return result;
    if( const int result = verifyWritePolicies(directory); result != 0 )
        return result;
    if( const int result = verifyConvenienceSources(directory); result != 0 )
        return result;
    if( const int result = verifyAdvancedPrimitiveSources(directory); result != 0 )
        return result;
    if( const int result = verifyCapabilities(); result != 0 )
        return result;

    std::filesystem::remove_all(directory, error);
    return 0;
}
