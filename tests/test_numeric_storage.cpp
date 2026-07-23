#include <houio/HouGeoIO.h>

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

houio::uword readHalfBits(const houio::HouGeoAdapter::AttributeAdapter::Ptr& attribute, int index)
{
    if (index < 0)
        throw std::out_of_range("Float16 attribute index cannot be negative");
    return attribute->rawData().read<houio::uword>(static_cast<size_t>(index));
}

houio::sint64 readInt64(const houio::HouGeoAdapter::AttributeAdapter::Ptr& attribute, int index)
{
    if (index < 0)
        throw std::out_of_range("Int64 attribute index cannot be negative");
    return attribute->rawData().read<houio::sint64>(static_cast<size_t>(index));
}

int verifyHalfConversion()
{
    for (unsigned int value = 0; value <= 0xffffu; ++value)
    {
        const houio::uword halfBits = static_cast<houio::uword>(value);
        const houio::uword roundTrip = houio::floatToHalfBits(houio::halfBitsToFloat(halfBits));
        if (roundTrip != halfBits)
        {
            return fail("Float16 conversion did not preserve all 16-bit encodings");
        }
    }
    return 0;
}

int verifyHalfAttribute(const houio::HouGeo::Ptr& geometry)
{
    houio::HouGeoAdapter::AttributeAdapter::Ptr attribute = geometry->pointAttribute("half_value");
    if (!attribute
        || attribute->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::float16
        || attribute->tupleSize() != 1 || attribute->elementCount() != 2)
    {
        return fail("Float16 attribute metadata was not preserved");
    }
    if (readHalfBits(attribute, 0) != houio::floatToHalfBits(0.5f)
        || readHalfBits(attribute, 1) != houio::floatToHalfBits(-2.0f))
    {
        return fail("Float16 attribute bits were changed");
    }
    return 0;
}

int verifyInt64Attribute(const houio::HouGeo::Ptr& geometry)
{
    if (!geometry || geometry->pointCount() != 2)
    {
        return fail("Int64 geometry has unexpected point count");
    }

    houio::HouGeoAdapter::AttributeAdapter::Ptr attribute = geometry->pointAttribute("large_id");
    if (!attribute
        || attribute->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::int64
        || attribute->tupleSize() != 1 || attribute->elementCount() != 2)
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
    if (const int result = verifyHalfConversion(); result != 0)
    {
        return result;
    }

    houio::HouGeo::Ptr geometry = houio::HouGeo::create();

    houio::Attribute::Ptr positions = houio::Attribute::createV3f();
    positions->appendElement(houio::math::V3f(0.0f, 0.0f, 0.0f));
    positions->appendElement(houio::math::V3f(1.0f, 0.0f, 0.0f));
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("P", positions));

    houio::Attribute::Ptr identifiers = std::make_shared<houio::Attribute>(1, houio::Attribute::INT64);
    identifiers->appendElement<houio::sint64>(1099511627776LL);
    identifiers->appendElement<houio::sint64>(-1099511627777LL);
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("large_id", identifiers));

    houio::Attribute::Ptr halfValues = std::make_shared<houio::Attribute>(1, houio::Attribute::HALF);
    halfValues->appendElement<houio::uword>(houio::floatToHalfBits(0.5f));
    halfValues->appendElement<houio::uword>(houio::floatToHalfBits(-2.0f));
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("half_value", halfValues));

    if (const int result = verifyInt64Attribute(geometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyHalfAttribute(geometry); result != 0)
    {
        return result;
    }

    std::ostringstream output(std::ios::out | std::ios::binary);
    if (!houio::HouGeoIO::exportGeometry(output, geometry, true))
    {
        return fail("failed to export Int64 attribute fixture");
    }

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::HouGeo::Ptr imported = houio::HouGeoIO::import(input);
    if (const int result = verifyInt64Attribute(imported); result != 0)
    {
        return result;
    }
    if (const int result = verifyHalfAttribute(imported); result != 0)
    {
        return result;
    }

    houio::Geometry::Ptr converted = houio::HouGeoIO::convertToGeometry(
        imported, houio::HouGeoAdapter::Primitive::Ptr());
    houio::Attribute::Ptr convertedHalf = converted ? converted->getAttr("half_value") : nullptr;
    if (!convertedHalf || convertedHalf->elementComponentType() != houio::Attribute::HALF
        || convertedHalf->numElements() != 2)
    {
        return fail("simplified conversion did not preserve Float16 attribute storage");
    }
    return 0;
}
