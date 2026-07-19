#include <houio/HouGeoIO.h>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

bool importRejected(const std::string& sourceText)
{
    try
    {
        std::istringstream source(sourceText);
        return !houio::HouGeoIO::import(&source);
    }
    catch (const std::exception&)
    {
        return true;
    }
    catch (...)
    {
        return true;
    }
}
}

int main()
{
    const std::vector<std::pair<std::string, std::string>> malformedCases = {
        {"odd root key/value array", R"JSON(["pointcount", 0, "vertexcount"])JSON"},
        {"non-string root key", R"JSON([1, 0])JSON"},
        {"duplicate root key", R"JSON(["pointcount", 0, "pointcount", 0])JSON"},
        {"negative geometry count", R"JSON(["pointcount", -1, "vertexcount", 0, "primitivecount", 0])JSON"},
        {"topology count mismatch",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 0, "topology", ["pointref", ["indices", []]]])JSON"},
        {"topology point index out of range",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 0, "topology", ["pointref", ["indices", [1]]]])JSON"},
        {"point group membership count mismatch",
         R"JSON(["pointcount", 2, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["unordered", ["i8", [1]]]]]]])JSON"},
        {"point group non-binary membership",
         R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["unordered", ["i8", [2]]]]]]])JSON"},
        {"ordered group selection",
         R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["ordered", ["i8", [1]]]]]]])JSON"},
        {"unsupported group membership encoding",
         R"JSON(["pointcount", 1, "vertexcount", 0, "primitivecount", 0, "pointgroups", [[["name", "bad"], ["selection", ["unordered", ["i32", [1]]]]]]])JSON"},
        {"direct polygon topology index out of range",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 1, "topology", ["pointref", ["indices", [0]]], "primitives", [[["type", "Poly"], ["vertex", [1], "closed", true]]]])JSON"},
        {"polygon run exceeds topology",
         R"JSON(["pointcount", 1, "vertexcount", 1, "primitivecount", 1, "topology", ["pointref", ["indices", [0]]], "primitives", [[["type", "p_r"], ["s_v", 0, "n_p", 1, "n_v", [2]]]]])JSON"},
    };

    for (const auto& [name, sourceText] : malformedCases)
    {
        if (!importRejected(sourceText))
        {
            return fail("malformed input was accepted: " + name);
        }
    }
    return 0;
}
