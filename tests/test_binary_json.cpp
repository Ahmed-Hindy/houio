#include <houio/json.h>

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
}

int main()
{
    std::string binaryData;
    const unsigned char bytes[] = {
        0x7f, 0x4e, 0x53, 0x4a, 0x62,  // Binary JSON magic.
        0x5b,                          // Root array begin.
        0x40, 0x11, 0x02, 0x04, 0x06,  // Uniform int8 array: [4, 6].
        0x5d,                          // Root array end.
    };
    binaryData.assign(reinterpret_cast<const char*>(bytes), sizeof(bytes));

    std::istringstream input(binaryData, std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(&input, &reader))
    {
        return fail("failed to parse uniform int8 binary JSON");
    }

    houio::json::ArrayPtr root = reader.getRoot().asArray();
    if (!root || root->size() != 1)
    {
        return fail("unexpected root array");
    }

    houio::json::ArrayPtr values = root->getArray(0);
    if (!values || values->size() != 2)
    {
        return fail("unexpected uniform int8 array size");
    }
    if (values->get<int>(0) != 4 || values->get<int>(1) != 6)
    {
        return fail("uniform int8 values were not widened correctly");
    }

    return 0;
}
