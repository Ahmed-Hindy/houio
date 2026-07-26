#include <houio/HouGeoAdapter.h>

#include <houio/json.h>

namespace houio
{
    std::string HouGeoAdapter::AttributeAdapter::name() const
    {
        return {};
    }

    std::string HouGeoAdapter::AttributeAdapter::scope() const
    {
        return "public";
    }

    std::shared_ptr<json::Object> HouGeoAdapter::AttributeAdapter::options() const
    {
        return nullptr;
    }

    HouGeoAdapter::AttributeAdapter::Type HouGeoAdapter::AttributeAdapter::type() const
    {
        return Type::invalid;
    }

    HouGeoAdapter::AttributeAdapter::Storage HouGeoAdapter::AttributeAdapter::storage() const
    {
        return Storage::invalid;
    }

    std::vector<int> HouGeoAdapter::AttributeAdapter::packing() const
    {
        return {};
    }

    std::span<const int> HouGeoAdapter::Topology::indexView() const noexcept
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

    std::string HouGeoAdapter::AttributeAdapter::stringValue(
        int element_index,
        int component_index) const
    {
        if (component_index != 0)
            throw std::out_of_range("AttributeAdapter string component is out of range");
        return stringValue(element_index);
    }

    std::shared_ptr<json::Object> HouGeoAdapter::AttributeAdapter::dictionaryValue(int) const
    {
        return nullptr;
    }

    HouGeoAdapter::AttributeAdapter::Type HouGeoAdapter::AttributeAdapter::parseType(
        std::string_view type_name) noexcept
    {
        if (type_name == "numeric")
            return Type::numeric;
        if (type_name == "string")
            return Type::string;
        if (type_name == "dict")
            return Type::dictionary;
        return Type::invalid;
    }

    std::optional<std::string_view> HouGeoAdapter::AttributeAdapter::typeName(Type type) noexcept
    {
        switch (type)
        {
        case Type::numeric:
            return "numeric";
        case Type::string:
            return "string";
        case Type::dictionary:
            return "dict";
        case Type::invalid:
            return std::nullopt;
        }
        return std::nullopt;
    }

    HouGeoAdapter::AttributeAdapter::Storage HouGeoAdapter::AttributeAdapter::parseStorage(
        std::string_view storage_name) noexcept
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

    std::optional<std::string_view> HouGeoAdapter::AttributeAdapter::storageName(
        Storage storage) noexcept
    {
        switch (storage)
        {
        case Storage::float16:
            return "fpreal16";
        case Storage::float32:
            return "fpreal32";
        case Storage::float64:
            return "fpreal64";
        case Storage::int32:
            return "int32";
        case Storage::int64:
            return "int64";
        case Storage::invalid:
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<std::size_t> HouGeoAdapter::AttributeAdapter::storageByteWidth(
        Storage storage) noexcept
    {
        switch (storage)
        {
        case Storage::float16:
            return sizeof(uword);
        case Storage::float32:
            return sizeof(real32);
        case Storage::float64:
            return sizeof(real64);
        case Storage::int32:
            return sizeof(sint32);
        case Storage::int64:
            return sizeof(sint64);
        case Storage::invalid:
            return std::nullopt;
        }
        return std::nullopt;
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

    HouGeoAdapter::AttributeAdapter::ConstPtr HouGeoAdapter::pointAttribute(
        const std::string&) const
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

    HouGeoAdapter::AttributeAdapter::ConstPtr HouGeoAdapter::vertexAttribute(
        const std::string&) const
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

    HouGeoAdapter::AttributeAdapter::ConstPtr HouGeoAdapter::globalAttribute(
        const std::string&) const
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

    std::vector<HouGeoAdapter::Primitive::ConstPtr> HouGeoAdapter::primitives() const
    {
        return {};
    }

    std::span<const HouGeoAdapter::Primitive::Ptr> HouGeoAdapter::primitiveView() const noexcept
    {
        return {};
    }

    HouGeoAdapter::Topology::Ptr HouGeoAdapter::topology()
    {
        return nullptr;
    }

    HouGeoAdapter::Topology::ConstPtr HouGeoAdapter::topology() const
    {
        return nullptr;
    }
}
