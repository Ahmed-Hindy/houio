#include <houio/json.h>

#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
constexpr unsigned char binaryMagic[] = {0x7f, 0x4e, 0x53, 0x4a, 0x62};

int fail(const std::string& message)
{
    std::cerr << "error: " << message << '\n';
    return 1;
}

std::string binaryDocument(std::initializer_list<unsigned char> payload)
{
    std::string result(reinterpret_cast<const char*>(binaryMagic), sizeof(binaryMagic));
    result.append(reinterpret_cast<const char*>(payload.begin()), payload.size());
    return result;
}

int expectParseFailure(const std::string& binaryData, const houio::json::ParserLimits& limits,
                       const std::string& description)
{
    std::istringstream input(binaryData, std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser(limits);
    try
    {
        parser.parse(&input, &reader);
    }
    catch (const std::exception&)
    {
        return 0;
    }
    return fail(description + " was not rejected");
}

int expectDiagnosticFailure(const std::string& binaryData, houio::DiagnosticCategory category,
                            houio::sint64 byteOffset, const std::string& description)
{
    std::istringstream input(binaryData, std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    houio::DiagnosticList diagnostics;
    if (parser.parse(&input, &reader, &diagnostics))
    {
        return fail(description + " unexpectedly parsed");
    }
    if (diagnostics.size() != 1)
    {
        return fail(description + " did not produce exactly one diagnostic");
    }
    const houio::Diagnostic& diagnostic = diagnostics.front();
    if (diagnostic.severity != houio::DiagnosticSeverity::error
        || diagnostic.category != category || diagnostic.byteOffset != byteOffset
        || diagnostic.message.empty())
    {
        return fail(description + " produced incorrect diagnostic metadata");
    }
    return 0;
}

int verifyUniformInt8()
{
    const std::string binaryData = binaryDocument({
        0x5b,                          // Root array begin.
        0x40, 0x11, 0x02, 0x04, 0x06,  // Uniform int8 array: [4, 6].
        0x5d,                          // Root array end.
    });

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
    if (!values || values->size() != 2 || values->get<int>(0) != 4 || values->get<int>(1) != 6)
    {
        return fail("uniform int8 values were not widened correctly");
    }
    return 0;
}

int verifyUniformBoolAcrossWords()
{
    const std::string binaryData = binaryDocument({
        0x5b,
        0x40, 0x10, 0x28,
        0x01, 0x00, 0x00, 0x80,
        0x81, 0x00, 0x00, 0x00,
        0x5d,
    });

    std::istringstream input(binaryData, std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(&input, &reader))
    {
        return fail("failed to parse multiword uniform bool array");
    }

    houio::json::ArrayPtr root = reader.getRoot().asArray();
    houio::json::ArrayPtr values = root ? root->getArray(0) : houio::json::ArrayPtr();
    if (!values || values->size() != 40 || !values->get<bool>(0) || values->get<bool>(1)
        || !values->get<bool>(31) || !values->get<bool>(32) || !values->get<bool>(39))
    {
        return fail("uniform bool values were decoded at incorrect word offsets");
    }
    return 0;
}

int verifyTruncatedInput()
{
    houio::json::ParserLimits limits;
    if (const int result = expectParseFailure(
            binaryDocument({0x5b, 0x13, 0x01, 0x02}), limits, "truncated int32 value");
        result != 0)
    {
        return result;
    }
    return expectParseFailure(
        binaryDocument({0x5b, 0x40, 0x11, 0x02, 0x04}), limits, "truncated uniform array");
}

int verifyLengthValidation()
{
    houio::json::ParserLimits limits;
    const std::string negativeStringLength = binaryDocument({
        0x5b, 0x27, 0xf8,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    });
    if (const int result = expectParseFailure(negativeStringLength, limits, "negative string length");
        result != 0)
    {
        return result;
    }

    limits.maxStringBytes = 3;
    if (const int result = expectParseFailure(
            binaryDocument({0x5b, 0x27, 0x04, 't', 'e', 's', 't', 0x5d}), limits,
            "oversized binary string");
        result != 0)
    {
        return result;
    }

    limits = houio::json::ParserLimits();
    limits.maxUniformArrayElements = 1;
    return expectParseFailure(
        binaryDocument({0x5b, 0x40, 0x11, 0x02, 0x04, 0x06, 0x5d}), limits,
        "oversized uniform array");
}

int verifyTokenAndNestingValidation()
{
    houio::json::ParserLimits limits;
    if (const int result = expectParseFailure(
            binaryDocument({0x5b, 0x26, 0x00, 0x5d}), limits, "undefined string token");
        result != 0)
    {
        return result;
    }

    limits.maxNestingDepth = 2;
    return expectParseFailure(
        binaryDocument({0x5b, 0x5b, 0x5b, 0x5d, 0x5d, 0x5d}), limits,
        "excessive nesting");
}

int verifyStructuredDiagnostics()
{
    if (const int result = expectDiagnosticFailure(
            binaryDocument({0x5b, 0x13, 0x01, 0x02}),
            houio::DiagnosticCategory::malformed_input, 9, "truncated int32 diagnostic");
        result != 0)
    {
        return result;
    }
    if (const int result = expectDiagnosticFailure(
            binaryDocument({0x5b, 0x26, 0x00, 0x5d}),
            houio::DiagnosticCategory::malformed_input, 6, "undefined token diagnostic");
        result != 0)
    {
        return result;
    }
    return expectDiagnosticFailure(
        binaryDocument({0x5b, 0x40, 0x18, 0x00, 0x5d}),
        houio::DiagnosticCategory::unsupported_input, 7, "unsupported array diagnostic");
}

int verifyParserReuse()
{
    houio::json::Parser parser;
    const std::string binaryData = binaryDocument({0x5b, 0x11, 0x05, 0x5d});
    for (int parseIndex = 0; parseIndex < 2; ++parseIndex)
    {
        std::istringstream input(binaryData, std::ios::in | std::ios::binary);
        houio::json::JSONReader reader;
        if (!parser.parse(&input, &reader))
        {
            return fail("parser reuse failed");
        }
        houio::json::ArrayPtr root = reader.getRoot().asArray();
        if (!root || root->size() != 1 || root->get<int>(0) != 5)
        {
            return fail("parser reuse retained invalid document state");
        }
    }
    return 0;
}

int verifyWriterValidation()
{
    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(&output);
    try
    {
        writer.writeLength(-1);
        return fail("writer accepted a negative length");
    }
    catch (const std::length_error&)
    {
    }

    try
    {
        writer.jsonUniformArray<houio::sint32>(nullptr, 1);
        return fail("writer accepted null uniform-array data");
    }
    catch (const std::invalid_argument&)
    {
    }
    return 0;
}
}

int main()
{
    if (const int result = verifyUniformInt8(); result != 0)
    {
        return result;
    }
    if (const int result = verifyUniformBoolAcrossWords(); result != 0)
    {
        return result;
    }
    if (const int result = verifyTruncatedInput(); result != 0)
    {
        return result;
    }
    if (const int result = verifyLengthValidation(); result != 0)
    {
        return result;
    }
    if (const int result = verifyTokenAndNestingValidation(); result != 0)
    {
        return result;
    }
    if (const int result = verifyStructuredDiagnostics(); result != 0)
    {
        return result;
    }
    if (const int result = verifyParserReuse(); result != 0)
    {
        return result;
    }
    return verifyWriterValidation();
}
