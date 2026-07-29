#include <houio/GeometryIO.h>
#include <houio/HomManifest.h>
#include <houio/NativeVdbPayload.h>
#include <houio/OpenVdbBackend.h>
#include <houio/Writer.h>

#include "TestSupport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>

namespace
{
    using houio::test::fail;

    void writeText(const std::filesystem::path &path, const std::string &text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
    }

    bool containsCategory(
        const houio::DiagnosticList &diagnostics,
        houio::DiagnosticCategory category)
    {
        for( const houio::Diagnostic &diagnostic : diagnostics )
        {
            if( diagnostic.category == category )
                return true;
        }
        return false;
    }

    bool matchesUInt8(
        const houio::HouGeoAdapter::AttributeAdapter::ConstPtr &attribute,
        std::initializer_list<houio::ubyte> expected)
    {
        if( !attribute
            || attribute->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::uint8
            || attribute->elementCount() != static_cast<int>(expected.size()) )
        {
            return false;
        }
        std::size_t index = 0;
        for( const houio::ubyte value : expected )
        {
            if( attribute->rawData().read<houio::ubyte>(index) != value )
                return false;
            ++index;
        }
        return true;
    }

    int verifyMixedManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path manifestPath = directory / "mixed.json";
        writeText(manifestPath, R"JSON({
            "schema":"houio.hom/1",
            "point_count":4,
            "vertex_count":4,
            "primitive_count":2,
            "topology":[0,1,2,3],
            "primitives":[
                {"type":"polygon","vertex_offset":0,"vertex_count":3,"closed":true},
                {
                    "type":"dense_volume",
                    "vertex_offset":3,
                    "resolution":[1,1,1],
                    "local_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                    "voxels":[4.5],
                    "visualization":{"mode":"smoke","iso":0.25,"density":2.0}
                }
            ],
            "attributes":{
                "point":[
                    {
                        "name":"P","kind":"numeric","storage":"float32",
                        "tuple_size":4,"element_count":4,
                        "values":[0,0,0,1,1,0,0,1,0,1,0,1,0.5,0.5,0.5,1]
                    },
                    {
                        "name":"mask","kind":"numeric","storage":"uint8",
                        "tuple_size":1,"element_count":4,"values":[0,128,255,64]
                    },
                    {
                        "name":"label","kind":"string","tuple_size":1,
                        "element_count":4,"values":["a","b","c","volume"]
                    }
                ],
                "vertex":[
                    {
                        "name":"uv","kind":"numeric","storage":"float32",
                        "tuple_size":2,"element_count":4,
                        "values":[0,0,1,0,0,1,0.5,0.5]
                    },
                    {
                        "name":"vertex_mask","kind":"numeric","storage":"uint8",
                        "tuple_size":1,"element_count":4,"values":[255,0,128,64]
                    }
                ],
                "primitive":[
                    {
                        "name":"kind","kind":"string","tuple_size":1,
                        "element_count":2,"values":["polygon","volume"]
                    },
                    {
                        "name":"primitive_mask","kind":"numeric","storage":"uint8",
                        "tuple_size":1,"element_count":2,"values":[128,255]
                    }
                ],
                "global":[
                    {
                        "name":"asset","kind":"string","tuple_size":1,
                        "element_count":1,"values":["manifest_test"]
                    },
                    {
                        "name":"global_mask","kind":"numeric","storage":"uint8",
                        "tuple_size":1,"element_count":1,"values":[255]
                    }
                ]
            },
            "groups":{
                "point":{"selected":[0,3]},
                "vertex":{"corner":[1]},
                "primitive":{"volumes":[1]}
            }
        })JSON");

        const houio::GeometryReadResult<houio::HouGeo::Ptr> result =
            houio::HomManifest::read(manifestPath);
        if( !result )
        {
            for( const houio::Diagnostic &diagnostic : result.diagnostics )
                std::cerr << diagnostic.message << " [" << diagnostic.path << "]\n";
            return fail("mixed HOM manifest failed to load");
        }
        if( result.value->pointCount() != 4
            || result.value->vertexCount() != 4
            || result.value->primitiveCount() != 2 )
        {
            return fail("mixed HOM manifest domain counts are incorrect");
        }
        if( !matchesUInt8(result.value->pointAttribute("mask"), {0, 128, 255, 64})
            || !matchesUInt8(
                result.value->vertexAttribute("vertex_mask"), {255, 0, 128, 64})
            || !matchesUInt8(
                result.value->primitiveAttribute("primitive_mask"), {128, 255})
            || !matchesUInt8(result.value->globalAttribute("global_mask"), {255}) )
        {
            return fail("mixed HOM manifest lost UInt8 attribute storage across domains");
        }
        if( !result.value->pointAttribute("label")
            || !result.value->vertexAttribute("uv")
            || !result.value->primitiveAttribute("kind")
            || !result.value->globalAttribute("asset") )
        {
            return fail("mixed HOM manifest lost attributes");
        }
        const auto volumeGroup = result.value->primitiveGroupMembership("volumes");
        if( !volumeGroup || volumeGroup->size() != 2 || !(*volumeGroup)[1] )
            return fail("mixed HOM manifest lost primitive group membership");

        const auto primitives = result.value->primitives();
        if( primitives.size() != 2 )
            return fail("mixed HOM manifest did not retain two records");
        const auto volume = std::dynamic_pointer_cast<houio::HouGeo::HouVolume>(primitives[1]);
        if( !volume || volume->voxelValue(0, 0, 0) != 4.5f
            || volume->visualizationDensity() != 2.0f )
        {
            return fail("mixed HOM manifest did not retain dense volume data");
        }

        const std::filesystem::path outputPath = directory / "mixed.bgeo";
        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(result.value));
        if( !writeResult )
            return fail("mixed HOM manifest failed custom serialization");
        const auto roundtrip = houio::GeometryIO::readHouGeo(outputPath);
        if( !roundtrip || roundtrip.value->primitiveCount() != 2
            || !roundtrip.value->vertexAttribute("uv")
            || !matchesUInt8(roundtrip.value->pointAttribute("mask"), {0, 128, 255, 64})
            || !matchesUInt8(
                roundtrip.value->vertexAttribute("vertex_mask"), {255, 0, 128, 64})
            || !matchesUInt8(
                roundtrip.value->primitiveAttribute("primitive_mask"), {128, 255})
            || !matchesUInt8(roundtrip.value->globalAttribute("global_mask"), {255}) )
        {
            return fail("mixed HOM manifest output did not round-trip");
        }
        return 0;
    }

    int verifyInvalidTopologyOffsets(const std::filesystem::path &directory)
    {
        const std::filesystem::path densePath = directory / "invalid_dense_offset.json";
        writeText(densePath, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"dense_volume","vertex_offset":1,"resolution":[1,1,1],
                "local_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "voxels":[1]
            }],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto denseResult = houio::HomManifest::read(densePath);
        if( denseResult
            || !containsCategory(denseResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("dense-volume manifest accepted an out-of-range topology vertex");
        }

        const std::filesystem::path packedPath = directory / "invalid_packed_offset.json";
        writeText(packedPath, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"packed_geometry","vertex_offset":1,
                "pivot":[0,0,0],
                "transform":[1,0,0,0,1,0,0,0,1],
                "embedded_manifest":{
                    "schema":"houio.hom/1",
                    "point_count":0,"vertex_count":0,"primitive_count":0,
                    "topology":[],"primitives":[],
                    "attributes":{"point":[],"vertex":[],"primitive":[],"global":[]}
                }
            }],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto packedResult = houio::HomManifest::read(packedPath);
        if( packedResult
            || !containsCategory(packedResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("packed manifest accepted an out-of-range topology vertex");
        }
        return 0;
    }

    int verifyPackedFragmentManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "packed_fragment.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"packed_fragment","vertex_offset":0,
                "pivot":[0.25,0.5,0.75],
                "transform":[2,0,0,0,3,0,0,0,4],
                "fragment_attribute":"name","fragment_name":"piece0",
                "bounds":[0,1,2,3,4,5],
                "cached_bounds":[-1,2,1,4,3,6],
                "embedded_manifest":{
                    "schema":"houio.hom/1",
                    "point_count":0,"vertex_count":0,"primitive_count":0,
                    "topology":[],"primitives":[],
                    "attributes":{"point":[],"vertex":[],"primitive":[],"global":[]}
                }
            }],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 1 )
            return fail("packed fragment manifest failed to load");
        const auto fragment = std::dynamic_pointer_cast<houio::HouGeo::HouPackedFragment>(
            result.value->primitives().front());
        if( !fragment || fragment->fragmentAttribute() != "name"
            || fragment->fragmentName() != "piece0" )
        {
            return fail("packed fragment manifest lost fragment identity");
        }
        if( fragment->bounds()
                != houio::HouGeoAdapter::PackedFragmentPrimitive::Bounds{
                    0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f}
            || fragment->cachedBounds()
                != houio::HouGeoAdapter::PackedFragmentPrimitive::Bounds{
                    -1.0f, 2.0f, 1.0f, 4.0f, 3.0f, 6.0f} )
        {
            return fail("packed fragment manifest lost bounds");
        }
        return 0;
    }

    int verifyPackedDiskManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "packed_disk.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"packed_disk","vertex_offset":0,
                "filename":"$HIP/cache/payload.$F4.bgeo",
                "expand_frame":24.5,"expand_filename":true,
                "pivot":[0.25,0.5,0.75],
                "transform":[2,0,0,0,3,0,0,0,4],
                "viewport_lod":"box",
                "point_instance_transform":true,
                "treat_as_folder":true
            }],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 1 )
            return fail("packed disk manifest failed to load");
        const auto disk = std::dynamic_pointer_cast<houio::HouGeo::HouPackedDisk>(
            result.value->primitives().front());
        if( !disk || disk->filename() != "$HIP/cache/payload.$F4.bgeo"
            || disk->expandFrame() != 24.5f || !disk->expandFilename()
            || disk->viewportLod() != "box"
            || !disk->pointInstanceTransform() || !disk->treatAsFolder() )
        {
            return fail("packed disk manifest lost reference metadata");
        }
        return 0;
    }

    int verifyPackedDiskSequenceManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "packed_disk_sequence.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,
            "vertex_count":1,
            "primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"packed_disk_sequence","vertex_offset":0,
                "filenames":["cache.0001.bgeo","cache.0002.bgeo","cache.0003.bgeo"],
                "index":1.5,"wrap":"mirror",
                "pivot":[0.25,0.5,0.75],
                "transform":[2,0,0,0,3,0,0,0,4],
                "viewport_lod":"box",
                "point_instance_transform":true
            }],
            "attributes":{
                "point":[{
                    "name":"P","kind":"numeric","storage":"float32",
                    "tuple_size":4,"element_count":1,"values":[0,0,0,1]
                }],
                "vertex":[],"primitive":[],"global":[]
            }
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 1 )
            return fail("packed disk sequence manifest failed to load");
        const auto sequence =
            std::dynamic_pointer_cast<houio::HouGeo::HouPackedDiskSequence>(
                result.value->primitives().front());
        if( !sequence || sequence->filenames().size() != 3
            || sequence->index() != 1.5f
            || sequence->wrapMode()
                != houio::HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::mirror
            || sequence->viewportLod() != "box"
            || !sequence->pointInstanceTransform() )
        {
            return fail("packed disk sequence manifest lost metadata");
        }
        return 0;
    }

    int verifySparseVdbManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "sparse_float_vdb.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_float_vdb","vertex_offset":0,
                "name":"density","grid_class":"fog_volume","background":0.0,
                "index_to_world":[0.5,0,0,0,0,0.5,0,0,0,0,0.5,0,-0.75,-0.75,-0.75,1],
                "active_indices":[1,1,1,2,1,1],
                "active_values":[1.0,2.0],
                "active_tiles":[{
                    "minimum":[-8,-8,-8],"maximum":[-1,-1,-1],"value":0.25
                }],
                "metadata":{"creator":"houio_test"}
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[-0.75,-0.75,-0.75,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 1 )
            return fail("sparse VDB manifest failed to load");
        const auto sparseVdb =
            std::dynamic_pointer_cast<houio::HouGeo::HouSparseVdb>(
                result.value->primitives().front());
        if( !sparseVdb || sparseVdb->topologyVertex() != 0 )
            return fail("sparse VDB manifest did not create the expected primitive");
        const houio::SparseFloatGrid& grid = sparseVdb->sparseGrid();
        if( grid.name() != "density"
            || grid.gridClass() != houio::SparseGridClass::fog_volume
            || grid.activeVoxelCount() != 2
            || grid.activeTileCount() != 1
            || grid.value(houio::math::V3i(1, 1, 1)) != 1.0f
            || grid.value(houio::math::V3i(2, 1, 1)) != 2.0f
            || grid.value(houio::math::V3i(-4, -4, -4)) != 0.25f
            || grid.metadata("creator") != std::optional<std::string>("houio_test") )
        {
            return fail("sparse VDB manifest changed grid data");
        }

        const std::filesystem::path tileOnlyPath =
            directory / "tile_only_sparse_float_vdb.json";
        writeText(tileOnlyPath, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_float_vdb","vertex_offset":0,
                "name":"tiles","grid_class":"fog_volume","background":0.0,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_tiles":[{
                    "minimum":[-8,-8,-8],"maximum":[-1,-1,-1],"value":0.5
                }]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto tileOnlyResult = houio::HomManifest::read(tileOnlyPath);
        const auto tileOnlyVdb = tileOnlyResult
            ? std::dynamic_pointer_cast<houio::HouGeo::HouSparseVdb>(
                tileOnlyResult.value->primitives().front())
            : nullptr;
        if( !tileOnlyVdb || tileOnlyVdb->sparseGrid().activeVoxelCount() != 0
            || tileOnlyVdb->sparseGrid().activeTileCount() != 1
            || tileOnlyVdb->sparseGrid().value(houio::math::V3i(-4, -4, -4)) != 0.5f )
        {
            return fail("tile-only sparse VDB manifest failed to load");
        }

#if HOUIO_HAS_OPENVDB
        const std::filesystem::path outputPath = directory / "sparse_float_vdb.bgeo";
        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(result.value));
        if( !writeResult )
            return fail("sparse VDB manifest failed native writer serialization");
        const auto writtenGeometry = houio::GeometryIO::readHouGeo(outputPath);
        if( !writtenGeometry || writtenGeometry.value->primitiveCount() != 1 )
            return fail("sparse VDB manifest output failed to load");
        const auto nativeVdb = std::dynamic_pointer_cast<houio::HouGeo::HouVdb>(
            writtenGeometry.value->primitives().front());
        if( !nativeVdb || !nativeVdb->serializedPayload() )
            return fail("sparse VDB manifest output has no native payload");
        const auto stream = houio::NativeVdbPayload::decode(nativeVdb->serializedPayload());
        const auto decoded = stream
            ? houio::OpenVdbBackend::decodeFloatGrid(stream.value, "density")
            : houio::GeometryReadResult<houio::SparseFloatGrid>{};
        if( !decoded || decoded.value.activeTileCount() == 0
            || decoded.value.value(houio::math::V3i(-4, -4, -4)) != 0.25f
            || decoded.value.value(houio::math::V3i(1, 1, 1)) != 1.0f )
        {
            return fail("sparse VDB manifest native round-trip changed tile data");
        }
#endif
        return 0;
    }

    int verifySparseInt32VdbManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "sparse_int32_vdb.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_int32_vdb","vertex_offset":0,
                "name":"labels","grid_class":"unknown","background":-1,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[10,10,10,-1,-2,-3],
                "active_values":[42,-9],
                "active_tiles":[{
                    "minimum":[8,8,8],"maximum":[15,15,15],"value":7
                }],
                "metadata":{"creator":"houio_int_test"}
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 1 )
            return fail("sparse Int32 VDB manifest failed to load");
        const auto sparseVdb =
            std::dynamic_pointer_cast<houio::HouGeo::HouSparseInt32Vdb>(
                result.value->primitives().front());
        if( !sparseVdb || sparseVdb->topologyVertex() != 0 )
            return fail("sparse Int32 VDB manifest did not create the expected primitive");
        const houio::SparseInt32Grid& grid = sparseVdb->sparseGrid();
        if( grid.name() != "labels"
            || grid.background() != -1
            || grid.activeVoxelCount() != 2
            || grid.activeTileCount() != 1
            || grid.value(houio::math::V3i(9, 9, 9)) != 7
            || grid.value(houio::math::V3i(10, 10, 10)) != 42
            || grid.value(houio::math::V3i(-1, -2, -3)) != -9
            || grid.metadata("creator")
                != std::optional<std::string>("houio_int_test") )
        {
            return fail("sparse Int32 VDB manifest changed grid data");
        }

        const std::filesystem::path outputPath = directory / "sparse_int32_vdb.bgeo";
        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(result.value));
#if HOUIO_HAS_OPENVDB
        if( !writeResult )
            return fail("sparse Int32 VDB manifest failed native writer serialization");
        const auto writtenGeometry = houio::GeometryIO::readHouGeo(outputPath);
        const auto nativeVdb = writtenGeometry && writtenGeometry.value->primitiveCount() == 1
            ? std::dynamic_pointer_cast<houio::HouGeo::HouVdb>(
                writtenGeometry.value->primitives().front())
            : houio::HouGeo::HouVdb::Ptr{};
        const auto stream = nativeVdb && nativeVdb->serializedPayload()
            ? houio::NativeVdbPayload::decode(nativeVdb->serializedPayload())
            : houio::GeometryReadResult<std::vector<houio::ubyte>>{};
        const auto decoded = stream
            ? houio::OpenVdbBackend::decodeInt32Grid(stream.value, "labels")
            : houio::GeometryReadResult<houio::SparseInt32Grid>{};
        if( !decoded
            || decoded.value.value(houio::math::V3i(9, 9, 9)) != 7
            || decoded.value.value(houio::math::V3i(10, 10, 10)) != 42 )
        {
            return fail("sparse Int32 VDB manifest native round-trip changed data");
        }
#else
        if( writeResult || writeResult.diagnostics.empty()
            || writeResult.diagnostics.front().category
                != houio::DiagnosticCategory::unsupported_input
            || std::filesystem::exists(outputPath) )
        {
            return fail("backend-off sparse Int32 VDB write did not fail cleanly");
        }
#endif
        return 0;
    }

    int verifySparseVec3fVdbManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "sparse_vec3f_vdb.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_vec3f_vdb","vertex_offset":0,
                "name":"velocity","grid_class":"staggered",
                "vector_type":"contravariant_relative",
                "background":[0,0,0],
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[18,18,18,-1,-2,-3],
                "active_values":[[-1,4,0.5],[2,-3,1]],
                "active_tiles":[{
                    "minimum":[16,16,16],"maximum":[23,23,23],
                    "value":[1,2,3]
                }],
                "metadata":{"creator":"houio_vec_test"}
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 1 )
            return fail("sparse Vec3f VDB manifest failed to load");
        const auto sparseVdb =
            std::dynamic_pointer_cast<houio::HouGeo::HouSparseVec3fVdb>(
                result.value->primitives().front());
        if( !sparseVdb || sparseVdb->topologyVertex() != 0 )
            return fail("sparse Vec3f VDB manifest did not create the expected primitive");
        const houio::SparseVec3fGrid& grid = sparseVdb->sparseGrid();
        if( grid.name() != "velocity"
            || grid.gridClass() != houio::SparseGridClass::staggered
            || grid.vectorType() != houio::SparseVectorType::contravariant_relative
            || grid.background() != houio::math::V3f(0.0f)
            || grid.activeVoxelCount() != 2
            || grid.activeTileCount() != 1
            || grid.value(houio::math::V3i(17, 17, 17))
                != houio::math::V3f(1.0f, 2.0f, 3.0f)
            || grid.value(houio::math::V3i(18, 18, 18))
                != houio::math::V3f(-1.0f, 4.0f, 0.5f)
            || grid.metadata("creator")
                != std::optional<std::string>("houio_vec_test") )
        {
            return fail("sparse Vec3f VDB manifest changed grid data or semantics");
        }

        const std::filesystem::path outputPath = directory / "sparse_vec3f_vdb.bgeo";
        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(result.value));
#if HOUIO_HAS_OPENVDB
        if( !writeResult )
            return fail("sparse Vec3f VDB manifest failed native writer serialization");
        const auto writtenGeometry = houio::GeometryIO::readHouGeo(outputPath);
        const auto nativeVdb = writtenGeometry && writtenGeometry.value->primitiveCount() == 1
            ? std::dynamic_pointer_cast<houio::HouGeo::HouVdb>(
                writtenGeometry.value->primitives().front())
            : houio::HouGeo::HouVdb::Ptr{};
        const auto stream = nativeVdb && nativeVdb->serializedPayload()
            ? houio::NativeVdbPayload::decode(nativeVdb->serializedPayload())
            : houio::GeometryReadResult<std::vector<houio::ubyte>>{};
        const auto decoded = stream
            ? houio::OpenVdbBackend::decodeVec3fGrid(stream.value, "velocity")
            : houio::GeometryReadResult<houio::SparseVec3fGrid>{};
        if( !decoded
            || decoded.value.gridClass() != houio::SparseGridClass::staggered
            || decoded.value.vectorType()
                != houio::SparseVectorType::contravariant_relative
            || decoded.value.value(houio::math::V3i(17, 17, 17))
                != houio::math::V3f(1.0f, 2.0f, 3.0f)
            || decoded.value.value(houio::math::V3i(18, 18, 18))
                != houio::math::V3f(-1.0f, 4.0f, 0.5f) )
        {
            return fail("sparse Vec3f VDB manifest native round-trip changed data");
        }
#else
        if( writeResult || writeResult.diagnostics.empty()
            || writeResult.diagnostics.front().category
                != houio::DiagnosticCategory::unsupported_input
            || std::filesystem::exists(outputPath) )
        {
            return fail("backend-off sparse Vec3f VDB write did not fail cleanly");
        }
#endif
        return 0;
    }

    int verifyInvalidSparseVec3fVdbManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "invalid_sparse_vec3f_vdb.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_vec3f_vdb","vertex_offset":0,
                "grid_class":"staggered","vector_type":"invariant",
                "background":[0,0,0],
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[1,2,3],"active_values":[[1,2]]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto tupleResult = houio::HomManifest::read(path);
        if( tupleResult
            || !containsCategory(tupleResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("invalid sparse Vec3f VDB tuple was accepted");
        }

        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_vec3f_vdb","vertex_offset":0,
                "grid_class":"staggered","vector_type":"directionish",
                "background":[0,0,0],
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto semanticsResult = houio::HomManifest::read(path);
        if( semanticsResult
            || !containsCategory(
                semanticsResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("invalid sparse Vec3f VDB vector semantics were accepted");
        }
        return 0;
    }

    int verifyInvalidSparseInt32VdbManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "invalid_sparse_int32_vdb.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_int32_vdb","vertex_offset":0,
                "grid_class":"unknown","background":0,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[1,2,3,4],"active_values":[7]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( result
            || !containsCategory(result.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("invalid sparse Int32 VDB manifest was accepted");
        }
        return 0;
    }

    int verifyInvalidSparseVdbManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "invalid_sparse_float_vdb.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_float_vdb","vertex_offset":0,
                "grid_class":"fog_volume","background":0.0,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[1,2,3,4],"active_values":[1.0]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( result || !containsCategory(result.diagnostics, houio::DiagnosticCategory::schema) )
            return fail("invalid sparse VDB manifest did not return a schema diagnostic");

        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_float_vdb","vertex_offset":0,
                "grid_class":"fog_volume","background":0.0,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[1,2,3]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto unpairedActivityResult = houio::HomManifest::read(path);
        if( unpairedActivityResult
            || !containsCategory(
                unpairedActivityResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("unpaired sparse VDB activity arrays were not rejected");
        }

        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_float_vdb","vertex_offset":0,
                "grid_class":"fog_volume","background":0.0,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[1,2,3,1,2,3],"active_values":[1.0,2.0]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto duplicateResult = houio::HomManifest::read(path);
        if( duplicateResult
            || !containsCategory(
                duplicateResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("duplicate sparse VDB coordinates were not rejected");
        }

        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_float_vdb","vertex_offset":0,
                "grid_class":"fog_volume","background":0.0,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[],"active_values":[],
                "active_tiles":[{
                    "minimum":[1,0,0],"maximum":[0,0,0],"value":1.0
                }]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto unorderedTileResult = houio::HomManifest::read(path);
        if( unorderedTileResult
            || !containsCategory(
                unorderedTileResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("unordered sparse VDB tile bounds were not rejected");
        }

        writeText(path, R"JSON({
            "schema":"houio.hom/1",
            "point_count":1,"vertex_count":1,"primitive_count":1,
            "topology":[0],
            "primitives":[{
                "type":"sparse_float_vdb","vertex_offset":0,
                "grid_class":"fog_volume","background":0.0,
                "index_to_world":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],
                "active_indices":[],"active_values":[],
                "active_tiles":[
                    {"minimum":[-8,-8,-8],"maximum":[-1,-1,-1],"value":1.0},
                    {"minimum":[-8,-8,-8],"maximum":[-1,-1,-1],"value":2.0}
                ]
            }],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto duplicateTileResult = houio::HomManifest::read(path);
        if( duplicateTileResult
            || !containsCategory(
                duplicateTileResult.diagnostics, houio::DiagnosticCategory::schema) )
        {
            return fail("duplicate sparse VDB tile bounds were not rejected");
        }
        return 0;
    }

    int verifyCurveManifest(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "curves.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1","point_count":12,"vertex_count":12,
            "primitive_count":2,"topology":[0,1,2,3,4,5,6,7,8,9,10,11],
            "primitives":[
                {"type":"nurbs_curve","vertex_offset":0,"vertex_count":5,
                 "closed":false,"order":4,"end_interpolation":true,
                 "knots":[0,0,0,0,1,2,2,2,2]},
                {"type":"bezier_curve","vertex_offset":5,"vertex_count":7,
                 "closed":false,"order":4,"knots":[0,1,2]}
            ],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":12,
            "values":[0,0,0,1,1,0,0,1,2,0,0,1,3,0,0,1,4,0,0,1,
                      0,2,0,1,1,2,0,1,2,2,0,1,3,2,0,1,4,2,0,1,5,2,0,1,6,2,0,1]}],
            "vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 2 )
            return fail("curve manifest failed to load");
        const auto primitives = result.value->primitives();
        const auto nurbs = std::dynamic_pointer_cast<houio::HouGeo::HouCurve>(primitives[0]);
        const auto bezier = std::dynamic_pointer_cast<houio::HouGeo::HouCurve>(primitives[1]);
        if( !nurbs || !bezier
            || nurbs->basis() != houio::HouGeoAdapter::CurvePrimitive::Basis::nurbs
            || nurbs->order() != 4 || nurbs->vertexIndices().size() != 5
            || bezier->basis() != houio::HouGeoAdapter::CurvePrimitive::Basis::bezier
            || bezier->order() != 4 || bezier->vertexIndices().size() != 7 )
        {
            return fail("curve manifest changed basis metadata or topology");
        }

        const std::filesystem::path outputPath = directory / "curves.bgeo";
        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(result.value));
        const auto roundTrip = writeResult
            ? houio::GeometryIO::readHouGeo(outputPath)
            : houio::GeometryReadResult<houio::HouGeo::Ptr>{};
        if( !roundTrip || roundTrip.value->primitiveCount() != 2 )
            return fail("curve manifest failed writer round-trip");

        const auto roundTripPrimitives = roundTrip.value->primitives();
        const auto roundTripNurbs =
            std::dynamic_pointer_cast<houio::HouGeo::HouCurve>(roundTripPrimitives[0]);
        const auto roundTripBezier =
            std::dynamic_pointer_cast<houio::HouGeo::HouCurve>(roundTripPrimitives[1]);
        const std::vector<int> expectedNurbsVertices{0, 1, 2, 3, 4};
        const std::vector<houio::real64> expectedNurbsKnots{
            0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 2.0, 2.0, 2.0};
        const std::vector<int> expectedBezierVertices{5, 6, 7, 8, 9, 10, 11};
        const std::vector<houio::real64> expectedBezierKnots{0.0, 1.0, 2.0};
        if( !roundTripNurbs || !roundTripBezier
            || roundTripNurbs->basis()
                != houio::HouGeoAdapter::CurvePrimitive::Basis::nurbs
            || roundTripNurbs->order() != 4 || roundTripNurbs->isClosed()
            || !roundTripNurbs->endInterpolation()
            || !std::equal(
                roundTripNurbs->vertexIndices().begin(),
                roundTripNurbs->vertexIndices().end(),
                expectedNurbsVertices.begin(),
                expectedNurbsVertices.end())
            || !std::equal(
                roundTripNurbs->knots().begin(),
                roundTripNurbs->knots().end(),
                expectedNurbsKnots.begin(),
                expectedNurbsKnots.end())
            || roundTripBezier->basis()
                != houio::HouGeoAdapter::CurvePrimitive::Basis::bezier
            || roundTripBezier->order() != 4 || roundTripBezier->isClosed()
            || !std::equal(
                roundTripBezier->vertexIndices().begin(),
                roundTripBezier->vertexIndices().end(),
                expectedBezierVertices.begin(),
                expectedBezierVertices.end())
            || !std::equal(
                roundTripBezier->knots().begin(),
                roundTripBezier->knots().end(),
                expectedBezierKnots.begin(),
                expectedBezierKnots.end()) )
        {
            return fail("curve manifest writer round-trip changed curve metadata");
        }
        return 0;
    }

    int verifyQuadricManifest(const std::filesystem::path& directory)
    {
        const std::filesystem::path path = directory / "quadrics.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1","point_count":2,"vertex_count":2,
            "primitive_count":2,"topology":[0,1],
            "primitives":[
                {"type":"sphere","vertex_offset":0,
                 "transform":[2,0,0,0,3,0,0,0,4]},
                {"type":"tube","vertex_offset":1,
                 "transform":[1,0,0,0,5,0,0,0,2],
                 "caps":true,"taper":0.5}
            ],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":2,
            "values":[1,2,3,1,-1,0.5,2,1]}],
            "vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( !result || result.value->primitiveCount() != 2 )
            return fail("quadric manifest failed to load");
        const auto primitives = result.value->primitives();
        const auto sphere = std::dynamic_pointer_cast<houio::HouGeo::HouSphere>(primitives[0]);
        const auto tube = std::dynamic_pointer_cast<houio::HouGeo::HouTube>(primitives[1]);
        if( !sphere || !tube || sphere->topologyVertex() != 0
            || tube->topologyVertex() != 1 || !tube->hasCaps()
            || tube->taper() != 0.5f || sphere->transform().ma[0] != 2.0f
            || sphere->transform().ma[4] != 3.0f || sphere->transform().ma[8] != 4.0f
            || tube->transform().ma[4] != 5.0f || tube->transform().ma[8] != 2.0f )
        {
            return fail("quadric manifest changed native metadata");
        }

        const std::filesystem::path outputPath = directory / "quadrics.bgeo";
        const houio::WriteResult writeResult = houio::Writer::write(
            outputPath,
            std::static_pointer_cast<houio::HouGeoAdapter>(result.value));
        const auto roundTrip = writeResult
            ? houio::GeometryIO::readHouGeo(outputPath)
            : houio::GeometryReadResult<houio::HouGeo::Ptr>{};
        if( !roundTrip || roundTrip.value->primitiveCount() != 2 )
            return fail("quadric manifest failed writer round-trip");
        const auto roundTripPrimitives = roundTrip.value->primitives();
        const auto roundTripSphere =
            std::dynamic_pointer_cast<houio::HouGeo::HouSphere>(roundTripPrimitives[0]);
        const auto roundTripTube =
            std::dynamic_pointer_cast<houio::HouGeo::HouTube>(roundTripPrimitives[1]);
        if( !roundTripSphere || !roundTripTube || roundTripSphere->topologyVertex() != 0
            || roundTripTube->topologyVertex() != 1 || !roundTripTube->hasCaps()
            || roundTripTube->taper() != 0.5f
            || roundTripSphere->transform().ma[8] != 4.0f
            || roundTripTube->transform().ma[4] != 5.0f )
        {
            return fail("quadric manifest writer round-trip changed metadata");
        }
        return 0;
    }

    int verifyUnsupportedRecord(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "unsupported_tetrahedron.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1","point_count":1,"vertex_count":1,
            "primitive_count":1,"topology":[0],
            "primitives":[{"type":"tetrahedron","vertex_offset":0}],
            "attributes":{"point":[{"name":"P","kind":"numeric",
            "storage":"float32","tuple_size":4,"element_count":1,
            "values":[0,0,0,1]}],"vertex":[],"primitive":[],"global":[]}
        })JSON");
        const auto result = houio::HomManifest::read(path);
        if( result
            || !containsCategory(result.diagnostics, houio::DiagnosticCategory::unsupported_input) )
        {
            return fail("unsupported manifest record did not return an explicit diagnostic");
        }
        return 0;
    }
}

int main()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "houio_hom_manifest_test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    if( error )
        return fail("could not create HOM manifest test directory");

    if( const int result = verifyMixedManifest(directory); result != 0 )
        return result;
    if( const int result = verifyInvalidTopologyOffsets(directory); result != 0 )
        return result;
    if( const int result = verifyPackedFragmentManifest(directory); result != 0 )
        return result;
    if( const int result = verifyPackedDiskManifest(directory); result != 0 )
        return result;
    if( const int result = verifyPackedDiskSequenceManifest(directory); result != 0 )
        return result;
    if( const int result = verifySparseVdbManifest(directory); result != 0 )
        return result;
    if( const int result = verifySparseInt32VdbManifest(directory); result != 0 )
        return result;
    if( const int result = verifySparseVec3fVdbManifest(directory); result != 0 )
        return result;
    if( const int result = verifyInvalidSparseVec3fVdbManifest(directory); result != 0 )
        return result;
    if( const int result = verifyInvalidSparseInt32VdbManifest(directory); result != 0 )
        return result;
    if( const int result = verifyInvalidSparseVdbManifest(directory); result != 0 )
        return result;
    if( const int result = verifyCurveManifest(directory); result != 0 )
        return result;
    if( const int result = verifyQuadricManifest(directory); result != 0 )
        return result;
    if( const int result = verifyUnsupportedRecord(directory); result != 0 )
        return result;

    std::filesystem::remove_all(directory, error);
    return 0;
}
