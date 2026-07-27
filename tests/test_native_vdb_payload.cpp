#include "TestSupport.h"

#include <houio/GeometryIO.h>
#include <houio/NativeVdbPayload.h>

#include <filesystem>
#include <memory>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc != 2)
        return houio::test::fail("expected one fixture path");

    const auto geometry = houio::GeometryIO::readHouGeo(std::filesystem::path(argv[1]));
    if (!geometry || geometry.value->primitiveCount() != 1)
        return houio::test::fail("fixture failed to load");

    const auto primitive = std::dynamic_pointer_cast<houio::HouGeo::HouVdb>(
        geometry.value->primitives().front());
    if (!primitive || !primitive->serializedPayload())
        return houio::test::fail("fixture has no native payload");

    const auto stream = houio::NativeVdbPayload::decode(primitive->serializedPayload());
    if (!stream || stream.value.size() <= 4
        || !houio::NativeVdbPayload::hasOpenVdbMagic(stream.value))
    {
        return houio::test::fail("payload is not an OpenVDB stream");
    }

    const auto payload = houio::NativeVdbPayload::encode(stream.value);
    const auto rebuilt = payload
        ? houio::NativeVdbPayload::decode(payload.value)
        : houio::GeometryReadResult<std::vector<houio::ubyte>>{};
    if (!rebuilt || rebuilt.value != stream.value)
        return houio::test::fail("retile changed stream bytes");

    return 0;
}
