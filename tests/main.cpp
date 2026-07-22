#include <houio/json.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
    const std::string document =
        R"JSON(["pointcount",0,"vertexcount",0,"primitivecount",0])JSON";
    std::istringstream input(document);
    std::ostringstream output;
    houio::json::JSONLogger logger(output);
    houio::json::Parser parser;
    houio::DiagnosticList diagnostics;

    if (!parser.parse(&input, &logger, &diagnostics))
    {
        return fail("JSON logger rejected a valid modern geometry header");
    }
    if (!diagnostics.empty())
    {
        return fail("JSON logger produced diagnostics for valid input");
    }

    const std::string log = output.str();
    const std::vector<std::string> expectedEntries = {
        "jsonBeginArray",
        "jsonString pointcount",
        "jsonString vertexcount",
        "jsonString primitivecount",
        "jsonEndArray",
    };
    for (const std::string& expectedEntry : expectedEntries)
    {
        if (log.find(expectedEntry) == std::string::npos)
        {
            return fail("JSON logger output is missing: " + expectedEntry);
        }
    }

    return 0;
}
