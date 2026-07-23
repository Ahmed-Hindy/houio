#include <houio/HouGeoAdapter.h>

#include <houio/json.h>

namespace houio
{
    std::string HouGeoAdapter::AttributeAdapter::name() const
    {
        return {};
    }

    HouGeoAdapter::AttributeAdapter::Type HouGeoAdapter::AttributeAdapter::type() const
    {
        return Type::invalid;
    }

    int HouGeoAdapter::AttributeAdapter::tupleSize() const
    {
        return 0;
    }

    HouGeoAdapter::AttributeAdapter::Storage HouGeoAdapter::AttributeAdapter::storage() const
    {
        return Storage::invalid;
    }

    std::vector<int> HouGeoAdapter::AttributeAdapter::packing() const
    {
        return {};
    }

    int HouGeoAdapter::AttributeAdapter::elementCount() const
    {
        return 0;
    }

    HouGeoAdapter::RawDataView HouGeoAdapter::AttributeAdapter::rawData() const
    {
        return {};
    }

    std::shared_ptr<json::Object> HouGeoAdapter::AttributeAdapter::dictionaryValue(int) const
    {
        return nullptr;
    }

    HouGeoAdapter::AttributeAdapter::Type HouGeoAdapter::AttributeAdapter::parseType(
        const std::string& type_name)
    {
        if (type_name == "numeric")
            return Type::numeric;
        if (type_name == "string")
            return Type::string;
        if (type_name == "dict")
            return Type::dictionary;
        return Type::invalid;
    }

    HouGeoAdapter::AttributeAdapter::Storage HouGeoAdapter::AttributeAdapter::parseStorage(
        const std::string& storage_name)
    {
        if (storage_name == "fpreal16")
            return Storage::float16;
        if (storage_name == "fpreal32")
            return Storage::float32;
        if (storage_name == "fpreal64")
            return Storage::float64;
        if (storage_name == "int32")
            return Storage::int32;
        if (storage_name == "int64")
            return Storage::int64;
        return Storage::invalid;
    }

    int HouGeoAdapter::AttributeAdapter::storageSize(Storage storage_type)
    {
        switch (storage_type)
        {
        case Storage::float16:
            return static_cast<int>(sizeof(uword));
        case Storage::float32:
            return static_cast<int>(sizeof(real32));
        case Storage::float64:
            return static_cast<int>(sizeof(real64));
        case Storage::int32:
            return static_cast<int>(sizeof(sint32));
        case Storage::int64:
            return static_cast<int>(sizeof(sint64));
        case Storage::invalid:
            return 0;
        }
        return 0;
    }

    math::Vec3i HouGeoAdapter::VolumePrimitive::resolution() const
    {
        return math::Vec3i(0);
    }

    std::string HouGeoAdapter::VolumePrimitive::visualizationMode() const
    {
        return "smoke";
    }

    real32 HouGeoAdapter::VolumePrimitive::visualizationIso() const
    {
        return 0.0f;
    }

    real32 HouGeoAdapter::VolumePrimitive::visualizationDensity() const
    {
        return 1.0f;
    }

    HouGeoAdapter::RawDataView HouGeoAdapter::VolumePrimitive::rawData() const
    {
        return {};
    }

    int HouGeoAdapter::PolyPrimitive::polygonCount() const
    {
        return 0;
    }

    int HouGeoAdapter::PolyPrimitive::polygonVertexCount(int) const
    {
        return 0;
    }

    std::span<const int> HouGeoAdapter::PolyPrimitive::polygonVertexIndices(int) const
    {
        return {};
    }

    bool HouGeoAdapter::PolyPrimitive::isClosed() const
    {
        return false;
    }

    sint64 HouGeoAdapter::pointCount() const
    {
        return 0;
    }

    sint64 HouGeoAdapter::vertexCount() const
    {
        return 0;
    }

    sint64 HouGeoAdapter::primitiveCount() const
    {
        return 0;
    }

    std::vector<std::string> HouGeoAdapter::pointAttributeNames() const
    {
        return {};
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::pointAttribute(const std::string&)
    {
        return nullptr;
    }

    std::vector<std::string> HouGeoAdapter::vertexAttributeNames() const
    {
        return {};
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::vertexAttribute(const std::string&)
    {
        return nullptr;
    }

    std::vector<std::string> HouGeoAdapter::globalAttributeNames() const
    {
        return {};
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::globalAttribute(const std::string&)
    {
        return nullptr;
    }

    std::vector<std::string> HouGeoAdapter::pointGroupNames() const
    {
        return {};
    }

    std::optional<std::vector<bool>> HouGeoAdapter::pointGroupMembership(
        const std::string&) const
    {
        return std::nullopt;
    }

    std::vector<std::string> HouGeoAdapter::vertexGroupNames() const
    {
        return {};
    }

    std::optional<std::vector<bool>> HouGeoAdapter::vertexGroupMembership(
        const std::string&) const
    {
        return std::nullopt;
    }

    std::vector<std::string> HouGeoAdapter::primitiveGroupNames() const
    {
        return {};
    }

    std::optional<std::vector<bool>> HouGeoAdapter::primitiveGroupMembership(
        const std::string&) const
    {
        return std::nullopt;
    }

    bool HouGeoAdapter::hasPrimitiveAttribute(const std::string&) const
    {
        return false;
    }

    std::vector<HouGeoAdapter::Primitive::Ptr> HouGeoAdapter::primitives()
    {
        return {};
    }

    HouGeoAdapter::Topology::Ptr HouGeoAdapter::topology()
    {
        return nullptr;
    }
}
