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
    const std::string document =
        R"JSON(["pointcount",0,"vertexcount",0,"primitivecount",0])JSON";
    std::istringstream input(document);
    std::ostringstream output;
    houio::json::JSONLogger logger(output);
    houio::json::Parser parser;
    houio::DiagnosticList diagnostics;

    if (!parser.parse(input, logger, diagnostics))
    {
        return fail("JSON logger rejected a valid modern geometry header");
    }
    if (!diagnostics.empty())
    {
        return fail("JSON logger produced diagnostics for valid input");
    }

    const std::string log = output.str();
    const std::string expectedLog =
        "jsonBeginArray\n"
        "\tjsonString pointcount\n"
        "\tjsonInt64 0\n"
        "\tjsonString vertexcount\n"
        "\tjsonInt64 0\n"
        "\tjsonString primitivecount\n"
        "\tjsonInt64 0\n"
        "jsonEndArray\n";
    if (log != expectedLog)
    {
        return fail("JSON logger output does not match the expected sequence:\n" + log);
    }

    return 0;
}
