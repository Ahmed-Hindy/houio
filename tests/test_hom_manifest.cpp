#include <houio/HomManifest.h>
#include <houio/Writer.h>

#include "TestSupport.h"

#include <filesystem>
#include <fstream>
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
                        "name":"label","kind":"string","tuple_size":1,
                        "element_count":4,"values":["a","b","c","volume"]
                    }
                ],
                "vertex":[
                    {
                        "name":"uv","kind":"numeric","storage":"float32",
                        "tuple_size":2,"element_count":4,
                        "values":[0,0,1,0,0,1,0.5,0.5]
                    }
                ],
                "primitive":[
                    {
                        "name":"kind","kind":"string","tuple_size":1,
                        "element_count":2,"values":["polygon","volume"]
                    }
                ],
                "global":[
                    {
                        "name":"asset","kind":"string","tuple_size":1,
                        "element_count":1,"values":["manifest_test"]
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
            || !roundtrip.value->vertexAttribute("uv") )
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
            || grid.value(houio::math::V3i(1, 1, 1)) != 1.0f
            || grid.value(houio::math::V3i(2, 1, 1)) != 2.0f
            || grid.metadata("creator") != std::optional<std::string>("houio_test") )
        {
            return fail("sparse VDB manifest changed grid data");
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
        return 0;
    }

    int verifyUnsupportedRecord(const std::filesystem::path &directory)
    {
        const std::filesystem::path path = directory / "unsupported_curve.json";
        writeText(path, R"JSON({
            "schema":"houio.hom/1","point_count":1,"vertex_count":1,
            "primitive_count":1,"topology":[0],
            "primitives":[{"type":"nurbs_curve","vertex_offset":0}],
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
    if( const int result = verifyInvalidSparseVdbManifest(directory); result != 0 )
        return result;
    if( const int result = verifyUnsupportedRecord(directory); result != 0 )
        return result;

    std::filesystem::remove_all(directory, error);
    return 0;
}
