#include <houio/HouGeoIO.h>

#include "TestSupport.h"

#include <iostream>
#include <sstream>
#include <string>

namespace
{
using houio::test::fail;

houio::ubyte readUInt8(const houio::HouGeoAdapter::AttributeAdapter::Ptr& attribute, int index)
{
    if (index < 0)
        throw std::out_of_range("UInt8 attribute index cannot be negative");
    return attribute->rawData().read<houio::ubyte>(static_cast<size_t>(index));
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

houio::real64 readFloat64(const houio::HouGeoAdapter::AttributeAdapter::Ptr& attribute, int index)
{
    if (index < 0)
        throw std::out_of_range("Float64 attribute index cannot be negative");
    return attribute->rawData().read<houio::real64>(static_cast<size_t>(index));
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

int verifyUInt8Attribute(const houio::HouGeo::Ptr& geometry)
{
    houio::HouGeoAdapter::AttributeAdapter::Ptr attribute = geometry->pointAttribute("mask");
    if (!attribute
        || attribute->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::uint8
        || attribute->tupleSize().value() != 1 || attribute->elementCount() != 2)
    {
        return fail("UInt8 attribute metadata was not preserved");
    }
    if (readUInt8(attribute, 0) != static_cast<houio::ubyte>(128)
        || readUInt8(attribute, 1) != static_cast<houio::ubyte>(255))
    {
        return fail("UInt8 attribute values were signed, narrowed, or changed");
    }
    return 0;
}

int verifyHalfAttribute(const houio::HouGeo::Ptr& geometry)
{
    houio::HouGeoAdapter::AttributeAdapter::Ptr attribute = geometry->pointAttribute("half_value");
    if (!attribute
        || attribute->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::float16
        || attribute->tupleSize().value() != 1 || attribute->elementCount() != 2)
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
        || attribute->tupleSize().value() != 1 || attribute->elementCount() != 2)
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

int verifyFloat64Attribute(const houio::HouGeo::Ptr& geometry)
{
    houio::HouGeoAdapter::AttributeAdapter::Ptr attribute = geometry->pointAttribute("precise_value");
    if (!attribute
        || attribute->storage() != houio::HouGeoAdapter::AttributeAdapter::Storage::float64
        || attribute->tupleSize().value() != 1 || attribute->elementCount() != 2)
    {
        return fail("Float64 attribute metadata was not preserved");
    }

    if (readFloat64(attribute, 0) != 1.0000000000000002
        || readFloat64(attribute, 1) != -123456789.125)
    {
        return fail("Float64 attribute values were narrowed or changed");
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

    houio::Attribute::Ptr maskValues = std::make_shared<houio::Attribute>(
        1, houio::Attribute::ComponentType::uint8);
    maskValues->appendElement<houio::ubyte>(static_cast<houio::ubyte>(128));
    maskValues->appendElement<houio::ubyte>(static_cast<houio::ubyte>(255));
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("mask", maskValues));

    houio::Attribute::Ptr identifiers = std::make_shared<houio::Attribute>(
        1, houio::Attribute::ComponentType::int64);
    identifiers->appendElement<houio::sint64>(1099511627776LL);
    identifiers->appendElement<houio::sint64>(-1099511627777LL);
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("large_id", identifiers));

    houio::Attribute::Ptr halfValues = std::make_shared<houio::Attribute>(
        1, houio::Attribute::ComponentType::float16);
    halfValues->appendElement<houio::uword>(houio::floatToHalfBits(0.5f));
    halfValues->appendElement<houio::uword>(houio::floatToHalfBits(-2.0f));
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("half_value", halfValues));

    houio::Attribute::Ptr preciseValues = std::make_shared<houio::Attribute>(
        1, houio::Attribute::ComponentType::float64);
    preciseValues->appendElement<houio::real64>(1.0000000000000002);
    preciseValues->appendElement<houio::real64>(-123456789.125);
    geometry->setPointAttribute(std::make_shared<houio::HouGeo::HouAttribute>("precise_value", preciseValues));

    if (const int result = verifyUInt8Attribute(geometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyInt64Attribute(geometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyHalfAttribute(geometry); result != 0)
    {
        return result;
    }
    if (const int result = verifyFloat64Attribute(geometry); result != 0)
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
    if (const int result = verifyUInt8Attribute(imported); result != 0)
    {
        return result;
    }
    if (const int result = verifyInt64Attribute(imported); result != 0)
    {
        return result;
    }
    if (const int result = verifyHalfAttribute(imported); result != 0)
    {
        return result;
    }
    if (const int result = verifyFloat64Attribute(imported); result != 0)
    {
        return result;
    }

    houio::Geometry::Ptr converted = houio::HouGeoIO::convertToGeometry(
        imported, houio::HouGeoAdapter::Primitive::Ptr());
    houio::Attribute::Ptr convertedUInt8 = converted ? converted->attribute("mask") : nullptr;
    if (!convertedUInt8
        || convertedUInt8->elementComponentType() != houio::Attribute::ComponentType::uint8
        || convertedUInt8->numElements() != 2
        || convertedUInt8->get<houio::ubyte>(0) != static_cast<houio::ubyte>(128)
        || convertedUInt8->get<houio::ubyte>(1) != static_cast<houio::ubyte>(255))
    {
        return fail("simplified conversion did not preserve UInt8 attribute storage");
    }

    houio::Attribute::Ptr convertedHalf = converted ? converted->attribute("half_value") : nullptr;
    if (!convertedHalf
        || convertedHalf->elementComponentType() != houio::Attribute::ComponentType::float16
        || convertedHalf->numElements() != 2)
    {
        return fail("simplified conversion did not preserve Float16 attribute storage");
    }

    houio::Attribute::Ptr convertedFloat64 = converted ? converted->attribute("precise_value") : nullptr;
    if (!convertedFloat64
        || convertedFloat64->elementComponentType() != houio::Attribute::ComponentType::float64
        || convertedFloat64->numElements() != 2
        || convertedFloat64->get<houio::real64>(0) != 1.0000000000000002
        || convertedFloat64->get<houio::real64>(1) != -123456789.125)
    {
        return fail("simplified conversion did not preserve Float64 attribute storage");
    }
    return 0;
}
