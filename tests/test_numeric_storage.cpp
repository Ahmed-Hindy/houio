#include <houio/HouGeoIO.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

houio::sint64 readInt64(const houio::HouGeoAdapter::AttributeAdapter::Ptr& attribute, int index)
{
    houio::HouGeoAdapter::RawPointer::Ptr rawPointer = attribute->getRawPointer();
    if (!rawPointer || !rawPointer->ptr)
    {
        throw std::runtime_error("Int64 attribute has no raw data");
    }
    houio::sint64 value = 0;
    const auto* bytes = static_cast<const unsigned char*>(rawPointer->ptr);
    std::memcpy(&value, bytes + static_cast<size_t>(index) * sizeof(value), sizeof(value));
    return value;
}

int verifyInt64Attribute(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry || geometry->pointcount() != 2)
    {
        return fail("Int64 geometry has unexpected point count");
    }

    houio::HouGeoAdapter::AttributeAdapter::Ptr attribute = geometry->getPointAttribute("large_id");
    if (!attribute
        || attribute->getStorage() != houio::HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64
        || attribute->getTupleSize() != 1 || attribute->getNumElements() != 2)
    {
        return fail("Int64 attribute metadata was not preserved");
    }

    if (readInt64(attribute, 0) != 1099511627776LL
        || readInt64(attribute, 1) != -1099511627777LL)
    {
        return fail("Int64 attribute values were narrowed or changed");
    }
    return 0;
}
}

int main()
{
    houio::HouGeo::Ptr geometry = houio::HouGeo::create();

    houio::Attribute::Ptr positions = houio::Attribute::createV3f();
    positions->appendElement(houio::math::V3f(0.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(1.0f, 0.0f, 0.0f));
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("P", positions));

    houio::Attribute::Ptr identifiers = std::make_shared<houio::Attribute>(1, houio::Attribute::INT64);
    identifiers->appendElement<houio::sint64>(1099511627776LL);
    identifiers->appendElement<houio::sint64>(-1099511627777LL);
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("large_id", identifiers));

    if (const int result = verifyInt64Attribute(geometry); result != 0)
    {
        return result;
    }

    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::xport(&output, geometry, true))
    {
        return fail("failed to export Int64 attribute fixture");
    }

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    return verifyInt64Attribute(houio::HouGeoIO::import(&input));
}
