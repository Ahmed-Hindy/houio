#include <houio/HomManifest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace houio
{
    namespace
    {
        [[noreturn]] void failManifest(
            DiagnosticCategory category,
            std::string message,
            std::string path)
        {
            throw DiagnosticException(Diagnostic{
                DiagnosticSeverity::error,
                category,
                std::move(message),
                -1,
                std::move(path)});
        }

        json::ObjectPtr requireObject(
            const json::ObjectPtr &parent,
            const std::string &key,
            const std::string &path)
        {
            if( !parent )
                failManifest(DiagnosticCategory::schema, "Manifest object is null", path);
            json::ObjectPtr value = parent->object(key);
            if( !value )
                failManifest(DiagnosticCategory::schema,
                    "Manifest requires object member " + key, path + "." + key);
            return value;
        }

        json::ArrayPtr requireArray(
            const json::ObjectPtr &parent,
            const std::string &key,
            const std::string &path)
        {
            if( !parent )
                failManifest(DiagnosticCategory::schema, "Manifest object is null", path);
            json::ArrayPtr value = parent->array(key);
            if( !value )
                failManifest(DiagnosticCategory::schema,
                    "Manifest requires array member " + key, path + "." + key);
            return value;
        }

        int checkedCount(sint64 value, const std::string &path)
        {
            if( value < 0 || value > static_cast<sint64>(std::numeric_limits<int>::max()) )
                failManifest(DiagnosticCategory::schema,
                    "Manifest count is outside the supported int range", path);
            return static_cast<int>(value);
        }

        std::size_t checkedSize(sint64 value, const std::string &path)
        {
            if( value < 0 )
                failManifest(DiagnosticCategory::schema,
                    "Manifest count cannot be negative", path);
            if( value > static_cast<sint64>(std::numeric_limits<int>::max()) )
                failManifest(DiagnosticCategory::schema,
                    "Manifest count is outside the supported int range", path);
            return static_cast<std::size_t>(value);
        }

        template<typename T>
        std::vector<T> scalarArray(
            const json::ArrayPtr &array,
            const std::string &path)
        {
            if( !array )
                failManifest(DiagnosticCategory::schema,
                    "Manifest scalar array is null", path);
            const std::size_t count = checkedSize(array->size(), path);
            std::vector<T> values;
            values.reserve(count);
            for( std::size_t index = 0; index < count; ++index )
            {
                try
                {
                    values.push_back(array->get<T>(static_cast<int>(index)));
                }
                catch( const std::exception &exception )
                {
                    failManifest(DiagnosticCategory::schema,
                        "Invalid scalar value: " + std::string(exception.what()),
                        path + "[" + std::to_string(index) + "]");
                }
            }
            return values;
        }

        template<typename T>
        Attribute::Ptr numericAttribute(
            const json::ArrayPtr &values,
            int tupleSize,
            int elementCount,
            Attribute::ComponentType componentType,
            const std::string &path)
        {
            const std::vector<T> scalars = scalarArray<T>(values, path + ".values");
            const std::size_t expected = static_cast<std::size_t>(tupleSize)
                * static_cast<std::size_t>(elementCount);
            if( scalars.size() != expected )
                failManifest(DiagnosticCategory::schema,
                    "Numeric attribute scalar count does not match tuple and element counts",
                    path + ".values");
            return Attribute::create(
                tupleSize,
                componentType,
                std::as_bytes(std::span<const T>(scalars)),
                elementCount);
        }

        HouGeo::HouAttribute::Ptr parseAttribute(
            const json::ObjectPtr &definition,
            int expectedElementCount,
            const std::string &path)
        {
            if( !definition )
                failManifest(DiagnosticCategory::schema,
                    "Attribute definition is null", path);
            const std::string name = definition->get<std::string>("name");
            if( name.empty() )
                failManifest(DiagnosticCategory::schema,
                    "Attribute name cannot be empty", path + ".name");
            const std::string kind = definition->get<std::string>("kind", "numeric");
            const int tupleSize = checkedCount(
                definition->get<sint64>("tuple_size", 1), path + ".tuple_size");
            if( tupleSize <= 0 )
                failManifest(DiagnosticCategory::schema,
                    "Attribute tuple size must be positive", path + ".tuple_size");
            const int elementCount = checkedCount(
                definition->get<sint64>("element_count", expectedElementCount),
                path + ".element_count");
            if( elementCount != expectedElementCount )
                failManifest(DiagnosticCategory::schema,
                    "Attribute element count does not match its domain",
                    path + ".element_count");

            auto attribute = std::make_shared<HouGeo::HouAttribute>();
            attribute->setName(name);
            attribute->setScope(definition->get<std::string>("scope", "public"));

            if( kind == "numeric" )
            {
                const std::string storageName = definition->get<std::string>("storage");
                std::string canonicalStorageName = storageName;
                if( storageName == "float16" )
                    canonicalStorageName = "fpreal16";
                else if( storageName == "float32" )
                    canonicalStorageName = "fpreal32";
                else if( storageName == "float64" )
                    canonicalStorageName = "fpreal64";
                const HouGeoAdapter::AttributeAdapter::Storage storage =
                    HouGeoAdapter::AttributeAdapter::parseStorage(canonicalStorageName);
                const json::ArrayPtr values = requireArray(definition, "values", path);
                Attribute::Ptr numeric;
                switch( storage )
                {
                case HouGeoAdapter::AttributeAdapter::Storage::float16:
                    numeric = numericAttribute<uword>(values, tupleSize, elementCount,
                        Attribute::ComponentType::float16, path);
                    break;
                case HouGeoAdapter::AttributeAdapter::Storage::float32:
                    numeric = numericAttribute<real32>(values, tupleSize, elementCount,
                        Attribute::ComponentType::float32, path);
                    break;
                case HouGeoAdapter::AttributeAdapter::Storage::float64:
                    numeric = numericAttribute<real64>(values, tupleSize, elementCount,
                        Attribute::ComponentType::float64, path);
                    break;
                case HouGeoAdapter::AttributeAdapter::Storage::int32:
                    numeric = numericAttribute<sint32>(values, tupleSize, elementCount,
                        Attribute::ComponentType::int32, path);
                    break;
                case HouGeoAdapter::AttributeAdapter::Storage::int64:
                    numeric = numericAttribute<sint64>(values, tupleSize, elementCount,
                        Attribute::ComponentType::int64, path);
                    break;
                default:
                    failManifest(DiagnosticCategory::unsupported_input,
                        "Manifest numeric storage is unsupported: " + storageName,
                        path + ".storage");
                }
                attribute->setNumericAttribute(std::move(numeric));
                attribute->setStorage(storage);
            }
            else if( kind == "string" )
            {
                const json::ArrayPtr values = requireArray(definition, "values", path);
                std::vector<std::string> strings = scalarArray<std::string>(
                    values, path + ".values");
                const std::size_t expected = static_cast<std::size_t>(tupleSize)
                    * static_cast<std::size_t>(elementCount);
                if( strings.size() != expected )
                    failManifest(DiagnosticCategory::schema,
                        "String attribute value count does not match tuple and element counts",
                        path + ".values");
                attribute->setStringValues(
                    std::move(strings),
                    HouGeoAdapter::AttributeAdapter::TupleSize(tupleSize));
            }
            else if( kind == "dictionary" )
            {
                if( tupleSize != 1 )
                    failManifest(DiagnosticCategory::unsupported_input,
                        "Dictionary tuple attributes are not supported by the manifest reader",
                        path + ".tuple_size");
                const json::ArrayPtr values = requireArray(definition, "values", path);
                if( values->size() != elementCount )
                    failManifest(DiagnosticCategory::schema,
                        "Dictionary attribute value count does not match its domain",
                        path + ".values");
                std::vector<json::ObjectPtr> dictionaries;
                dictionaries.reserve(static_cast<std::size_t>(elementCount));
                for( int index = 0; index < elementCount; ++index )
                {
                    json::ObjectPtr value = values->object(index);
                    if( !value )
                        failManifest(DiagnosticCategory::schema,
                            "Dictionary attribute value must be an object",
                            path + ".values[" + std::to_string(index) + "]");
                    dictionaries.push_back(std::move(value));
                }
                attribute->setDictionaryValues(std::move(dictionaries));
            }
            else
            {
                failManifest(DiagnosticCategory::unsupported_input,
                    "Manifest attribute kind is unsupported: " + kind,
                    path + ".kind");
            }
            return attribute;
        }

        void parseAttributeDomain(
            const json::ObjectPtr &attributes,
            const std::string &domain,
            int elementCount,
            const HouGeo::Ptr &geometry)
        {
            json::ArrayPtr definitions = attributes->array(domain);
            if( !definitions )
                return;
            const int count = checkedCount(definitions->size(),
                "attributes." + domain);
            for( int index = 0; index < count; ++index )
            {
                const std::string path = "attributes." + domain + "["
                    + std::to_string(index) + "]";
                HouGeo::HouAttribute::Ptr attribute = parseAttribute(
                    definitions->object(index), elementCount, path);
                if( domain == "point" )
                    geometry->setPointAttribute(std::move(attribute));
                else if( domain == "vertex" )
                    geometry->setVertexAttribute(std::move(attribute));
                else if( domain == "primitive" )
                {
                    const std::string name = attribute->name();
                    geometry->setPrimitiveAttribute(name, std::move(attribute));
                }
                else if( domain == "global" )
                    geometry->setGlobalAttribute(std::move(attribute));
                else
                    failManifest(DiagnosticCategory::schema,
                        "Unknown attribute domain: " + domain, path);
            }
        }

        std::vector<bool> parseMembership(
            const json::ArrayPtr &indices,
            int elementCount,
            const std::string &path)
        {
            std::vector<bool> membership(static_cast<std::size_t>(elementCount), false);
            if( !indices )
                failManifest(DiagnosticCategory::schema,
                    "Group membership must be an index array", path);
            const int count = checkedCount(indices->size(), path);
            for( int index = 0; index < count; ++index )
            {
                const int elementIndex = checkedCount(
                    indices->get<sint64>(index),
                    path + "[" + std::to_string(index) + "]");
                if( elementIndex >= elementCount )
                    failManifest(DiagnosticCategory::schema,
                        "Group index exceeds its domain",
                        path + "[" + std::to_string(index) + "]");
                membership[static_cast<std::size_t>(elementIndex)] = true;
            }
            return membership;
        }

        void parseGroups(
            const json::ObjectPtr &root,
            int pointCount,
            int vertexCount,
            int primitiveCount,
            const HouGeo::Ptr &geometry)
        {
            json::ObjectPtr groups = root->object("groups");
            if( !groups )
                return;
            for( const std::string &domain : {"point", "vertex", "primitive"} )
            {
                json::ObjectPtr domainGroups = groups->object(domain);
                if( !domainGroups )
                    continue;
                const int elementCount = domain == "point"
                    ? pointCount
                    : domain == "vertex" ? vertexCount : primitiveCount;
                for( const std::string &name : domainGroups->keys() )
                {
                    if( name.empty() )
                        failManifest(DiagnosticCategory::schema,
                            "Group name cannot be empty", "groups." + domain);
                    std::vector<bool> membership = parseMembership(
                        domainGroups->array(name), elementCount,
                        "groups." + domain + "." + name);
                    if( domain == "point" )
                        geometry->setPointGroup(name, membership);
                    else if( domain == "vertex" )
                        geometry->setVertexGroup(name, membership);
                    else
                        geometry->setPrimitiveGroup(name, membership);
                }
            }
        }

        math::M44f parseMatrix44(
            const json::ArrayPtr &values,
            const std::string &path)
        {
            const std::vector<real32> components = scalarArray<real32>(values, path);
            if( components.size() != 16 )
                failManifest(DiagnosticCategory::schema,
                    "Matrix44 requires exactly 16 row-major values", path);
            return math::M44f(
                components[0], components[1], components[2], components[3],
                components[4], components[5], components[6], components[7],
                components[8], components[9], components[10], components[11],
                components[12], components[13], components[14], components[15]);
        }

        HouGeo::Ptr parseGeometryRoot(const json::ObjectPtr &root);

        math::M33f parseMatrix33(
            const json::ArrayPtr &values,
            const std::string &path)
        {
            const std::vector<real32> components = scalarArray<real32>(values, path);
            if( components.size() != 9 )
                failManifest(DiagnosticCategory::schema,
                    "Matrix33 requires exactly nine row-major values", path);
            return math::M33f(
                components[0], components[1], components[2],
                components[3], components[4], components[5],
                components[6], components[7], components[8]);
        }

        math::V3f parseVector3(
            const json::ArrayPtr &values,
            const std::string &path)
        {
            const std::vector<real32> components = scalarArray<real32>(values, path);
            if( components.size() != 3 )
                failManifest(DiagnosticCategory::schema,
                    "Vector3 requires exactly three values", path);
            return math::V3f(components[0], components[1], components[2]);
        }

        math::V3f parseFiniteVector3(
            const json::ArrayPtr& values,
            const std::string& path)
        {
            const math::V3f result = parseVector3(values, path);
            if( !std::isfinite(result.x)
                || !std::isfinite(result.y)
                || !std::isfinite(result.z) )
            {
                failManifest(DiagnosticCategory::schema,
                    "Vector3 components must be finite", path);
            }
            return result;
        }

        SparseVectorType parseSparseVectorType(
            const std::string& value,
            const std::string& path)
        {
            if( value == "invariant" )
                return SparseVectorType::invariant;
            if( value == "covariant" )
                return SparseVectorType::covariant;
            if( value == "covariant_normalize" )
                return SparseVectorType::covariant_normalize;
            if( value == "contravariant_relative" )
                return SparseVectorType::contravariant_relative;
            if( value == "contravariant_absolute" )
                return SparseVectorType::contravariant_absolute;
            failManifest(DiagnosticCategory::schema,
                "Sparse Vec3f VDB vector type is invalid", path);
        }

        HouGeoAdapter::PackedFragmentPrimitive::Bounds parseBounds(
            const json::ArrayPtr &values,
            const std::string &path)
        {
            const std::vector<real32> components = scalarArray<real32>(values, path);
            if( components.size() != 6 )
                failManifest(DiagnosticCategory::schema,
                    "Bounds require exactly six values", path);
            HouGeoAdapter::PackedFragmentPrimitive::Bounds result{};
            std::copy(components.begin(), components.end(), result.begin());
            return result;
        }

        void parsePrimitives(
            const json::ObjectPtr &root,
            const std::vector<int> &topology,
            const HouGeo::Ptr &geometry)
        {
            const json::ArrayPtr definitions = requireArray(root, "primitives", "root");
            const int count = checkedCount(definitions->size(), "primitives");
            for( int index = 0; index < count; ++index )
            {
                const std::string path = "primitives[" + std::to_string(index) + "]";
                json::ObjectPtr definition = definitions->object(index);
                if( !definition )
                    failManifest(DiagnosticCategory::schema,
                        "Primitive definition must be an object", path);
                const std::string type = definition->get<std::string>("type");
                const int vertexOffset = checkedCount(
                    definition->get<sint64>("vertex_offset", 0),
                    path + ".vertex_offset");
                if( type != "polygon"
                    && static_cast<std::size_t>(vertexOffset) >= topology.size() )
                {
                    failManifest(DiagnosticCategory::schema,
                        "Primitive topology vertex exceeds the topology domain",
                        path + ".vertex_offset");
                }
                if( type == "polygon" )
                {
                    const int vertexCount = checkedCount(
                        definition->get<sint64>("vertex_count"),
                        path + ".vertex_count");
                    if( vertexCount <= 0
                        || static_cast<std::size_t>(vertexOffset) > topology.size()
                        || static_cast<std::size_t>(vertexCount)
                            > topology.size() - static_cast<std::size_t>(vertexOffset) )
                    {
                        failManifest(DiagnosticCategory::schema,
                            "Polygon vertex range exceeds the topology domain", path);
                    }
                    std::vector<int> pointIndices(
                        topology.begin() + vertexOffset,
                        topology.begin() + vertexOffset + vertexCount);
                    auto polygon = std::make_shared<HouGeo::HouPoly>();
                    polygon->setPolygonData(
                        1,
                        {vertexCount},
                        {0},
                        std::move(pointIndices),
                        definition->get<bool>("closed", true));
                    geometry->addPrimitive(
                        std::static_pointer_cast<HouGeoAdapter::PolyPrimitive>(polygon));
                }
                else if( type == "packed_geometry" )
                {
                    json::ObjectPtr embeddedManifest = definition->object("embedded_manifest");
                    if( !embeddedManifest )
                        failManifest(DiagnosticCategory::schema,
                            "Packed geometry requires an embedded manifest",
                            path + ".embedded_manifest");
                    auto packed = std::make_shared<HouGeo::HouPackedGeometry>();
                    packed->setEmbeddedGeometry(parseGeometryRoot(embeddedManifest));
                    packed->setTopologyVertex(vertexOffset);
                    packed->setPivot(parseVector3(
                        requireArray(definition, "pivot", path),
                        path + ".pivot"));
                    packed->setTransform(parseMatrix33(
                        requireArray(definition, "transform", path),
                        path + ".transform"));
                    packed->setViewportLod(
                        definition->get<std::string>("viewport_lod", "full"));
                    packed->setPointInstanceTransform(
                        definition->get<bool>("point_instance_transform", false));
                    packed->setTreatAsFolder(
                        definition->get<bool>("treat_as_folder", false));
                    geometry->addPrimitive(
                        std::static_pointer_cast<HouGeoAdapter::PackedGeometryPrimitive>(packed));
                }
                else if( type == "packed_fragment" )
                {
                    json::ObjectPtr embeddedManifest = definition->object("embedded_manifest");
                    if( !embeddedManifest )
                        failManifest(DiagnosticCategory::schema,
                            "Packed fragment requires an embedded manifest",
                            path + ".embedded_manifest");
                    const std::string fragmentAttribute =
                        definition->get<std::string>("fragment_attribute", "");
                    if( fragmentAttribute.empty() )
                        failManifest(DiagnosticCategory::schema,
                            "Packed fragment requires a fragment attribute",
                            path + ".fragment_attribute");
                    const std::string fragmentName =
                        definition->get<std::string>("fragment_name", "");
                    if( fragmentName.empty() )
                        failManifest(DiagnosticCategory::schema,
                            "Packed fragment requires a fragment name",
                            path + ".fragment_name");
                    auto packed = std::make_shared<HouGeo::HouPackedFragment>();
                    packed->setEmbeddedGeometry(parseGeometryRoot(embeddedManifest));
                    packed->setTopologyVertex(vertexOffset);
                    packed->setPivot(parseVector3(
                        requireArray(definition, "pivot", path),
                        path + ".pivot"));
                    packed->setTransform(parseMatrix33(
                        requireArray(definition, "transform", path),
                        path + ".transform"));
                    packed->setViewportLod(
                        definition->get<std::string>("viewport_lod", "full"));
                    packed->setPointInstanceTransform(
                        definition->get<bool>("point_instance_transform", false));
                    packed->setFragmentAttribute(fragmentAttribute);
                    packed->setFragmentName(fragmentName);
                    const auto bounds = parseBounds(
                        requireArray(definition, "bounds", path), path + ".bounds");
                    packed->setBounds(bounds);
                    json::ArrayPtr cachedBounds = definition->array("cached_bounds");
                    packed->setCachedBounds(cachedBounds
                        ? parseBounds(cachedBounds, path + ".cached_bounds")
                        : bounds);
                    geometry->addPrimitive(
                        std::static_pointer_cast<HouGeoAdapter::PackedFragmentPrimitive>(packed));
                }
                else if( type == "packed_disk" )
                {
                    const std::string filename =
                        definition->get<std::string>("filename", "");
                    if( filename.empty() )
                        failManifest(DiagnosticCategory::schema,
                            "Packed disk requires a filename", path + ".filename");
                    auto packed = std::make_shared<HouGeo::HouPackedDisk>();
                    packed->setTopologyVertex(vertexOffset);
                    packed->setFilename(filename);
                    packed->setExpandFrame(
                        definition->get<real32>("expand_frame", 1.0f));
                    packed->setExpandFilename(
                        definition->get<bool>("expand_filename", false));
                    packed->setPivot(parseVector3(
                        requireArray(definition, "pivot", path),
                        path + ".pivot"));
                    packed->setTransform(parseMatrix33(
                        requireArray(definition, "transform", path),
                        path + ".transform"));
                    packed->setViewportLod(
                        definition->get<std::string>("viewport_lod", "full"));
                    packed->setPointInstanceTransform(
                        definition->get<bool>("point_instance_transform", false));
                    packed->setTreatAsFolder(
                        definition->get<bool>("treat_as_folder", false));
                    geometry->addPrimitive(
                        std::static_pointer_cast<HouGeoAdapter::PackedDiskPrimitive>(packed));
                }
                else if( type == "packed_disk_sequence" )
                {
                    json::ArrayPtr filenameValues =
                        requireArray(definition, "filenames", path);
                    if( filenameValues->size() <= 0 )
                        failManifest(DiagnosticCategory::schema,
                            "Packed disk sequence requires at least one filename",
                            path + ".filenames");
                    const int filenameCount = checkedCount(
                        filenameValues->size(), path + ".filenames");
                    std::vector<std::string> filenames;
                    filenames.reserve(static_cast<std::size_t>(filenameCount));
                    for( int filenameIndex = 0; filenameIndex < filenameCount; ++filenameIndex )
                    {
                        const std::string filename =
                            filenameValues->get<std::string>(filenameIndex);
                        if( filename.empty() )
                            failManifest(DiagnosticCategory::schema,
                                "Packed disk sequence filename cannot be empty",
                                path + ".filenames["
                                    + std::to_string(filenameIndex) + "]");
                        filenames.push_back(filename);
                    }

                    const std::string wrap =
                        definition->get<std::string>("wrap", "cycle");
                    HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode wrapMode;
                    if( wrap == "cycle" )
                        wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::cycle;
                    else if( wrap == "clamp" )
                        wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::clamp;
                    else if( wrap == "strict" )
                        wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::strict;
                    else if( wrap == "mirror" )
                        wrapMode = HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::mirror;
                    else
                        failManifest(DiagnosticCategory::schema,
                            "Packed disk sequence wrap mode is invalid",
                            path + ".wrap");

                    auto packed = std::make_shared<HouGeo::HouPackedDiskSequence>();
                    packed->setTopologyVertex(vertexOffset);
                    packed->setFilenames(std::move(filenames));
                    packed->setIndex(definition->get<real32>("index", 0.0f));
                    packed->setWrapMode(wrapMode);
                    packed->setPivot(parseVector3(
                        requireArray(definition, "pivot", path),
                        path + ".pivot"));
                    packed->setTransform(parseMatrix33(
                        requireArray(definition, "transform", path),
                        path + ".transform"));
                    packed->setViewportLod(
                        definition->get<std::string>("viewport_lod", "full"));
                    packed->setPointInstanceTransform(
                        definition->get<bool>("point_instance_transform", false));
                    geometry->addPrimitive(
                        std::static_pointer_cast<
                            HouGeoAdapter::PackedDiskSequencePrimitive>(packed));
                }
                else if( type == "sparse_float_vdb" )
                {
                    const std::string gridClassName =
                        definition->get<std::string>("grid_class", "unknown");
                    SparseGridClass gridClass = SparseGridClass::unknown;
                    if( gridClassName == "fog_volume" )
                        gridClass = SparseGridClass::fog_volume;
                    else if( gridClassName == "level_set" )
                        gridClass = SparseGridClass::level_set;
                    else if( gridClassName != "unknown" )
                        failManifest(DiagnosticCategory::schema,
                            "Sparse VDB grid class is invalid",
                            path + ".grid_class");

                    SparseFloatGrid grid(
                        definition->get<real32>("background", 0.0f));
                    grid.setName(definition->get<std::string>("name", ""));
                    grid.setGridClass(gridClass);
                    grid.setIndexToWorld(parseMatrix44(
                        requireArray(definition, "index_to_world", path),
                        path + ".index_to_world"));

                    const bool hasActiveIndices = definition->contains("active_indices");
                    const bool hasActiveValues = definition->contains("active_values");
                    if( hasActiveIndices != hasActiveValues )
                    {
                        failManifest(DiagnosticCategory::schema,
                            "Sparse VDB active_indices and active_values must be provided together",
                            path);
                    }
                    if( hasActiveIndices )
                    {
                        const std::vector<sint32> activeIndices = scalarArray<sint32>(
                            requireArray(definition, "active_indices", path),
                            path + ".active_indices");
                        const std::vector<real32> activeValues = scalarArray<real32>(
                            requireArray(definition, "active_values", path),
                            path + ".active_values");
                        if( activeIndices.size() % 3 != 0
                            || activeValues.size() != activeIndices.size() / 3 )
                        {
                            failManifest(DiagnosticCategory::schema,
                                "Sparse VDB active indices and values have inconsistent lengths",
                                path + ".active_indices");
                        }
                        for( std::size_t valueIndex = 0;
                            valueIndex < activeValues.size(); ++valueIndex )
                        {
                            const std::size_t coordinateOffset = valueIndex * 3;
                            grid.setVoxel(
                                math::V3i(
                                    activeIndices[coordinateOffset],
                                    activeIndices[coordinateOffset + 1],
                                    activeIndices[coordinateOffset + 2]),
                                activeValues[valueIndex]);
                        }
                        if( grid.activeVoxelCount() != activeValues.size() )
                        {
                            failManifest(DiagnosticCategory::schema,
                                "Sparse VDB active indices contain duplicate coordinates",
                                path + ".active_indices");
                        }
                    }

                    if( definition->contains("active_tiles") )
                    {
                        const json::ArrayPtr activeTiles =
                            requireArray(definition, "active_tiles", path);
                        const std::size_t tileCount =
                            checkedSize(activeTiles->size(), path + ".active_tiles");
                        std::vector<SparseIndexBounds> parsedBounds;
                        parsedBounds.reserve(tileCount);
                        for( std::size_t tileIndex = 0; tileIndex < tileCount; ++tileIndex )
                        {
                            const std::string tilePath = path + ".active_tiles["
                                + std::to_string(tileIndex) + "]";
                            const json::ObjectPtr tile =
                                activeTiles->object(static_cast<int>(tileIndex));
                            if( !tile )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse VDB active tile must be an object", tilePath);

                            const std::vector<sint32> minimum = scalarArray<sint32>(
                                requireArray(tile, "minimum", tilePath),
                                tilePath + ".minimum");
                            const std::vector<sint32> maximum = scalarArray<sint32>(
                                requireArray(tile, "maximum", tilePath),
                                tilePath + ".maximum");
                            if( minimum.size() != 3 || maximum.size() != 3 )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse VDB active tile bounds must contain three integers",
                                    tilePath);
                            }
                            if( !tile->contains("value") )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse VDB active tile requires a value",
                                    tilePath + ".value");
                            real32 value = 0.0f;
                            try
                            {
                                value = tile->get<real32>("value");
                            }
                            catch( const std::exception& exception )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Invalid sparse VDB active tile value: "
                                        + std::string(exception.what()),
                                    tilePath + ".value");
                            }
                            if( !std::isfinite(value) )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse VDB active tile value must be finite",
                                    tilePath + ".value");

                            const SparseIndexBounds bounds{
                                math::V3i(minimum[0], minimum[1], minimum[2]),
                                math::V3i(maximum[0], maximum[1], maximum[2])};
                            if( bounds.minimum.x > bounds.maximum.x
                                || bounds.minimum.y > bounds.maximum.y
                                || bounds.minimum.z > bounds.maximum.z )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse VDB active tile bounds must be ordered",
                                    tilePath);
                            }
                            const auto duplicate = std::find_if(
                                parsedBounds.begin(), parsedBounds.end(),
                                [&](const SparseIndexBounds& existing)
                                {
                                    return existing.minimum == bounds.minimum
                                        && existing.maximum == bounds.maximum;
                                });
                            if( duplicate != parsedBounds.end() )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse VDB active tiles contain duplicate bounds",
                                    tilePath);

                            parsedBounds.push_back(bounds);
                            grid.addActiveTile(bounds, value);
                        }
                    }

                    if( const json::ObjectPtr metadata = definition->object("metadata") )
                    {
                        for( const std::string& key : metadata->keys() )
                        {
                            if( key.empty() )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse VDB metadata key cannot be empty",
                                    path + ".metadata");
                            grid.setMetadata(key, metadata->get<std::string>(key));
                        }
                    }

                    auto sparseVdb = std::make_shared<HouGeo::HouSparseVdb>();
                    sparseVdb->setTopologyVertex(vertexOffset);
                    sparseVdb->setSparseGrid(std::move(grid));
                    geometry->addPrimitive(
                        std::static_pointer_cast<HouGeoAdapter::SparseVdbPrimitive>(sparseVdb));
                }
                else if( type == "sparse_int32_vdb" )
                {
                    const std::string gridClassName =
                        definition->get<std::string>("grid_class", "unknown");
                    SparseGridClass gridClass = SparseGridClass::unknown;
                    if( gridClassName == "fog_volume" )
                        gridClass = SparseGridClass::fog_volume;
                    else if( gridClassName == "level_set" )
                        gridClass = SparseGridClass::level_set;
                    else if( gridClassName != "unknown" )
                        failManifest(DiagnosticCategory::schema,
                            "Sparse Int32 VDB grid class is invalid",
                            path + ".grid_class");

                    SparseInt32Grid grid(
                        definition->get<sint32>("background", 0));
                    grid.setName(definition->get<std::string>("name", ""));
                    grid.setGridClass(gridClass);
                    grid.setIndexToWorld(parseMatrix44(
                        requireArray(definition, "index_to_world", path),
                        path + ".index_to_world"));

                    const bool hasActiveIndices = definition->contains("active_indices");
                    const bool hasActiveValues = definition->contains("active_values");
                    if( hasActiveIndices != hasActiveValues )
                    {
                        failManifest(DiagnosticCategory::schema,
                            "Sparse Int32 VDB active_indices and active_values must be provided together",
                            path);
                    }
                    if( hasActiveIndices )
                    {
                        const std::vector<sint32> activeIndices = scalarArray<sint32>(
                            requireArray(definition, "active_indices", path),
                            path + ".active_indices");
                        const std::vector<sint32> activeValues = scalarArray<sint32>(
                            requireArray(definition, "active_values", path),
                            path + ".active_values");
                        if( activeIndices.size() % 3 != 0
                            || activeValues.size() != activeIndices.size() / 3 )
                        {
                            failManifest(DiagnosticCategory::schema,
                                "Sparse Int32 VDB active indices and values have inconsistent lengths",
                                path + ".active_indices");
                        }
                        for( std::size_t valueIndex = 0;
                            valueIndex < activeValues.size(); ++valueIndex )
                        {
                            const std::size_t coordinateOffset = valueIndex * 3;
                            grid.setVoxel(
                                math::V3i(
                                    activeIndices[coordinateOffset],
                                    activeIndices[coordinateOffset + 1],
                                    activeIndices[coordinateOffset + 2]),
                                activeValues[valueIndex]);
                        }
                        if( grid.activeVoxelCount() != activeValues.size() )
                        {
                            failManifest(DiagnosticCategory::schema,
                                "Sparse Int32 VDB active indices contain duplicate coordinates",
                                path + ".active_indices");
                        }
                    }

                    if( definition->contains("active_tiles") )
                    {
                        const json::ArrayPtr activeTiles =
                            requireArray(definition, "active_tiles", path);
                        const std::size_t tileCount =
                            checkedSize(activeTiles->size(), path + ".active_tiles");
                        std::vector<SparseIndexBounds> parsedBounds;
                        parsedBounds.reserve(tileCount);
                        for( std::size_t tileIndex = 0; tileIndex < tileCount; ++tileIndex )
                        {
                            const std::string tilePath = path + ".active_tiles["
                                + std::to_string(tileIndex) + "]";
                            const json::ObjectPtr tile =
                                activeTiles->object(static_cast<int>(tileIndex));
                            if( !tile )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Int32 VDB active tile must be an object", tilePath);

                            const std::vector<sint32> minimum = scalarArray<sint32>(
                                requireArray(tile, "minimum", tilePath),
                                tilePath + ".minimum");
                            const std::vector<sint32> maximum = scalarArray<sint32>(
                                requireArray(tile, "maximum", tilePath),
                                tilePath + ".maximum");
                            if( minimum.size() != 3 || maximum.size() != 3 )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Int32 VDB active tile bounds must contain three integers",
                                    tilePath);
                            }
                            if( !tile->contains("value") )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Int32 VDB active tile requires a value",
                                    tilePath + ".value");
                            sint32 value = 0;
                            try
                            {
                                value = tile->get<sint32>("value");
                            }
                            catch( const std::exception& exception )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Invalid sparse Int32 VDB active tile value: "
                                        + std::string(exception.what()),
                                    tilePath + ".value");
                            }

                            const SparseIndexBounds bounds{
                                math::V3i(minimum[0], minimum[1], minimum[2]),
                                math::V3i(maximum[0], maximum[1], maximum[2])};
                            if( bounds.minimum.x > bounds.maximum.x
                                || bounds.minimum.y > bounds.maximum.y
                                || bounds.minimum.z > bounds.maximum.z )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Int32 VDB active tile bounds must be ordered",
                                    tilePath);
                            }
                            const auto duplicate = std::find_if(
                                parsedBounds.begin(), parsedBounds.end(),
                                [&](const SparseIndexBounds& existing)
                                {
                                    return existing.minimum == bounds.minimum
                                        && existing.maximum == bounds.maximum;
                                });
                            if( duplicate != parsedBounds.end() )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Int32 VDB active tiles contain duplicate bounds",
                                    tilePath);

                            parsedBounds.push_back(bounds);
                            grid.addActiveTile(bounds, value);
                        }
                    }

                    if( const json::ObjectPtr metadata = definition->object("metadata") )
                    {
                        for( const std::string& key : metadata->keys() )
                        {
                            if( key.empty() )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Int32 VDB metadata key cannot be empty",
                                    path + ".metadata");
                            grid.setMetadata(key, metadata->get<std::string>(key));
                        }
                    }

                    auto sparseVdb = std::make_shared<HouGeo::HouSparseInt32Vdb>();
                    sparseVdb->setTopologyVertex(vertexOffset);
                    sparseVdb->setSparseGrid(std::move(grid));
                    geometry->addPrimitive(
                        std::static_pointer_cast<
                            HouGeoAdapter::SparseInt32VdbPrimitive>(sparseVdb));
                }
                else if( type == "sparse_vec3f_vdb" )
                {
                    const std::string gridClassName =
                        definition->get<std::string>("grid_class", "unknown");
                    SparseGridClass gridClass = SparseGridClass::unknown;
                    if( gridClassName == "fog_volume" )
                        gridClass = SparseGridClass::fog_volume;
                    else if( gridClassName == "level_set" )
                        gridClass = SparseGridClass::level_set;
                    else if( gridClassName == "staggered" )
                        gridClass = SparseGridClass::staggered;
                    else if( gridClassName != "unknown" )
                        failManifest(DiagnosticCategory::schema,
                            "Sparse Vec3f VDB grid class is invalid",
                            path + ".grid_class");

                    math::V3f background(0.0f);
                    if( definition->contains("background") )
                    {
                        background = parseFiniteVector3(
                            requireArray(definition, "background", path),
                            path + ".background");
                    }
                    SparseVec3fGrid grid(background);
                    grid.setName(definition->get<std::string>("name", ""));
                    grid.setGridClass(gridClass);
                    grid.setVectorType(parseSparseVectorType(
                        definition->get<std::string>("vector_type", "invariant"),
                        path + ".vector_type"));
                    grid.setIndexToWorld(parseMatrix44(
                        requireArray(definition, "index_to_world", path),
                        path + ".index_to_world"));

                    const bool hasActiveIndices = definition->contains("active_indices");
                    const bool hasActiveValues = definition->contains("active_values");
                    if( hasActiveIndices != hasActiveValues )
                    {
                        failManifest(DiagnosticCategory::schema,
                            "Sparse Vec3f VDB active_indices and active_values must be provided together",
                            path);
                    }
                    if( hasActiveIndices )
                    {
                        const std::vector<sint32> activeIndices = scalarArray<sint32>(
                            requireArray(definition, "active_indices", path),
                            path + ".active_indices");
                        const json::ArrayPtr activeValues =
                            requireArray(definition, "active_values", path);
                        const std::size_t valueCount =
                            checkedSize(activeValues->size(), path + ".active_values");
                        if( activeIndices.size() % 3 != 0
                            || valueCount != activeIndices.size() / 3 )
                        {
                            failManifest(DiagnosticCategory::schema,
                                "Sparse Vec3f VDB active indices and values have inconsistent lengths",
                                path + ".active_indices");
                        }
                        for( std::size_t valueIndex = 0; valueIndex < valueCount; ++valueIndex )
                        {
                            const std::size_t coordinateOffset = valueIndex * 3;
                            const std::string valuePath = path + ".active_values["
                                + std::to_string(valueIndex) + "]";
                            const json::ArrayPtr tuple =
                                activeValues->array(static_cast<int>(valueIndex));
                            if( !tuple )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Vec3f VDB active value must be a three-component array",
                                    valuePath);
                            grid.setVoxel(
                                math::V3i(
                                    activeIndices[coordinateOffset],
                                    activeIndices[coordinateOffset + 1],
                                    activeIndices[coordinateOffset + 2]),
                                parseFiniteVector3(tuple, valuePath));
                        }
                        if( grid.activeVoxelCount() != valueCount )
                        {
                            failManifest(DiagnosticCategory::schema,
                                "Sparse Vec3f VDB active indices contain duplicate coordinates",
                                path + ".active_indices");
                        }
                    }

                    if( definition->contains("active_tiles") )
                    {
                        const json::ArrayPtr activeTiles =
                            requireArray(definition, "active_tiles", path);
                        const std::size_t tileCount =
                            checkedSize(activeTiles->size(), path + ".active_tiles");
                        std::vector<SparseIndexBounds> parsedBounds;
                        parsedBounds.reserve(tileCount);
                        for( std::size_t tileIndex = 0; tileIndex < tileCount; ++tileIndex )
                        {
                            const std::string tilePath = path + ".active_tiles["
                                + std::to_string(tileIndex) + "]";
                            const json::ObjectPtr tile =
                                activeTiles->object(static_cast<int>(tileIndex));
                            if( !tile )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Vec3f VDB active tile must be an object", tilePath);

                            const std::vector<sint32> minimum = scalarArray<sint32>(
                                requireArray(tile, "minimum", tilePath),
                                tilePath + ".minimum");
                            const std::vector<sint32> maximum = scalarArray<sint32>(
                                requireArray(tile, "maximum", tilePath),
                                tilePath + ".maximum");
                            if( minimum.size() != 3 || maximum.size() != 3 )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Vec3f VDB active tile bounds must contain three integers",
                                    tilePath);
                            }
                            const SparseIndexBounds bounds{
                                math::V3i(minimum[0], minimum[1], minimum[2]),
                                math::V3i(maximum[0], maximum[1], maximum[2])};
                            if( bounds.minimum.x > bounds.maximum.x
                                || bounds.minimum.y > bounds.maximum.y
                                || bounds.minimum.z > bounds.maximum.z )
                            {
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Vec3f VDB active tile bounds must be ordered",
                                    tilePath);
                            }
                            const auto duplicate = std::find_if(
                                parsedBounds.begin(), parsedBounds.end(),
                                [&](const SparseIndexBounds& existing)
                                {
                                    return existing.minimum == bounds.minimum
                                        && existing.maximum == bounds.maximum;
                                });
                            if( duplicate != parsedBounds.end() )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Vec3f VDB active tiles contain duplicate bounds",
                                    tilePath);

                            parsedBounds.push_back(bounds);
                            grid.addActiveTile(bounds, parseFiniteVector3(
                                requireArray(tile, "value", tilePath),
                                tilePath + ".value"));
                        }
                    }

                    if( const json::ObjectPtr metadata = definition->object("metadata") )
                    {
                        for( const std::string& key : metadata->keys() )
                        {
                            if( key.empty() )
                                failManifest(DiagnosticCategory::schema,
                                    "Sparse Vec3f VDB metadata key cannot be empty",
                                    path + ".metadata");
                            grid.setMetadata(key, metadata->get<std::string>(key));
                        }
                    }

                    auto sparseVdb = std::make_shared<HouGeo::HouSparseVec3fVdb>();
                    sparseVdb->setTopologyVertex(vertexOffset);
                    sparseVdb->setSparseGrid(std::move(grid));
                    geometry->addPrimitive(
                        std::static_pointer_cast<
                            HouGeoAdapter::SparseVec3fVdbPrimitive>(sparseVdb));
                }
                else if( type == "dense_volume" )
                {
                    const json::ArrayPtr resolutionValues = requireArray(
                        definition, "resolution", path);
                    const std::vector<sint32> resolution = scalarArray<sint32>(
                        resolutionValues, path + ".resolution");
                    if( resolution.size() != 3
                        || resolution[0] <= 0 || resolution[1] <= 0 || resolution[2] <= 0 )
                    {
                        failManifest(DiagnosticCategory::schema,
                            "Dense volume resolution must contain three positive values",
                            path + ".resolution");
                    }
                    const math::V3i fieldResolution(
                        resolution[0], resolution[1], resolution[2]);
                    ScalarField::Ptr field = ScalarField::create(fieldResolution);
                    field->setLocalToWorld(parseMatrix44(
                        requireArray(definition, "local_to_world", path),
                        path + ".local_to_world"));
                    const std::vector<real32> voxels = scalarArray<real32>(
                        requireArray(definition, "voxels", path), path + ".voxels");
                    if( voxels.size() != field->values().size() )
                        failManifest(DiagnosticCategory::schema,
                            "Dense volume voxel count does not match its resolution",
                            path + ".voxels");
                    std::copy(voxels.begin(), voxels.end(), field->values().begin());

                    auto volume = std::make_shared<HouGeo::HouVolume>();
                    volume->setField(std::move(field));
                    volume->setTopologyVertex(vertexOffset);
                    json::ObjectPtr visualization = definition->object("visualization");
                    if( visualization )
                    {
                        volume->setVisualization(
                            visualization->get<std::string>("mode", "smoke"),
                            visualization->get<real32>("iso", 0.0f),
                            visualization->get<real32>("density", 1.0f));
                    }
                    geometry->addPrimitive(
                        std::static_pointer_cast<HouGeoAdapter::VolumePrimitive>(volume));
                }
                else
                {
                    failManifest(DiagnosticCategory::unsupported_input,
                        "Manifest primitive type is unsupported: " + type,
                        path + ".type");
                }
            }
        }

        HouGeo::Ptr parseGeometryRoot(const json::ObjectPtr &root)
        {
            if( !root )
                failManifest(DiagnosticCategory::schema,
                    "HOM manifest root must be an object", "root");
            if( root->get<std::string>("schema") != "houio.hom/1" )
                failManifest(DiagnosticCategory::unsupported_input,
                    "Unsupported HOM manifest schema", "schema");

            const int pointCount = checkedCount(
                root->get<sint64>("point_count"), "point_count");
            const int vertexCount = checkedCount(
                root->get<sint64>("vertex_count"), "vertex_count");
            const int primitiveCount = checkedCount(
                root->get<sint64>("primitive_count"), "primitive_count");

            const std::vector<int> topology = scalarArray<int>(
                requireArray(root, "topology", "root"), "topology");
            if( topology.size() != static_cast<std::size_t>(vertexCount) )
                failManifest(DiagnosticCategory::schema,
                    "Topology count does not match vertex_count", "topology");
            for( std::size_t index = 0; index < topology.size(); ++index )
            {
                if( topology[index] < 0 || topology[index] >= pointCount )
                    failManifest(DiagnosticCategory::schema,
                        "Topology point index exceeds point_count",
                        "topology[" + std::to_string(index) + "]");
            }

            HouGeo::Ptr geometry = HouGeo::create();
            auto houTopology = std::make_shared<HouGeo::HouTopology>();
            houTopology->setIndices(topology);
            geometry->setTopology(std::move(houTopology));

            json::ObjectPtr attributes = requireObject(root, "attributes", "root");
            parseAttributeDomain(attributes, "point", pointCount, geometry);
            if( geometry->pointCount() != pointCount )
                failManifest(DiagnosticCategory::schema,
                    "Point attributes do not establish the declared point_count",
                    "attributes.point");
            parsePrimitives(root, topology, geometry);
            if( geometry->primitiveCount() != primitiveCount )
                failManifest(DiagnosticCategory::schema,
                    "Primitive records do not match primitive_count", "primitives");
            parseAttributeDomain(attributes, "vertex", vertexCount, geometry);
            parseAttributeDomain(attributes, "primitive", primitiveCount, geometry);
            parseAttributeDomain(attributes, "global", 1, geometry);
            parseGroups(root, pointCount, vertexCount, primitiveCount, geometry);
            return geometry;
        }
    }

    GeometryReadResult<HouGeo::Ptr> HomManifest::read(
        const std::filesystem::path &path,
        const json::ParserLimits &limits)
    {
        GeometryReadResult<HouGeo::Ptr> result;
        std::ifstream input(path, std::ios::binary);
        if( !input )
        {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::io,
                "Could not open HOM manifest: " + path.string(),
                -1,
                "file"});
            return result;
        }

        try
        {
            json::JSONReader reader;
            json::Parser parser(limits);
            if( !parser.parse(input, reader, result.diagnostics) )
                return result;
            result.value = parseGeometryRoot(reader.root().asObject());
            result.succeeded = true;
        }
        catch( const DiagnosticException &exception )
        {
            result.diagnostics.push_back(exception.diagnostic());
        }
        catch( const std::exception &exception )
        {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                DiagnosticCategory::schema,
                exception.what(),
                -1,
                "manifest"});
        }
        return result;
    }
}
