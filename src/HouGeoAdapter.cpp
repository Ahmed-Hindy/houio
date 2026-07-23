#include <houio/HouGeoAdapter.h>

#include <houio/json.h>

namespace houio
{
    HouGeoAdapter::AttributeAdapter::~AttributeAdapter() = default;

    std::string HouGeoAdapter::AttributeAdapter::name() const
    {
        return getName();
    }

    HouGeoAdapter::AttributeAdapter::Type HouGeoAdapter::AttributeAdapter::type() const
    {
        return getType();
    }

    int HouGeoAdapter::AttributeAdapter::tupleSize() const
    {
        return getTupleSize();
    }

    HouGeoAdapter::AttributeAdapter::Storage HouGeoAdapter::AttributeAdapter::storage() const
    {
        return getStorage();
    }

    std::vector<int> HouGeoAdapter::AttributeAdapter::packing() const
    {
        std::vector<int> result;
        getPacking(result);
        return result;
    }

    int HouGeoAdapter::AttributeAdapter::elementCount() const
    {
        return getNumElements();
    }

    std::string HouGeoAdapter::AttributeAdapter::stringValue(int index) const
    {
        return getString(index);
    }

    std::shared_ptr<json::Object> HouGeoAdapter::AttributeAdapter::dictionaryValue(
        int index) const
    {
        return getDictionary(index);
    }

    std::string HouGeoAdapter::AttributeAdapter::getName() const
    {
        return {};
    }

    HouGeoAdapter::AttributeAdapter::Type HouGeoAdapter::AttributeAdapter::getType() const
    {
        return Type::invalid;
    }

    int HouGeoAdapter::AttributeAdapter::getTupleSize() const
    {
        return 0;
    }

    HouGeoAdapter::AttributeAdapter::Storage HouGeoAdapter::AttributeAdapter::getStorage() const
    {
        return Storage::invalid;
    }

    void HouGeoAdapter::AttributeAdapter::getPacking(std::vector<int>& packing) const
    {
        packing.clear();
    }

    int HouGeoAdapter::AttributeAdapter::getNumElements() const
    {
        return 0;
    }

    HouGeoAdapter::RawDataView HouGeoAdapter::AttributeAdapter::rawData() const
    {
        return {};
    }

    std::shared_ptr<json::Object> HouGeoAdapter::AttributeAdapter::getDictionary(int) const
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

    HouGeoAdapter::Topology::~Topology() = default;

    math::Vec3i HouGeoAdapter::VolumePrimitive::getResolution() const
    {
        return math::Vec3i(0);
    }

    std::string HouGeoAdapter::VolumePrimitive::getVisualizationMode() const
    {
        return "smoke";
    }

    real32 HouGeoAdapter::VolumePrimitive::getVisualizationIso() const
    {
        return 0.0f;
    }

    real32 HouGeoAdapter::VolumePrimitive::getVisualizationDensity() const
    {
        return 1.0f;
    }

    HouGeoAdapter::RawDataView HouGeoAdapter::VolumePrimitive::rawData() const
    {
        return {};
    }

    int HouGeoAdapter::PolyPrimitive::numPolys() const
    {
        return 0;
    }

    int HouGeoAdapter::PolyPrimitive::numVertices(int) const
    {
        return 0;
    }

    const int* HouGeoAdapter::PolyPrimitive::vertices(int) const
    {
        return nullptr;
    }

    bool HouGeoAdapter::PolyPrimitive::closed() const
    {
        return false;
    }

    sint64 HouGeoAdapter::pointCount() const
    {
        return pointcount();
    }

    sint64 HouGeoAdapter::vertexCount() const
    {
        return vertexcount();
    }

    sint64 HouGeoAdapter::primitiveCount() const
    {
        return primitivecount();
    }

    std::vector<std::string> HouGeoAdapter::pointAttributeNames() const
    {
        std::vector<std::string> names;
        getPointAttributeNames(names);
        return names;
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::pointAttribute(const std::string& name)
    {
        return getPointAttribute(name);
    }

    std::vector<std::string> HouGeoAdapter::vertexAttributeNames() const
    {
        std::vector<std::string> names;
        getVertexAttributeNames(names);
        return names;
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::vertexAttribute(const std::string& name)
    {
        return getVertexAttribute(name);
    }

    std::vector<std::string> HouGeoAdapter::globalAttributeNames() const
    {
        std::vector<std::string> names;
        getGlobalAttributeNames(names);
        return names;
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::globalAttribute(const std::string& name)
    {
        return getGlobalAttribute(name);
    }

    std::vector<std::string> HouGeoAdapter::primitiveAttributeNames() const
    {
        std::vector<std::string> names;
        getPrimitiveAttributeNames(names);
        return names;
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::primitiveAttribute(
        const std::string& name)
    {
        return getPrimitiveAttribute(name);
    }

    std::vector<std::string> HouGeoAdapter::pointGroupNames() const
    {
        std::vector<std::string> names;
        getPointGroupNames(names);
        return names;
    }

    std::optional<std::vector<bool>> HouGeoAdapter::pointGroupMembership(
        const std::string& name) const
    {
        std::vector<bool> membership;
        if (!getPointGroupMembership(name, membership))
            return std::nullopt;
        return membership;
    }

    std::vector<std::string> HouGeoAdapter::vertexGroupNames() const
    {
        std::vector<std::string> names;
        getVertexGroupNames(names);
        return names;
    }

    std::optional<std::vector<bool>> HouGeoAdapter::vertexGroupMembership(
        const std::string& name) const
    {
        std::vector<bool> membership;
        if (!getVertexGroupMembership(name, membership))
            return std::nullopt;
        return membership;
    }

    std::vector<std::string> HouGeoAdapter::primitiveGroupNames() const
    {
        std::vector<std::string> names;
        getPrimitiveGroupNames(names);
        return names;
    }

    std::optional<std::vector<bool>> HouGeoAdapter::primitiveGroupMembership(
        const std::string& name) const
    {
        std::vector<bool> membership;
        if (!getPrimitiveGroupMembership(name, membership))
            return std::nullopt;
        return membership;
    }

    bool HouGeoAdapter::hasPrimitiveAttribute(const std::string& name) const
    {
        return hasPrimitiveAttributeLegacy(name);
    }

    std::vector<HouGeoAdapter::Primitive::Ptr> HouGeoAdapter::primitives()
    {
        std::vector<Primitive::Ptr> result;
        getPrimitives(result);
        return result;
    }

    HouGeoAdapter::Topology::Ptr HouGeoAdapter::topology()
    {
        return getTopology();
    }

    sint64 HouGeoAdapter::pointcount() const
    {
        return 0;
    }

    sint64 HouGeoAdapter::vertexcount() const
    {
        return 0;
    }

    sint64 HouGeoAdapter::primitivecount() const
    {
        return 0;
    }

    void HouGeoAdapter::getPointAttributeNames(std::vector<std::string>& names) const
    {
        names.clear();
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::getPointAttribute(const std::string&)
    {
        return nullptr;
    }

    void HouGeoAdapter::getVertexAttributeNames(std::vector<std::string>& names) const
    {
        names.clear();
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::getVertexAttribute(const std::string&)
    {
        return nullptr;
    }

    void HouGeoAdapter::getGlobalAttributeNames(std::vector<std::string>& names) const
    {
        names.clear();
    }

    HouGeoAdapter::AttributeAdapter::Ptr HouGeoAdapter::getGlobalAttribute(const std::string&)
    {
        return nullptr;
    }

    void HouGeoAdapter::getPointGroupNames(std::vector<std::string>& names) const
    {
        names.clear();
    }

    bool HouGeoAdapter::getPointGroupMembership(
        const std::string&,
        std::vector<bool>& membership) const
    {
        membership.clear();
        return false;
    }

    void HouGeoAdapter::getVertexGroupNames(std::vector<std::string>& names) const
    {
        names.clear();
    }

    bool HouGeoAdapter::getVertexGroupMembership(
        const std::string&,
        std::vector<bool>& membership) const
    {
        membership.clear();
        return false;
    }

    void HouGeoAdapter::getPrimitiveGroupNames(std::vector<std::string>& names) const
    {
        names.clear();
    }

    bool HouGeoAdapter::getPrimitiveGroupMembership(
        const std::string&,
        std::vector<bool>& membership) const
    {
        membership.clear();
        return false;
    }

    bool HouGeoAdapter::hasPrimitiveAttributeLegacy(const std::string&) const
    {
        return false;
    }

    void HouGeoAdapter::getPrimitives(std::vector<Primitive::Ptr>& primitives)
    {
        primitives.clear();
    }

    HouGeoAdapter::Topology::Ptr HouGeoAdapter::getTopology()
    {
        return nullptr;
    }
}
