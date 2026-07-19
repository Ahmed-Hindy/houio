#include <houio/Attribute.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

int verifyAppendStorage()
{
    houio::Attribute::Ptr attribute = houio::Attribute::createV3f();
    const unsigned int firstIndex = attribute->appendElement(houio::math::V3f(1.0f, 2.0f, 3.0f));
    const unsigned int secondIndex = attribute->appendElement(houio::math::V3f(4.0f, 5.0f, 6.0f));

    if (firstIndex != 0 || secondIndex != 1 || attribute->numElements() != 2)
    {
        return fail("appendElement returned incorrect indices or count");
    }

    houio::math::V3f firstValue;
    houio::math::V3f secondValue;
    std::memcpy(&firstValue, attribute->getRawPointer(0), sizeof(firstValue));
    std::memcpy(&secondValue, attribute->getRawPointer(1), sizeof(secondValue));
    if (firstValue.x != 1.0f || firstValue.y != 2.0f || firstValue.z != 3.0f
        || secondValue.x != 4.0f || secondValue.y != 5.0f || secondValue.z != 6.0f)
    {
        return fail("appendElement did not preserve tuple bytes");
    }
    return 0;
}

int verifyBoundsChecks()
{
    houio::Attribute::Ptr attribute = houio::Attribute::createFloat();
    attribute->appendElement(1.0f);

    for (const int invalidIndex : {-1, 1})
    {
        try
        {
            attribute->getRawPointer(invalidIndex);
            return fail("getRawPointer accepted an invalid index");
        }
        catch (const std::out_of_range&)
        {
        }
    }

    try
    {
        houio::Attribute invalidAttribute(0, houio::Attribute::FLOAT);
        return fail("Attribute accepted zero components");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        houio::Attribute::create(1, houio::Attribute::FLOAT, nullptr, 1);
        return fail("Attribute::create accepted null non-empty storage");
    }
    catch (const std::invalid_argument&)
    {
    }
    return 0;
}
}

int main()
{
    if (const int result = verifyAppendStorage(); result != 0)
    {
        return result;
    }
    return verifyBoundsChecks();
}
