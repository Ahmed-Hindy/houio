#include <houio/NativeVdbPayload.h>

#include <algorithm>
#include <array>
#include <limits>

namespace houio
{
    namespace
    {
        GeometryReadResult<json::ArrayPtr> encodeFailure(
            DiagnosticCategory category,
            std::string message,
            std::string path)
        {
            GeometryReadResult<json::ArrayPtr> result;
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                category,
                std::move(message),
                -1,
                std::move(path)});
            return result;
        }

        GeometryReadResult<std::vector<ubyte>> decodeFailure(
            DiagnosticCategory category,
            std::string message,
            std::string path)
        {
            GeometryReadResult<std::vector<ubyte>> result;
            result.diagnostics.push_back(Diagnostic{
                DiagnosticSeverity::error,
                category,
                std::move(message),
                -1,
                std::move(path)});
            return result;
        }
    }

    GeometryReadResult<json::ArrayPtr> NativeVdbPayload::encode(
        std::span<const ubyte> openvdbStream,
        std::size_t tileSize)
    {
        if( tileSize == 0 || tileSize > static_cast<std::size_t>(std::numeric_limits<sint32>::max()) )
        {
            return encodeFailure(
                DiagnosticCategory::schema,
                "Native VDB payload tile size is outside the supported range",
                "vdb.tilesize");
        }
        if( !hasOpenVdbMagic(openvdbStream) )
        {
            return encodeFailure(
                DiagnosticCategory::malformed_input,
                "Native VDB payload does not begin with the OpenVDB stream magic",
                "vdb.stream");
        }

        json::ArrayPtr payload = json::Array::create();
        json::ObjectPtr metadata = json::Object::create();
        metadata->appendValue("tilesize", static_cast<sint32>(tileSize));
        payload->append(metadata);

        for( std::size_t offset = 0; offset < openvdbStream.size(); offset += tileSize )
        {
            const std::size_t count = std::min(tileSize, openvdbStream.size() - offset);
            json::ArrayPtr tile = json::Array::create();
            const std::span<const ubyte> values = openvdbStream.subspan(offset, count);
            tile->setUniformStorage(
                static_cast<int>(json::variantIndex<ubyte, json::Value::Variant>),
                static_cast<sint64>(count),
                std::as_bytes(values));
            payload->append(tile);
        }

        GeometryReadResult<json::ArrayPtr> result;
        result.value = std::move(payload);
        result.succeeded = true;
        return result;
    }

    GeometryReadResult<std::vector<ubyte>> NativeVdbPayload::decode(
        const json::ArrayPtr& payload)
    {
        if( !payload || payload->size() < 2 )
        {
            return decodeFailure(
                DiagnosticCategory::schema,
                "Native VDB payload requires metadata and at least one byte tile",
                "vdb");
        }

        const json::ObjectPtr metadata = payload->object(0);
        if( !metadata )
        {
            return decodeFailure(
                DiagnosticCategory::schema,
                "Native VDB payload metadata is not an object",
                "vdb[0]");
        }
        const sint64 tileSize = metadata->get<sint64>("tilesize", 0);
        if( tileSize <= 0 || tileSize > std::numeric_limits<sint32>::max() )
        {
            return decodeFailure(
                DiagnosticCategory::schema,
                "Native VDB payload tile size is invalid",
                "vdb[0].tilesize");
        }

        std::vector<ubyte> bytes;
        for( sint64 tileIndex = 1; tileIndex < payload->size(); ++tileIndex )
        {
            const json::ArrayPtr tile = payload->array(static_cast<int>(tileIndex));
            if( !tile || tile->size() <= 0 )
            {
                return decodeFailure(
                    DiagnosticCategory::schema,
                    "Native VDB payload contains an empty or non-array tile",
                    "vdb[" + std::to_string(tileIndex) + "]");
            }
            if( tileIndex + 1 < payload->size() && tile->size() != tileSize )
            {
                return decodeFailure(
                    DiagnosticCategory::schema,
                    "Native VDB payload contains a non-final tile with the wrong size",
                    "vdb[" + std::to_string(tileIndex) + "]");
            }
            if( tile->size() > tileSize )
            {
                return decodeFailure(
                    DiagnosticCategory::schema,
                    "Native VDB payload tile exceeds the declared tile size",
                    "vdb[" + std::to_string(tileIndex) + "]");
            }
            if( static_cast<uint64>(bytes.size())
                > static_cast<uint64>(std::numeric_limits<std::size_t>::max())
                    - static_cast<uint64>(tile->size()) )
            {
                return decodeFailure(
                    DiagnosticCategory::malformed_input,
                    "Native VDB payload size exceeds addressable memory",
                    "vdb");
            }
            bytes.reserve(bytes.size() + static_cast<std::size_t>(tile->size()));
            for( int valueIndex = 0; valueIndex < static_cast<int>(tile->size()); ++valueIndex )
            {
                const int value = tile->get<int>(valueIndex);
                if( value < 0 || value > 255 )
                {
                    return decodeFailure(
                        DiagnosticCategory::schema,
                        "Native VDB payload byte is outside the UInt8 range",
                        "vdb[" + std::to_string(tileIndex) + "]["
                            + std::to_string(valueIndex) + "]");
                }
                bytes.push_back(static_cast<ubyte>(value));
            }
        }

        if( !hasOpenVdbMagic(bytes) )
        {
            return decodeFailure(
                DiagnosticCategory::malformed_input,
                "Native VDB payload does not contain an OpenVDB stream",
                "vdb");
        }

        GeometryReadResult<std::vector<ubyte>> result;
        result.value = std::move(bytes);
        result.succeeded = true;
        return result;
    }

    bool NativeVdbPayload::hasOpenVdbMagic(std::span<const ubyte> bytes) noexcept
    {
        static constexpr std::array<ubyte, 4> magic{0x20U, 0x42U, 0x44U, 0x56U};
        return bytes.size() >= magic.size()
            && std::equal(magic.begin(), magic.end(), bytes.begin());
    }
}
