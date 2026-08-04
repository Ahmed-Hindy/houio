#include <houio/NativeVdbPayload.h>

#include <algorithm>
#include <array>
#include <limits>
#include <streambuf>

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

        class TileStreamBuffer final : public std::streambuf
        {
        public:
            explicit TileStreamBuffer(std::size_t tileSize)
                : tile_size_(tileSize)
            {
                payload_ = json::Array::create();
                json::ObjectPtr metadata = json::Object::create();
                metadata->appendValue("tilesize", static_cast<sint32>(tileSize));
                payload_->append(metadata);
                pending_.reserve(tileSize);
            }

            [[nodiscard]] json::ArrayPtr finish()
            {
                flushTile();
                return first_byte_count_ == first_bytes_.size()
                    && NativeVdbPayload::hasOpenVdbMagic(first_bytes_)
                    ? payload_
                    : json::ArrayPtr{};
            }

        protected:
            std::streamsize xsputn(const char* source, std::streamsize count) override
            {
                if( count <= 0 )
                    return 0;
                const std::size_t requested = static_cast<std::size_t>(count);
                const auto* bytes = reinterpret_cast<const ubyte*>(source);
                std::size_t offset = 0;
                while( offset < requested )
                {
                    const std::size_t available = tile_size_ - pending_.size();
                    const std::size_t chunk = std::min(available, requested - offset);
                    recordMagic(std::span<const ubyte>(bytes + offset, chunk));
                    pending_.insert(
                        pending_.end(), bytes + offset, bytes + offset + chunk);
                    offset += chunk;
                    if( pending_.size() == tile_size_ )
                        flushTile();
                }
                return count;
            }

            int_type overflow(int_type character) override
            {
                if( traits_type::eq_int_type(character, traits_type::eof()) )
                    return traits_type::not_eof(character);
                const char value = traits_type::to_char_type(character);
                return xsputn(&value, 1) == 1
                    ? character
                    : traits_type::eof();
            }

            int sync() override
            {
                return 0;
            }

        private:
            void recordMagic(std::span<const ubyte> bytes)
            {
                const std::size_t missing = first_bytes_.size() - first_byte_count_;
                const std::size_t count = std::min(missing, bytes.size());
                std::copy_n(
                    bytes.begin(),
                    count,
                    first_bytes_.begin() + static_cast<std::ptrdiff_t>(first_byte_count_));
                first_byte_count_ += count;
            }

            void flushTile()
            {
                if( pending_.empty() )
                    return;
                json::ArrayPtr tile = json::Array::create();
                const std::span<const ubyte> values(pending_.data(), pending_.size());
                tile->setUniformStorage(
                    static_cast<int>(json::variantIndex<ubyte, json::Value::Variant>),
                    json::Token::JID_UINT8,
                    static_cast<sint64>(pending_.size()),
                    std::as_bytes(values));
                payload_->append(tile);
                pending_.clear();
            }

            std::size_t tile_size_ = 0;
            json::ArrayPtr payload_;
            std::vector<ubyte> pending_;
            std::array<ubyte, 4> first_bytes_{};
            std::size_t first_byte_count_ = 0;
        };

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
                json::Token::JID_UINT8,
                static_cast<sint64>(count),
                std::as_bytes(values));
            payload->append(tile);
        }

        GeometryReadResult<json::ArrayPtr> result;
        result.value = std::move(payload);
        result.succeeded = true;
        return result;
    }

    GeometryReadResult<json::ArrayPtr> NativeVdbPayload::encodeStream(
        const StreamWriter& writer,
        std::size_t tileSize)
    {
        if( !writer )
        {
            return encodeFailure(
                DiagnosticCategory::schema,
                "Native VDB stream writer cannot be empty",
                "vdb.writer");
        }
        if( tileSize == 0
            || tileSize > static_cast<std::size_t>(std::numeric_limits<sint32>::max()) )
        {
            return encodeFailure(
                DiagnosticCategory::schema,
                "Native VDB payload tile size is outside the supported range",
                "vdb.tilesize");
        }

        try
        {
            TileStreamBuffer buffer(tileSize);
            std::ostream output(&buffer);
            GeometryWriteResult writeResult = writer(output);
            output.flush();
            if( !writeResult )
            {
                GeometryReadResult<json::ArrayPtr> result;
                result.diagnostics = std::move(writeResult.diagnostics);
                return result;
            }
            if( !output )
            {
                return encodeFailure(
                    DiagnosticCategory::io,
                    "Native VDB stream writer failed while producing payload bytes",
                    "vdb.stream");
            }

            json::ArrayPtr payload = buffer.finish();
            if( !payload )
            {
                return encodeFailure(
                    DiagnosticCategory::malformed_input,
                    "Native VDB stream writer did not produce OpenVDB magic",
                    "vdb.stream");
            }

            GeometryReadResult<json::ArrayPtr> result;
            result.value = std::move(payload);
            result.succeeded = true;
            return result;
        }
        catch( const std::exception& exception )
        {
            return encodeFailure(
                DiagnosticCategory::conversion,
                std::string("Native VDB stream encoding failed: ") + exception.what(),
                "vdb.stream");
        }
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
