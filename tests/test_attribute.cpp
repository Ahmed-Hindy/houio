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

    const houio::math::V3f firstValue = attribute->get<houio::math::V3f>(0);
    const houio::math::V3f secondValue = attribute->get<houio::math::V3f>(1);
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
            static_cast<void>(attribute->elementBytes(static_cast<std::size_t>(invalidIndex)));
            return fail("elementBytes accepted an invalid index");
        }
        catch (const std::out_of_range&)
        {
        }
    }

    try
    {
        houio::Attribute invalidAttribute(
            0, houio::Attribute::ComponentType::float32);
        return fail("Attribute accepted zero components");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        static_cast<void>(houio::Attribute::create(
            1,
            houio::Attribute::ComponentType::float32,
            std::span<const std::byte>(),
            1));
        return fail("Attribute::create accepted mismatched non-empty storage");
    }
    catch (const std::invalid_argument&)
    {
    }
    return 0;
}

int verifySafeTypedAccess()
{
    houio::Attribute::Ptr attribute = houio::Attribute::createV3f();
    attribute->appendElement(houio::math::V3f(1.0f, 2.0f, 3.0f));
    attribute->markClean();
    if (attribute->isDirty())
        return fail("markClean did not clear the dirty state");

    attribute->set<houio::math::V3f>(0, houio::math::V3f(4.0f, 5.0f, 6.0f));
    if (!attribute->isDirty()
        || attribute->get<houio::math::V3f>(0) != houio::math::V3f(4.0f, 5.0f, 6.0f))
    {
        return fail("safe typed write did not preserve the value or dirty state");
    }

    try
    {
        static_cast<void>(attribute->get<houio::math::V2f>(0));
        return fail("typed read accepted a value with the wrong tuple width");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        attribute->appendElement(1.0f, 2.0f);
        return fail("appendElement accepted the wrong tuple width");
    }
    catch (const std::invalid_argument&)
    {
    }
    try
    {
        static_cast<void>(attribute->get<houio::math::Vec3i>(0));
        return fail("typed read accepted a same-size tuple with the wrong scalar type");
    }
    catch (const std::invalid_argument&)
    {
    }

    attribute->markClean();
    const std::span<const std::byte> immutable_bytes = attribute->bytes();
    if (attribute->isDirty() || immutable_bytes.size() != attribute->byteSize())
        return fail("immutable byte access changed dirty state or byte count");

    std::span<std::byte> mutable_bytes = attribute->mutableBytes();
    if (!attribute->isDirty() || mutable_bytes.size() != attribute->byteSize())
        return fail("mutable byte access did not mark the attribute dirty");

    attribute->markClean();
    std::span<std::byte> mutable_element = attribute->mutableElementBytes(0);
    if (!attribute->isDirty() || mutable_element.size() != attribute->elementByteSize())
        return fail("mutable element access did not mark the attribute dirty");

    houio::Attribute::Ptr scalar_attribute = houio::Attribute::createFloat();
    try
    {
        scalar_attribute->appendElement<houio::sint32>(1);
        return fail("typed append accepted a same-size scalar with the wrong component type");
    }
    catch (const std::invalid_argument&)
    {
    }
    if (scalar_attribute->numElements() != 0)
        return fail("rejected typed append changed the attribute");
    return 0;
}

int verifyCopyAppendAndDuplicate()
{
    houio::Attribute::Ptr source = houio::Attribute::createInt();
    source->appendElement<houio::sint32>(10);
    source->appendElement<houio::sint32>(20);

    houio::Attribute::Ptr copy = source->copy();
    if (copy->numElements() != 2 || copy->get<houio::sint32>(1) != 20)
        return fail("Attribute copy changed stored values");

    const unsigned int duplicated = copy->duplicateElement(0);
    if (duplicated != 2 || copy->get<houio::sint32>(2) != 10)
        return fail("Attribute duplicateElement changed the source value");

    houio::Attribute::Ptr destination = houio::Attribute::createInt();
    destination->append(*copy);
    if (destination->numElements() != 3
        || destination->get<houio::sint32>(0) != 10
        || destination->get<houio::sint32>(1) != 20
        || destination->get<houio::sint32>(2) != 10)
    {
        return fail("Attribute append changed values or element count");
    }

    try
    {
        destination->append(*houio::Attribute::createFloat());
        return fail("Attribute append accepted incompatible layouts");
    }
    catch (const std::invalid_argument&)
    {
    }

    destination->markClean();
    destination->resize(5);
    if (!destination->isDirty()
        || destination->numElements() != 5
        || destination->get<houio::sint32>(0) != 10
        || destination->get<houio::sint32>(3) != 0
        || destination->get<houio::sint32>(4) != 0)
    {
        return fail("Attribute growth did not preserve values or zero new storage");
    }
    destination->resize(1);
    if (destination->numElements() != 1
        || destination->get<houio::sint32>(0) != 10)
    {
        return fail("Attribute shrink did not preserve the retained prefix");
    }

    const houio::math::Matrix33f matrix = houio::math::Matrix33f::rotation(
        houio::math::Vec3f(0.0f, 0.0f, 1.0f), 0.5f);
    houio::Attribute::Ptr matrix_attribute = houio::Attribute::createM33f();
    matrix_attribute->appendElement(matrix);
    if (matrix_attribute->get<houio::math::Matrix33f>(0) != matrix)
        return fail("Attribute matrix layout did not round-trip exactly");
    return 0;
}
}

int main()
{
    if (const int result = verifyAppendStorage(); result != 0)
        return result;
    if (const int result = verifyBoundsChecks(); result != 0)
        return result;
    if (const int result = verifySafeTypedAccess(); result != 0)
        return result;
    return verifyCopyAppendAndDuplicate();
}
