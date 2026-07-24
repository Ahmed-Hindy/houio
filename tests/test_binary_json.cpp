#include <houio/json.h>

#include <array>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

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

std::string binaryDocument(const std::string& payload)
{
    std::string result(reinterpret_cast<const char*>(binaryMagic), sizeof(binaryMagic));
    result.append(payload);
    return result;
}

template<typename T>
void appendNative(std::string& destination, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const std::size_t offset = destination.size();
    destination.resize(offset + sizeof(T));
    std::memcpy(destination.data() + offset, &value, sizeof(T));
}

void appendBinaryString(std::string& destination, const std::string& value, unsigned char lengthId)
{
    destination.push_back(static_cast<char>(lengthId));
    const houio::sint64 size = static_cast<houio::sint64>(value.size());
    if (lengthId == 0xf2)
        appendNative(destination, static_cast<houio::uword>(size));
    else if (lengthId == 0xf4)
        appendNative(destination, static_cast<houio::uint32>(size));
    else if (lengthId == 0xf8)
        appendNative(destination, size);
    else
        throw std::invalid_argument("appendBinaryString requires a multi-byte length id");
    destination.append(value);
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
    if (parser.parse(input, reader, diagnostics))
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

int verifyEveryScalarToken()
{
    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(output);
    writer.jsonBeginArray();
    writer.jsonNull();
    writer.writeId(houio::json::Token::JID_BOOL);
    writer.write<houio::sbyte>(0);
    writer.writeId(houio::json::Token::JID_BOOL);
    writer.write<houio::sbyte>(-1);
    writer.writeId(houio::json::Token::JID_FALSE);
    writer.writeId(houio::json::Token::JID_TRUE);
    writer.writeId(houio::json::Token::JID_INT8);
    writer.write<houio::sbyte>(-8);
    writer.writeId(houio::json::Token::JID_INT16);
    writer.write<houio::sword>(-1234);
    writer.writeId(houio::json::Token::JID_INT32);
    writer.write<houio::sint32>(-12345678);
    writer.writeId(houio::json::Token::JID_INT64);
    writer.write<houio::sint64>(0x102030405060708LL);
    writer.writeId(houio::json::Token::JID_REAL16);
    writer.write<houio::uword>(houio::floatToHalfBits(1.5f));
    writer.writeId(houio::json::Token::JID_REAL32);
    writer.write<houio::real32>(-2.25f);
    writer.writeId(houio::json::Token::JID_REAL64);
    writer.write<houio::real64>(1.0 / 3.0);
    writer.writeId(houio::json::Token::JID_UINT8);
    writer.write<houio::ubyte>(250);
    writer.writeId(houio::json::Token::JID_UINT16);
    writer.write<houio::uword>(65000);
    writer.jsonString("token");
    writer.jsonEndArray();

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(input, reader))
        return fail("failed to parse the complete scalar-token fixture");

    const houio::json::ArrayPtr values = reader.root().asArray();
    if (!values || values->size() != 15 || !values->value(0).isNull()
        || values->get<bool>(1) || !values->get<bool>(2)
        || values->get<bool>(3) || !values->get<bool>(4)
        || values->get<int>(5) != -8 || values->get<int>(6) != -1234
        || values->get<houio::sint32>(7) != -12345678
        || values->get<houio::sint64>(8) != 0x102030405060708LL
        || values->get<houio::real32>(9) != 1.5f
        || values->get<houio::real32>(10) != -2.25f
        || values->get<houio::real64>(11) != 1.0 / 3.0
        || values->get<int>(12) != 250 || values->get<int>(13) != 65000
        || values->get<std::string>(14) != "token")
    {
        return fail("one or more supported scalar tokens changed value or position");
    }
    return 0;
}

int verifyNullTreeRoundTrip()
{
    houio::json::ArrayPtr source = houio::json::Array::create();
    source->append(houio::json::Value{});
    houio::json::ObjectPtr object = houio::json::Object::create();
    object->append("missing", houio::json::Value{});
    source->append(object);
    source->appendValue<houio::sint32>(7);

    for (const bool binary : {false, true})
    {
        std::ostringstream output(
            binary ? (std::ios::out | std::ios::binary) : std::ios::out);
        houio::json::JSONWriter writer(output, binary);
        if (!writer.write(source))
            return fail("JSONWriter reported failure while writing null values");

        std::istringstream input(
            output.str(), binary ? (std::ios::in | std::ios::binary) : std::ios::in);
        houio::json::JSONReader reader;
        houio::json::Parser parser;
        if (!parser.parse(input, reader))
            return fail("failed to parse a null-preserving JSON tree round trip");

        const houio::json::ArrayPtr result = reader.root().asArray();
        const houio::json::ObjectPtr result_object = result ? result->object(1) : nullptr;
        if (!result || result->size() != 3 || !result->value(0).isNull()
            || !result_object || !result_object->contains("missing")
            || !result_object->value("missing").isNull()
            || result->get<houio::sint32>(2) != 7)
        {
            return fail("null array elements or object values were not preserved");
        }

        houio::json::Value null_value;
        std::ostringstream null_output(
            binary ? (std::ios::out | std::ios::binary) : std::ios::out);
        houio::json::JSONWriter null_writer(null_output, binary);
        if (!null_writer.write(null_value))
            return fail("JSONWriter reported failure for a top-level null");
        std::istringstream null_input(
            null_output.str(), binary ? (std::ios::in | std::ios::binary) : std::ios::in);
        houio::json::JSONReader null_reader;
        houio::json::Parser null_parser;
        if (!null_parser.parse(null_input, null_reader) || !null_reader.root().isNull())
            return fail("top-level null did not round-trip");
    }
    return 0;
}

int verifyIntegerLengthEncodings()
{
    const std::vector<houio::sint64> lengths = {
        0,
        0xf0,
        0xf1,
        static_cast<houio::sint64>(std::numeric_limits<houio::uword>::max()),
        static_cast<houio::sint64>(std::numeric_limits<houio::uword>::max()) + 1,
        static_cast<houio::sint64>(std::numeric_limits<houio::uint32>::max()),
        static_cast<houio::sint64>(std::numeric_limits<houio::uint32>::max()) + 1,
        std::numeric_limits<houio::sint64>::max(),
    };

    for (const houio::sint64 length : lengths)
    {
        std::ostringstream output(std::ios::out | std::ios::binary);
        houio::json::BinaryWriter writer(output);
        if (!writer.writeLength(length))
            return fail("BinaryWriter rejected a valid length");
        const std::string encoded = output.str().substr(sizeof(binaryMagic));
        std::string expected;
        if (length < 0xf1)
        {
            expected.push_back(static_cast<char>(length));
        }
        else if (length <= std::numeric_limits<houio::uword>::max())
        {
            expected.push_back(static_cast<char>(0xf2));
            appendNative(expected, static_cast<houio::uword>(length));
        }
        else if (length <= std::numeric_limits<houio::uint32>::max())
        {
            expected.push_back(static_cast<char>(0xf4));
            appendNative(expected, static_cast<houio::uint32>(length));
        }
        else
        {
            expected.push_back(static_cast<char>(0xf8));
            appendNative(expected, length);
        }
        if (encoded != expected)
            return fail("BinaryWriter selected an incorrect length-width encoding");
    }

    for (const unsigned char length_id : {0xf2, 0xf4, 0xf8})
    {
        std::string payload;
        payload.push_back(static_cast<char>(houio::json::Token::JID_STRING));
        appendBinaryString(payload, "abc", length_id);
        std::istringstream input(binaryDocument(payload), std::ios::in | std::ios::binary);
        houio::json::JSONReader reader;
        houio::json::Parser parser;
        if (!parser.parse(input, reader) || reader.root().as<std::string>() != "abc")
            return fail("Parser rejected a valid multi-byte length encoding");
    }

    houio::json::ParserLimits limits;
    for (const unsigned char invalid_id : {0xf1, 0xf3, 0xf5, 0xff})
    {
        if (const int result = expectParseFailure(
                binaryDocument({0x27, invalid_id}), limits,
                "invalid binary length id");
            result != 0)
        {
            return result;
        }
    }
    return 0;
}

int verifyStringDefinitionsAndReferences()
{
    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(output);
    writer.jsonBeginMap();

    auto defineAndReference = [&](houio::sint64 id, const std::string& value)
    {
        writer.writeId(houio::json::Token::JID_TOKENDEF);
        writer.writeLength(id);
        writer.writeLength(static_cast<houio::sint64>(value.size()));
        writer.write(std::span<const char>(value.data(), value.size()));
        writer.writeId(houio::json::Token::JID_TOKENREF);
        writer.writeLength(id);
    };

    defineAndReference(1, "name");
    defineAndReference(2, "value");
    writer.jsonString("again");
    writer.writeId(houio::json::Token::JID_TOKENREF);
    writer.writeLength(2);
    writer.jsonEndMap();

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(input, reader))
        return fail("failed to parse string token definitions and references");
    const houio::json::ObjectPtr object = reader.root().asObject();
    if (!object || object->size() != 2
        || object->get<std::string>("name") != "value"
        || object->get<std::string>("again") != "value")
    {
        return fail("defined string tokens were not resolved as keys and values");
    }
    return 0;
}

int verifyWriterIntegerWidths()
{
    const std::vector<std::pair<houio::sint64, houio::json::Token::Type>> cases = {
        {-129, houio::json::Token::JID_INT16},
        {-128, houio::json::Token::JID_INT8},
        {127, houio::json::Token::JID_INT8},
        {128, houio::json::Token::JID_INT16},
        {-32769, houio::json::Token::JID_INT32},
        {-32768, houio::json::Token::JID_INT16},
        {32767, houio::json::Token::JID_INT16},
        {32768, houio::json::Token::JID_INT32},
        {static_cast<houio::sint64>(std::numeric_limits<houio::sint32>::min()) - 1,
            houio::json::Token::JID_INT64},
        {std::numeric_limits<houio::sint32>::min(), houio::json::Token::JID_INT32},
        {std::numeric_limits<houio::sint32>::max(), houio::json::Token::JID_INT32},
        {static_cast<houio::sint64>(std::numeric_limits<houio::sint32>::max()) + 1,
            houio::json::Token::JID_INT64},
    };

    for (const auto& [value, expected_token] : cases)
    {
        std::ostringstream output(std::ios::out | std::ios::binary);
        houio::json::BinaryWriter writer(output);
        writer.jsonInt(value);
        const std::string encoded = output.str();
        if (encoded.size() <= sizeof(binaryMagic)
            || static_cast<unsigned char>(encoded[sizeof(binaryMagic)])
                != static_cast<unsigned char>(expected_token))
        {
            return fail("BinaryWriter selected the wrong integer token width");
        }

        std::istringstream input(encoded, std::ios::in | std::ios::binary);
        houio::json::JSONReader reader;
        houio::json::Parser parser;
        if (!parser.parse(input, reader) || reader.root().as<houio::sint64>() != value)
            return fail("integer width selection changed the scalar value");
    }
    return 0;
}

int verifyEveryUniformNumericArray()
{
    std::array<bool, 40> bool_values{};
    bool_values[0] = true;
    bool_values[31] = true;
    bool_values[32] = true;
    bool_values[39] = true;
    const std::vector<bool> bool_vector = {true, false, true, true};
    const std::array<houio::sbyte, 2> int8_values = {-128, 127};
    const std::array<houio::sword, 2> int16_values = {-32768, 32767};
    const std::array<houio::sint32, 2> int32_values = {-1234567, 1234567};
    const std::array<houio::sint64, 2> int64_values = {
        -1099511627776LL, 1099511627776LL};
    const std::array<houio::uword, 2> real16_values = {
        houio::floatToHalfBits(0.5f), houio::floatToHalfBits(-2.0f)};
    const std::array<houio::real32, 2> real32_values = {-1.25f, 3.5f};
    const std::array<houio::real64, 2> real64_values = {-1.0 / 3.0, 1.0 / 7.0};
    const std::array<houio::ubyte, 2> uint8_values = {0, 255};
    const std::array<houio::uword, 2> uint16_values = {0, 65535};

    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(output);
    writer.jsonBeginArray();
    const bool wrote_all_arrays =
        writer.jsonUniformArray(std::span<const bool>(bool_values))
        && writer.jsonUniformArray(bool_vector)
        && writer.jsonUniformArray(std::span<const houio::sbyte>(int8_values))
        && writer.jsonUniformArray(std::span<const houio::sword>(int16_values))
        && writer.jsonUniformArray(std::span<const houio::sint32>(int32_values))
        && writer.jsonUniformArray(std::span<const houio::sint64>(int64_values))
        && writer.jsonUniformArrayReal16(std::span<const houio::uword>(real16_values))
        && writer.jsonUniformArray(std::span<const houio::real32>(real32_values))
        && writer.jsonUniformArray(std::span<const houio::real64>(real64_values))
        && writer.jsonUniformArray(std::span<const houio::ubyte>(uint8_values))
        && writer.jsonUniformArray(std::span<const houio::uword>(uint16_values));
    writer.jsonEndArray();
    if (!wrote_all_arrays)
        return fail("BinaryWriter rejected a supported uniform-array type");

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(input, reader))
        return fail("failed to round-trip every numeric uniform-array type");
    const houio::json::ArrayPtr root = reader.root().asArray();
    if (!root || root->size() != 11)
        return fail("uniform-array matrix produced the wrong number of arrays");

    const houio::json::ArrayPtr bool_array = root->array(0);
    const houio::json::ArrayPtr vector_bool_array = root->array(1);
    const houio::json::ArrayPtr int8_array = root->array(2);
    const houio::json::ArrayPtr int16_array = root->array(3);
    const houio::json::ArrayPtr int32_array = root->array(4);
    const houio::json::ArrayPtr int64_array = root->array(5);
    const houio::json::ArrayPtr real16_array = root->array(6);
    const houio::json::ArrayPtr real32_array = root->array(7);
    const houio::json::ArrayPtr real64_array = root->array(8);
    const houio::json::ArrayPtr uint8_array = root->array(9);
    const houio::json::ArrayPtr uint16_array = root->array(10);

    if (!bool_array || bool_array->size() != 40 || !bool_array->get<bool>(0)
        || bool_array->get<bool>(1) || !bool_array->get<bool>(31)
        || !bool_array->get<bool>(32) || !bool_array->get<bool>(39)
        || !vector_bool_array || vector_bool_array->size() != 4
        || !vector_bool_array->get<bool>(0) || vector_bool_array->get<bool>(1)
        || !vector_bool_array->get<bool>(2) || !vector_bool_array->get<bool>(3)
        || !int8_array || int8_array->get<int>(0) != -128 || int8_array->get<int>(1) != 127
        || !int16_array || int16_array->get<int>(0) != -32768
        || int16_array->get<int>(1) != 32767
        || !int32_array || int32_array->get<houio::sint32>(0) != -1234567
        || int32_array->get<houio::sint32>(1) != 1234567
        || !int64_array || int64_array->get<houio::sint64>(0) != -1099511627776LL
        || int64_array->get<houio::sint64>(1) != 1099511627776LL
        || !real16_array || real16_array->get<houio::real32>(0) != 0.5f
        || real16_array->get<houio::real32>(1) != -2.0f
        || !real32_array || real32_array->get<houio::real32>(0) != -1.25f
        || real32_array->get<houio::real32>(1) != 3.5f
        || !real64_array || real64_array->get<houio::real64>(0) != -1.0 / 3.0
        || real64_array->get<houio::real64>(1) != 1.0 / 7.0
        || !uint8_array || uint8_array->get<int>(0) != 0 || uint8_array->get<int>(1) != 255
        || !uint16_array || uint16_array->get<int>(0) != 0
        || uint16_array->get<int>(1) != 65535)
    {
        return fail("a numeric uniform-array type changed values during round trip");
    }
    if (!int8_array->isUniform() || !int16_array->isUniform()
        || !int32_array->isUniform() || !int64_array->isUniform()
        || !real32_array->isUniform() || !real64_array->isUniform()
        || !uint8_array->isUniform() || !uint16_array->isUniform())
    {
        return fail("numeric uniform storage was not preserved in the JSON tree");
    }

    for (const bool binary : {false, true})
    {
        std::ostringstream rewritten_output(
            binary ? (std::ios::out | std::ios::binary) : std::ios::out);
        houio::json::JSONWriter tree_writer(rewritten_output, binary);
        if (!tree_writer.write(root))
            return fail("JSONWriter rejected a tree containing uniform arrays");

        std::istringstream rewritten_input(
            rewritten_output.str(),
            binary ? (std::ios::in | std::ios::binary) : std::ios::in);
        houio::json::JSONReader rewritten_reader;
        houio::json::Parser rewritten_parser;
        if (!rewritten_parser.parse(rewritten_input, rewritten_reader))
            return fail("JSONWriter uniform-array tree did not parse");

        const houio::json::ArrayPtr rewritten_root = rewritten_reader.root().asArray();
        const houio::json::ArrayPtr rewritten_bool = rewritten_root ? rewritten_root->array(0) : nullptr;
        const houio::json::ArrayPtr rewritten_int64 = rewritten_root ? rewritten_root->array(5) : nullptr;
        const houio::json::ArrayPtr rewritten_uint16 = rewritten_root ? rewritten_root->array(10) : nullptr;
        if (!rewritten_root || rewritten_root->size() != 11
            || !rewritten_bool || rewritten_bool->size() != 40
            || !rewritten_bool->get<bool>(31) || !rewritten_bool->get<bool>(39)
            || !rewritten_int64
            || rewritten_int64->get<houio::sint64>(1) != 1099511627776LL
            || !rewritten_uint16 || rewritten_uint16->get<int>(1) != 65535)
        {
            return fail("JSONWriter lost uniform-array values during tree round trip");
        }
    }
    return 0;
}

int verifyUniformStringArray()
{
    const std::array<std::string, 3> strings = {"alpha", "", "omega"};
    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(output);
    writer.jsonBeginArray();
    writer.writeId(houio::json::Token::JID_UNIFORM_ARRAY);
    writer.write<houio::ubyte>(static_cast<houio::ubyte>(houio::json::Token::JID_STRING));
    writer.writeLength(static_cast<houio::sint64>(strings.size()));
    for (const std::string& value : strings)
    {
        writer.writeLength(static_cast<houio::sint64>(value.size()));
        writer.write(std::span<const char>(value.data(), value.size()));
    }
    writer.jsonEndArray();

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(input, reader))
        return fail("failed to parse a uniform string array");
    const houio::json::ArrayPtr root = reader.root().asArray();
    const houio::json::ArrayPtr values = root ? root->array(0) : nullptr;
    if (!values || values->size() != 3
        || values->get<std::string>(0) != "alpha"
        || values->get<std::string>(1) != ""
        || values->get<std::string>(2) != "omega")
    {
        return fail("uniform string-array values changed");
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
    if (!parser.parse(input, reader))
    {
        return fail("failed to parse uniform int8 binary JSON");
    }

    houio::json::ArrayPtr root = reader.root().asArray();
    if (!root || root->size() != 1)
    {
        return fail("unexpected root array");
    }

    houio::json::ArrayPtr values = root->array(0);
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
    if (!parser.parse(input, reader))
    {
        return fail("failed to parse multiword uniform bool array");
    }

    houio::json::ArrayPtr root = reader.root().asArray();
    houio::json::ArrayPtr values = root ? root->array(0) : houio::json::ArrayPtr();
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
    if (!parser.parse(scalarInput, scalarReader)
        || scalarReader.root().asArray()->get<houio::real32>(0) != 1.0f)
    {
        return fail("standalone Float16 token was not decoded");
    }

    const houio::uword halfValues[] = {
        houio::floatToHalfBits(0.5f),
        houio::floatToHalfBits(-2.0f),
        houio::floatToHalfBits(3.25f),
    };
    std::ostringstream output(std::ios::out | std::ios::binary);
    houio::json::BinaryWriter writer(output);
    writer.jsonMagic();
    writer.jsonBeginArray();
    if (!writer.jsonUniformArrayReal16(std::span<const houio::uword>(halfValues)))
        return fail("failed to write Float16 uniform array");
    writer.jsonEndArray();

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    if (!parser.parse(input, reader))
    {
        return fail("failed to parse Float16 uniform array");
    }
    houio::json::ArrayPtr values = reader.root().asArray()->array(0);
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
    houio::json::BinaryWriter writer(output);
    writer.jsonMagic();
    writer.jsonBeginArray();
    writer.jsonInt64(1099511627779LL);
    writer.jsonReal64(1.0 / 3.0);
    writer.jsonEndArray();

    std::istringstream input(output.str(), std::ios::in | std::ios::binary);
    houio::json::JSONReader reader;
    houio::json::Parser parser;
    if (!parser.parse(input, reader))
    {
        return fail("failed to parse wide scalar fixture");
    }
    houio::json::ArrayPtr values = reader.root().asArray();
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
        if (!parser.parse(input, reader))
        {
            return fail("valid scalar root at exact EOF was rejected");
        }
    }

    const std::string binaryScalar = binaryDocument({0x11, 0x05});
    std::istringstream binaryScalarInput(binaryScalar, std::ios::in | std::ios::binary);
    houio::json::JSONReader binaryScalarReader;
    houio::json::Parser binaryScalarParser;
    if (!binaryScalarParser.parse(binaryScalarInput, binaryScalarReader)
        || binaryScalarReader.root().as<houio::sint32>() != 5)
    {
        return fail("valid binary scalar root was rejected");
    }

    NonSeekableBuffer scalarBuffer("1");
    std::istream nonSeekableScalarInput(&scalarBuffer);
    houio::json::JSONReader scalarReader;
    houio::json::Parser scalarParser;
    if (!scalarParser.parse(nonSeekableScalarInput, scalarReader)
        || scalarReader.root().as<houio::sint64>() != 1)
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
        if (parser.parse(input, reader, diagnostics) || diagnostics.empty()
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
    if (!asciiParser.parse(asciiInput, asciiReader))
    {
        return fail("valid trailing ASCII whitespace and comments were rejected");
    }

    std::istringstream invalidAsciiInput("[1] 2");
    houio::json::JSONReader invalidAsciiReader;
    houio::DiagnosticList diagnostics;
    if (asciiParser.parse(invalidAsciiInput, invalidAsciiReader, diagnostics)
        || diagnostics.empty())
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
    if (!parser.parse(input, reader))
    {
        return fail("failed to parse array safety fixture");
    }
    houio::json::ArrayPtr values = reader.root().asArray()->array(0);
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

    const houio::sint64 integer_value = 0x102030405060708LL;
    const houio::json::Value typed_value =
        houio::json::Value::create<houio::sint64>(integer_value);
    if (typed_value.as<houio::sint64>() != integer_value
        || std::get<houio::sint64>(typed_value.variant()) != integer_value)
    {
        return fail("int64 Value access changed the value");
    }

    houio::json::ArrayPtr nested_array = houio::json::Array::create();
    nested_array->appendValue<houio::sint32>(7);
    houio::json::ObjectPtr child = houio::json::Object::create();
    child->appendValue("name", std::string("child"));
    houio::json::ObjectPtr object = houio::json::Object::create();
    object->append("items", nested_array);
    object->append("child", child);
    if (!object->contains("items") || object->contains("missing")
        || object->keys() != std::vector<std::string>({"child", "items"})
        || !object->array("items") || object->array("items")->get<houio::sint32>(0) != 7
        || !object->object("child")
        || object->object("child")->get<std::string>("name") != "child"
        || !object->value("missing").isNull())
    {
        return fail("modern JSON tree accessors are inconsistent");
    }

    try
    {
        static_cast<void>(houio::json::Value::createArray().as<houio::sint64>());
        return fail("non-scalar Value::as was accepted");
    }
    catch (const std::logic_error&)
    {
    }

    std::istringstream duplicateMap("{\"a\":1,\"a\":2}");
    houio::json::JSONReader duplicateReader;
    houio::DiagnosticList diagnostics;
    if (parser.parse(duplicateMap, duplicateReader, diagnostics) || diagnostics.empty())
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
    if (const int result = expectParseFailure(
            binaryDocument({0x5b, 0x2d, 0x00, 0x5d}), limits,
            "unsupported string-token undefinition");
        result != 0)
    {
        return result;
    }
    if (const int result = expectParseFailure(
            binaryDocument({0x5b, 0x55, 0x5d}), limits, "unknown binary token");
        result != 0)
    {
        return result;
    }
    if (const int result = expectParseFailure(
            binaryDocument({0x5b, 0x2c, 0x5d}), limits,
            "binary value separator token");
        result != 0)
    {
        return result;
    }
    if (const int result = expectParseFailure(
            binaryDocument({0x5b, 0x2b, 0x01}), limits,
            "truncated string-token definition");
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
        binaryDocument({0x5b, 0x40, 0x7b, 0x00, 0x5d}),
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
        if (!parser.parse(input, reader))
        {
            return fail("parser reuse failed");
        }
        houio::json::ArrayPtr root = reader.root().asArray();
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

    const std::array<houio::sint32, 2> values = {3, 7};
    if (!writer.jsonUniformArray(std::span<const houio::sint32>(values)))
        return fail("writer rejected a valid span-based uniform array");
    const std::vector<bool> empty_bool_values;
    if (!writer.jsonUniformArray(empty_bool_values))
        return fail("writer rejected an empty bool uniform array");

    std::ostringstream ascii_output;
    houio::json::ASCIIWriter ascii_writer(ascii_output);
    ascii_writer.jsonBeginArray();
    ascii_writer.jsonUInt8(houio::ubyte{250});
    ascii_writer.jsonInt8(houio::sbyte{-8});
    ascii_writer.jsonEndArray();
    std::istringstream ascii_input(ascii_output.str());
    houio::json::JSONReader ascii_reader;
    houio::json::Parser ascii_parser;
    if (!ascii_parser.parse(ascii_input, ascii_reader))
        return fail("ASCIIWriter emitted invalid 8-bit integer JSON");
    const houio::json::ArrayPtr ascii_values = ascii_reader.root().asArray();
    if (!ascii_values || ascii_values->size() != 2
        || ascii_values->get<int>(0) != 250 || ascii_values->get<int>(1) != -8)
    {
        return fail("ASCIIWriter changed an 8-bit integer value");
    }

    std::ostringstream failed_output(std::ios::out | std::ios::binary);
    failed_output.setstate(std::ios::badbit);
    houio::json::BinaryWriter failed_writer(failed_output);
    if (failed_writer.writeLength(1))
        return fail("writer reported success for a failed output stream");
    return 0;
}
}

int main()
{
    struct TestCase
    {
        const char* name;
        int (*run)();
    };

    const std::array<TestCase, 21> tests = {{
        {"every scalar token", verifyEveryScalarToken},
        {"null tree round trip", verifyNullTreeRoundTrip},
        {"integer length encodings", verifyIntegerLengthEncodings},
        {"writer integer widths", verifyWriterIntegerWidths},
        {"string definitions and references", verifyStringDefinitionsAndReferences},
        {"every uniform numeric array", verifyEveryUniformNumericArray},
        {"uniform string array", verifyUniformStringArray},
        {"uniform int8", verifyUniformInt8},
        {"uniform bool across words", verifyUniformBoolAcrossWords},
        {"truncated input", verifyTruncatedInput},
        {"length validation", verifyLengthValidation},
        {"float16 tokens", verifyFloat16Tokens},
        {"wide scalar fidelity", verifyWideScalarFidelity},
        {"root and closing state safety", verifyRootAndClosingStateSafety},
        {"input budget and trailing data", verifyInputBudgetAndTrailingData},
        {"array and value safety", verifyArrayAndValueSafety},
        {"declared payload validation", verifyDeclaredPayloadValidation},
        {"token and nesting validation", verifyTokenAndNestingValidation},
        {"structured diagnostics", verifyStructuredDiagnostics},
        {"parser reuse", verifyParserReuse},
        {"writer validation", verifyWriterValidation},
    }};

    for (const TestCase& test : tests)
    {
        try
        {
            if (const int result = test.run(); result != 0)
                return result;
        }
        catch (const std::exception& exception)
        {
            return fail(std::string(test.name) + " threw: " + exception.what());
        }
        catch (...)
        {
            return fail(std::string(test.name) + " threw an unknown exception");
        }
    }
    return 0;
}
