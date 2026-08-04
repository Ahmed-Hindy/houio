#include <houio/HouGeo.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace houio
{
    namespace
    {
        int checkedArrayCount(
            const json::ArrayPtr& array,
            const std::string& description)
        {
            if( !array )
                throw std::runtime_error(description + " must be an array");
            const sint64 count = array->size();
            if( count < 0 )
                throw std::runtime_error(description + " has a negative element count");
            if( count > static_cast<sint64>(std::numeric_limits<int>::max()) )
                throw std::length_error(description + " exceeds supported indexing");
            return static_cast<int>(count);
        }
    }

    void HouGeo::loadPackedGeometryPrimitive(
        json::ObjectPtr packedGeometry,
        SharedPrimitiveData& sharedPrimitiveData)
    {
        if( !packedGeometry )
            throw std::invalid_argument(
                "HouGeo::loadPackedGeometryPrimitive received null data");
        json::ObjectPtr parameters = packedGeometry->object("parameters");
        if( !parameters )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedGeometry primitive is missing parameters",
                -1,
                "parameters"});
        const std::string embeddedId = parameters->get<std::string>("embedded", "");
        if( embeddedId.empty() )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedGeometry primitive is missing embedded geometry id",
                -1,
                "parameters.embedded"});
        const auto embedded = sharedPrimitiveData.sharedEmbeddedGeometry.find(embeddedId);
        if( embedded == sharedPrimitiveData.sharedEmbeddedGeometry.end() )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedGeometry embedded geometry was not found",
                -1,
                "parameters.embedded"});

        const int topologyVertex = packedGeometry->get<int>("vertex", -1);
        if( topologyVertex < 0 || !m_topology
            || static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedGeometry topology vertex is outside vertexcount",
                -1,
                "vertex"});
        }

        math::V3f pivot(0.0f);
        json::ArrayPtr pivotValues = packedGeometry->array("pivot");
        if( pivotValues )
        {
            if( pivotValues->size() != 3 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedGeometry pivot requires three values",
                    -1,
                    "pivot"});
            pivot = math::V3f(
                pivotValues->get<real32>(0),
                pivotValues->get<real32>(1),
                pivotValues->get<real32>(2));
        }

        math::M33f transform = math::M33f::identity();
        json::ArrayPtr transformValues = packedGeometry->array("transform");
        if( transformValues )
        {
            if( transformValues->size() != 9 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedGeometry transform requires nine values",
                    -1,
                    "transform"});
            transform = math::M33f(
                transformValues->get<real32>(0),
                transformValues->get<real32>(1),
                transformValues->get<real32>(2),
                transformValues->get<real32>(3),
                transformValues->get<real32>(4),
                transformValues->get<real32>(5),
                transformValues->get<real32>(6),
                transformValues->get<real32>(7),
                transformValues->get<real32>(8));
        }

        auto result = std::make_shared<HouPackedGeometry>();
        result->embedded_geometry_ = embedded->second;
        result->topology_vertex_ = topologyVertex;
        result->pivot_ = pivot;
        result->transform_ = transform;
        result->viewport_lod_ = packedGeometry->get<std::string>("viewportlod", "full");
        result->point_instance_transform_ =
            parameters->get<int>("pointinstancetransform", 0) != 0;
        result->treat_as_folder_ = parameters->get<int>("treatasfolder", 0) != 0;
        m_primitives.push_back(std::move(result));
    }

    void HouGeo::loadPackedFragmentPrimitive(
        json::ObjectPtr packedFragment,
        SharedPrimitiveData& sharedPrimitiveData)
    {
        if( !packedFragment )
            throw std::invalid_argument(
                "HouGeo::loadPackedFragmentPrimitive received null data");
        json::ObjectPtr parameters = packedFragment->object("parameters");
        if( !parameters )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedFragment primitive is missing parameters",
                -1,
                "parameters"});

        const std::string embeddedId = parameters->get<std::string>("embedded", "");
        if( embeddedId.empty() )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedFragment primitive is missing embedded geometry id",
                -1,
                "parameters.embedded"});
        const auto embedded = sharedPrimitiveData.sharedEmbeddedGeometry.find(embeddedId);
        if( embedded == sharedPrimitiveData.sharedEmbeddedGeometry.end() )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedFragment embedded geometry was not found",
                -1,
                "parameters.embedded"});

        const std::string fragmentAttribute = parameters->get<std::string>("attribute", "");
        if( fragmentAttribute.empty() )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedFragment primitive is missing its fragment attribute",
                -1,
                "parameters.attribute"});
        const std::string fragmentName = parameters->get<std::string>("name", "");
        if( fragmentName.empty() )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedFragment primitive is missing its fragment name",
                -1,
                "parameters.name"});

        const int topologyVertex = packedFragment->get<int>("vertex", -1);
        if( topologyVertex < 0 || !m_topology
            || static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedFragment topology vertex is outside vertexcount",
                -1,
                "vertex"});
        }

        const auto parseBounds = [&](const std::string& key,
            const HouGeoAdapter::PackedFragmentPrimitive::Bounds* fallback = nullptr)
        {
            json::ArrayPtr values = parameters->array(key);
            if( !values )
            {
                if( fallback )
                    return *fallback;
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedFragment " + key + " requires six values",
                    -1,
                    "parameters." + key});
            }
            if( values->size() != 6 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedFragment " + key + " requires six values",
                    -1,
                    "parameters." + key});
            HouGeoAdapter::PackedFragmentPrimitive::Bounds result{};
            for( int index = 0; index < 6; ++index )
                result[static_cast<std::size_t>(index)] = values->get<real32>(index);
            return result;
        };

        const HouGeoAdapter::PackedFragmentPrimitive::Bounds bounds = parseBounds("bounds");
        const HouGeoAdapter::PackedFragmentPrimitive::Bounds cachedBounds =
            parseBounds("cachedbounds", &bounds);

        math::V3f pivot(0.0f);
        json::ArrayPtr pivotValues = packedFragment->array("pivot");
        if( pivotValues )
        {
            if( pivotValues->size() != 3 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedFragment pivot requires three values",
                    -1,
                    "pivot"});
            pivot = math::V3f(
                pivotValues->get<real32>(0),
                pivotValues->get<real32>(1),
                pivotValues->get<real32>(2));
        }

        math::M33f transform = math::M33f::identity();
        json::ArrayPtr transformValues = packedFragment->array("transform");
        if( transformValues )
        {
            if( transformValues->size() != 9 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedFragment transform requires nine values",
                    -1,
                    "transform"});
            transform = math::M33f(
                transformValues->get<real32>(0),
                transformValues->get<real32>(1),
                transformValues->get<real32>(2),
                transformValues->get<real32>(3),
                transformValues->get<real32>(4),
                transformValues->get<real32>(5),
                transformValues->get<real32>(6),
                transformValues->get<real32>(7),
                transformValues->get<real32>(8));
        }

        auto result = std::make_shared<HouPackedFragment>();
        result->embedded_geometry_ = embedded->second;
        result->topology_vertex_ = topologyVertex;
        result->pivot_ = pivot;
        result->transform_ = transform;
        result->viewport_lod_ = packedFragment->get<std::string>("viewportlod", "full");
        result->point_instance_transform_ =
            parameters->get<int>("pointinstancetransform", 0) != 0;
        result->fragment_attribute_ = fragmentAttribute;
        result->fragment_name_ = fragmentName;
        result->bounds_ = bounds;
        result->cached_bounds_ = cachedBounds;
        m_primitives.push_back(std::move(result));
    }

    void HouGeo::loadPackedDiskPrimitive(json::ObjectPtr packedDisk)
    {
        if( !packedDisk )
            throw std::invalid_argument(
                "HouGeo::loadPackedDiskPrimitive received null data");
        json::ObjectPtr parameters = packedDisk->object("parameters");
        if( !parameters )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedDisk primitive is missing parameters",
                -1,
                "parameters"});

        const std::string filename = parameters->get<std::string>("filename", "");
        if( filename.empty() )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedDisk primitive is missing its filename",
                -1,
                "parameters.filename"});

        const int topologyVertex = packedDisk->get<int>("vertex", -1);
        if( topologyVertex < 0 || !m_topology
            || static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedDisk topology vertex is outside vertexcount",
                -1,
                "vertex"});
        }

        math::V3f pivot(0.0f);
        json::ArrayPtr pivotValues = packedDisk->array("pivot");
        if( pivotValues )
        {
            if( pivotValues->size() != 3 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedDisk pivot requires three values",
                    -1,
                    "pivot"});
            pivot = math::V3f(
                pivotValues->get<real32>(0),
                pivotValues->get<real32>(1),
                pivotValues->get<real32>(2));
        }

        math::M33f transform = math::M33f::identity();
        json::ArrayPtr transformValues = packedDisk->array("transform");
        if( transformValues )
        {
            if( transformValues->size() != 9 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedDisk transform requires nine values",
                    -1,
                    "transform"});
            transform = math::M33f(
                transformValues->get<real32>(0),
                transformValues->get<real32>(1),
                transformValues->get<real32>(2),
                transformValues->get<real32>(3),
                transformValues->get<real32>(4),
                transformValues->get<real32>(5),
                transformValues->get<real32>(6),
                transformValues->get<real32>(7),
                transformValues->get<real32>(8));
        }

        auto result = std::make_shared<HouPackedDisk>();
        result->topology_vertex_ = topologyVertex;
        result->filename_ = filename;
        result->expand_frame_ = parameters->get<real32>("expandframe", 1.0f);
        result->expand_filename_ = parameters->get<int>("expandfilename", 0) != 0;
        result->pivot_ = pivot;
        result->transform_ = transform;
        result->viewport_lod_ = packedDisk->get<std::string>("viewportlod", "full");
        result->point_instance_transform_ =
            parameters->get<int>("pointinstancetransform", 0) != 0;
        result->treat_as_folder_ = parameters->get<int>("treatasfolder", 0) != 0;
        m_primitives.push_back(std::move(result));
    }

    void HouGeo::loadPackedDiskSequencePrimitive(
        json::ObjectPtr packedDiskSequence)
    {
        if( !packedDiskSequence )
            throw std::invalid_argument(
                "HouGeo::loadPackedDiskSequencePrimitive received null data");
        json::ObjectPtr parameters = packedDiskSequence->object("parameters");
        if( !parameters )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedDiskSequence primitive is missing parameters",
                -1,
                "parameters"});

        json::ArrayPtr filenameValues = parameters->array("filenames");
        if( !filenameValues || filenameValues->size() <= 0 )
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedDiskSequence requires at least one filename",
                -1,
                "parameters.filenames"});
        const int filenameCount = checkedArrayCount(
            filenameValues, "PackedDiskSequence filenames");
        std::vector<std::string> filenames;
        filenames.reserve(static_cast<std::size_t>(filenameCount));
        for( int index = 0; index < filenameCount; ++index )
        {
            const std::string filename = filenameValues->get<std::string>(index);
            if( filename.empty() )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedDiskSequence filename cannot be empty",
                    -1,
                    "parameters.filenames[" + std::to_string(index) + "]"});
            filenames.push_back(filename);
        }

        const int topologyVertex = packedDiskSequence->get<int>("vertex", -1);
        if( topologyVertex < 0 || !m_topology
            || static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedDiskSequence topology vertex is outside vertexcount",
                -1,
                "vertex"});
        }

        math::V3f pivot(0.0f);
        json::ArrayPtr pivotValues = packedDiskSequence->array("pivot");
        if( pivotValues )
        {
            if( pivotValues->size() != 3 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedDiskSequence pivot requires three values",
                    -1,
                    "pivot"});
            pivot = math::V3f(
                pivotValues->get<real32>(0),
                pivotValues->get<real32>(1),
                pivotValues->get<real32>(2));
        }

        math::M33f transform = math::M33f::identity();
        json::ArrayPtr transformValues = packedDiskSequence->array("transform");
        if( transformValues )
        {
            if( transformValues->size() != 9 )
                throw DiagnosticException(Diagnostic{
                    DiagnosticSeverity::error,
                    DiagnosticCategory::schema,
                    "PackedDiskSequence transform requires nine values",
                    -1,
                    "transform"});
            transform = math::M33f(
                transformValues->get<real32>(0),
                transformValues->get<real32>(1),
                transformValues->get<real32>(2),
                transformValues->get<real32>(3),
                transformValues->get<real32>(4),
                transformValues->get<real32>(5),
                transformValues->get<real32>(6),
                transformValues->get<real32>(7),
                transformValues->get<real32>(8));
        }

        const std::string wrap = parameters->get<std::string>("wrap", "cycle");
        PackedDiskSequencePrimitive::WrapMode wrapMode;
        if( wrap == "cycle" )
            wrapMode = PackedDiskSequencePrimitive::WrapMode::cycle;
        else if( wrap == "clamp" )
            wrapMode = PackedDiskSequencePrimitive::WrapMode::clamp;
        else if( wrap == "strict" )
            wrapMode = PackedDiskSequencePrimitive::WrapMode::strict;
        else if( wrap == "mirror" )
            wrapMode = PackedDiskSequencePrimitive::WrapMode::mirror;
        else
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                "PackedDiskSequence wrap mode is invalid",
                -1,
                "parameters.wrap"});

        auto result = std::make_shared<HouPackedDiskSequence>();
        result->topology_vertex_ = topologyVertex;
        result->filenames_ = std::move(filenames);
        result->index_ = parameters->get<real32>("index", 0.0f);
        result->wrap_mode_ = wrapMode;
        result->pivot_ = pivot;
        result->transform_ = transform;
        result->viewport_lod_ =
            packedDiskSequence->get<std::string>("viewportlod", "full");
        result->point_instance_transform_ =
            parameters->get<int>("pointinstancetransform", 0) != 0;
        m_primitives.push_back(std::move(result));
    }
}
