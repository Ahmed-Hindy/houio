#include "HomManifestInternal.h"

#include <memory>
#include <span>
#include <utility>

namespace houio::hom_manifest_detail
{
    namespace
    {
        template<typename T>
        Attribute::Ptr numericAttribute(
            const json::ArrayPtr& values,
            int tupleSize,
            int elementCount,
            Attribute::ComponentType componentType,
            const std::string& path)
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
            const json::ObjectPtr& definition,
            int expectedElementCount,
            const std::string& path)
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
                case HouGeoAdapter::AttributeAdapter::Storage::uint8:
                    numeric = numericAttribute<ubyte>(values, tupleSize, elementCount,
                        Attribute::ComponentType::uint8, path);
                    break;
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

        std::vector<bool> parseMembership(
            const json::ArrayPtr& indices,
            int elementCount,
            const std::string& path)
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
    }

    void parseAttributeDomain(
        const json::ObjectPtr& attributes,
        const std::string& domain,
        int elementCount,
        const HouGeo::Ptr& geometry)
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

    void parseGroups(
        const json::ObjectPtr& root,
        int pointCount,
        int vertexCount,
        int primitiveCount,
        const HouGeo::Ptr& geometry)
    {
        json::ObjectPtr groups = root->object("groups");
        if( !groups )
            return;
        for( const std::string& domain : {"point", "vertex", "primitive"} )
        {
            json::ObjectPtr domainGroups = groups->object(domain);
            if( !domainGroups )
                continue;
            const int elementCount = domain == "point"
                ? pointCount
                : domain == "vertex" ? vertexCount : primitiveCount;
            for( const std::string& name : domainGroups->keys() )
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
}
