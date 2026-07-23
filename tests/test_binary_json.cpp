#include <houio/json.h>

#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
constexpr unsigned char binaryMagic[] = {0x7f, 0x4e, 0x53, 0x4a, 0x62};

class NonSeekableBuffer : public std::stringbuf
{
public:
    explicit NonSeekableBuffer(const std::string& value) : std::stringbuf(value, std::ios::in)
    {
    }

protected:
    pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }

    pos_type seekpos(pos_type, std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }
};

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
        if (parser.parse(input, reader))
            return fail(description + " was unexpectedly accepted");
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
    if (const int result = expectParseFailure(
            R"JSON(["test"])JSON", limits, "oversized ASCII string");
        result != 0)
    {
        return result;
    }
    if (const int result = expectParseFailure(
            R"JSON(["a\qb"])JSON", limits, "oversized escaped ASCII string");
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

int verifyFloat16Tokens()
{
    const std::string scalarData = binaryDocument({0x5b, 0x18, 0x00, 0x3c, 0x5d});
    std::istringstream scalarInput(scalarData, std::ios::in | std::ios::binary);
    houio::json::JSONReader scalarReader;
    houio::json::Parser parser;
    if (!parser.parse(&scalarInput, &scalarReader)
        || scalarReader.getRoot().asArray()->get<houio::real32>(0) != 1.0f)
    {
        return fail("standalone Float16 token was not decoded");
    }

    const houio::uword halfValues[] = {
        houio::floatToHalfBits(0.5f),
        houio::floatToHalfBits(-2.0f),
        houio::floatToHalfBits(3.25f),
    };
    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(&output);
    writer.jsonMagic();
    writer.jsonBeginArray();
    writer.jsonUniformArrayReal16(halfValues, 3);
    writer.jsonEndArray();

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    if (!parser.parse(&input, &reader))
    {
        return fail("failed to parse Float16 uniform array");
    }
    houio::json::ArrayPtr values = reader.getRoot().asArray()->getArray(0);
    if (!values || values->size() != 3 || values->get<houio::real32>(0) != 0.5f
        || values->get<houio::real32>(1) != -2.0f || values->get<houio::real32>(2) != 3.25f)
    {
        return fail("Float16 uniform array values changed");
    }
    return 0;
}

int verifyWideScalarFidelity()
{
    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(&output);
    writer.jsonMagic();
    writer.jsonBeginArray();
    writer.jsonInt64(1099511627779LL);
    writer.jsonReal64(1.0 / 3.0);
    writer.jsonEndArray();

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(&input, &reader))
    {
        return fail("failed to parse wide scalar fixture");
    }
    houio::json::ArrayPtr values = reader.getRoot().asArray();
    if (!values || values->size() != 2 || values->get<houio::sint64>(0) != 1099511627779LL
        || values->get<houio::real64>(1) != 1.0 / 3.0)
    {
        return fail("standalone Int64 or Real64 scalar was narrowed");
    }
    return 0;
}

int verifyRootAndClosingStateSafety()
{
    const std::vector<std::string> scalarDocuments = {
        "1",
        "-2",
        "1.5",
        "true",
        "false",
        "null",
        "\"text\"",
    };
    for (const std::string& document : scalarDocuments)
    {
        std::istringstream input(document);
        houio::json::JSONReader reader;
        houio::json::Parser parser;
        if (!parser.parse(&input, &reader))
        {
            return fail("valid scalar root at exact EOF was rejected");
        }
    }

    const std::string binaryScalar = binaryDocument({0x11, 0x05});
    std::istringstream binaryScalarInput(binaryScalar, std::ios::in | std::ios::binary);
    houio::json::JSONReader binaryScalarReader;
    houio::json::Parser binaryScalarParser;
    if (!binaryScalarParser.parse(&binaryScalarInput, &binaryScalarReader)
        || binaryScalarReader.getRoot().as<houio::sint32>() != 5)
    {
        return fail("valid binary scalar root was rejected");
    }

    NonSeekableBuffer scalarBuffer("1");
    std::istream nonSeekableScalarInput(&scalarBuffer);
    houio::json::JSONReader scalarReader;
    houio::json::Parser scalarParser;
    if (!scalarParser.parse(&nonSeekableScalarInput, &scalarReader)
        || scalarReader.getRoot().as<houio::sint64>() != 1)
    {
        return fail("non-seekable scalar root at exact EOF was rejected");
    }

    const std::vector<std::string> malformed = {
        "]",
        "}",
        "[}",
        "{]",
        "[1}",
        "{\"a\":1]",
        "{\"a\":}",
        "[1,]",
        "{\"a\":1,}",
        "[,]",
        ",",
        ":",
    };
    for (const std::string& document : malformed)
    {
        std::istringstream input(document);
        houio::json::JSONReader reader;
        houio::json::Parser parser;
        houio::DiagnosticList diagnostics;
        if (parser.parse(&input, &reader, &diagnostics) || diagnostics.empty()
            || diagnostics.front().category != houio::DiagnosticCategory::malformed_input)
        {
            return fail("invalid root, separator, or closing token was not rejected safely");
        }
    }
    return 0;
}

int verifyInputBudgetAndTrailingData()
{
    const std::string binaryData = binaryDocument({0x5b, 0x11, 0x05, 0x5d});
    houio::json::ParserLimits limits;
    limits.maxInputBytes = static_cast<houio::sint64>(binaryData.size() - 1);
    if (const int result = expectParseFailure(binaryData, limits, "seekable input byte limit"); result != 0)
    {
        return result;
    }

    NonSeekableBuffer buffer(binaryData);
    std::istream nonSeekableInput(&buffer);
    houio::json::JSONReader reader;
    houio::json::Parser streamingParser(limits);
    try
    {
        if (streamingParser.parse(nonSeekableInput, reader))
            return fail("streaming input byte limit was not rejected");
    }
    catch (const std::exception&)
    {
    }

    std::string trailingBinary = binaryData;
    trailingBinary.push_back('\0');
    limits = houio::json::ParserLimits();
    if (const int result = expectParseFailure(trailingBinary, limits, "trailing binary data"); result != 0)
    {
        return result;
    }

    std::istringstream asciiInput("[1]  \n// trailing comment\n");
    houio::json::JSONReader asciiReader;
    houio::json::Parser asciiParser;
    if (!asciiParser.parse(&asciiInput, &asciiReader))
    {
        return fail("valid trailing ASCII whitespace and comments were rejected");
    }

    std::istringstream invalidAsciiInput("[1] 2");
    houio::json::JSONReader invalidAsciiReader;
    houio::DiagnosticList diagnostics;
    if (asciiParser.parse(&invalidAsciiInput, &invalidAsciiReader, &diagnostics) || diagnostics.empty())
    {
        return fail("trailing ASCII value was not rejected");
    }

    limits.maxInputBytes = -1;
    try
    {
        houio::json::Parser invalidParser(limits);
        return fail("negative input byte limit was accepted");
    }
    catch (const std::invalid_argument&)
    {
    }
    return 0;
}

int verifyArrayAndValueSafety()
{
    const std::string binaryData = binaryDocument({
        0x5b,
        0x40, 0x11, 0x02, 0x04, 0x06,
        0x5d,
    });
    std::istringstream input(binaryData, std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(&input, &reader))
    {
        return fail("failed to parse array safety fixture");
    }
    houio::json::ArrayPtr values = reader.getRoot().asArray()->getArray(0);
    try
    {
        static_cast<void>(values->get<int>(-1));
        return fail("negative array index was accepted");
    }
    catch (const std::out_of_range&)
    {
    }
    try
    {
        static_cast<void>(values->get<int>(2));
        return fail("past-end array index was accepted");
    }
    catch (const std::out_of_range&)
    {
    }

    houio::sint64 integerValue = 0x102030405060708LL;
    char integerBytes[sizeof(integerValue)]{};
    houio::json::Value::create<houio::sint64>(integerValue).cpyTo(integerBytes);
    houio::sint64 integerCopy = 0;
    std::memcpy(&integerCopy, integerBytes, sizeof(integerCopy));
    if (integerCopy != integerValue)
    {
        return fail("int64 Value::cpyTo changed the value");
    }

    try
    {
        char destination = 0;
        houio::json::Value::create<std::string>("text").cpyTo(&destination);
        return fail("string Value::cpyTo was accepted");
    }
    catch (const std::invalid_argument&)
    {
    }

    std::istringstream duplicateMap("{\"a\":1,\"a\":2}");
    houio::json::JSONReader duplicateReader;
    houio::DiagnosticList diagnostics;
    if (parser.parse(&duplicateMap, &duplicateReader, &diagnostics) || diagnostics.empty())
    {
        return fail("duplicate native map key was not rejected");
    }
    return 0;
}

int verifyDeclaredPayloadValidation()
{
    houio::json::ParserLimits limits;
    limits.maxUniformArrayElements = std::numeric_limits<houio::sint64>::max();
    limits.maxInputBytes = 1024;
    return expectParseFailure(
        binaryDocument({
            0x5b, 0x40, 0x14, 0xf8,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
            0x5d,
        }),
        limits, "overflowing declared uniform-array payload");
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
        binaryDocument({0x5b, 0x40, 0x22, 0x00, 0x5d}),
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
    houio::json::BinaryWriter writer(output);
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

    try
    {
        houio::json::BinaryWriter invalid_binary_writer(
            static_cast<std::ostream*>(nullptr));
        return fail("binary writer accepted a null output stream");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        houio::json::ASCIIWriter invalid_ascii_writer(
            static_cast<std::ostream*>(nullptr));
        return fail("ASCII writer accepted a null output stream");
    }
    catch (const std::invalid_argument&)
    {
    }

    try
    {
        houio::json::JSONWriter invalid_json_writer(
            static_cast<std::ostream*>(nullptr));
        return fail("JSON writer accepted a null output stream");
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
    if (const int result = verifyFloat16Tokens(); result != 0)
    {
        return result;
    }
    if (const int result = verifyWideScalarFidelity(); result != 0)
    {
        return result;
    }
    if (const int result = verifyRootAndClosingStateSafety(); result != 0)
    {
        return result;
    }
    if (const int result = verifyInputBudgetAndTrailingData(); result != 0)
    {
        return result;
    }
    if (const int result = verifyArrayAndValueSafety(); result != 0)
    {
        return result;
    }
    if (const int result = verifyDeclaredPayloadValidation(); result != 0)
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
