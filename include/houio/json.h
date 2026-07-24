#pragma once

// Houdini ASCII and binary JSON parser, event handlers, value tree, and writers.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <span>
#include <stack>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

#include <houio/Diagnostic.h>
#include <houio/HalfFloat.h>
#include <houio/types.h>




namespace houio
{
	namespace json
	{
		struct Parser;

		// Scalar handlers preserve 32-bit and 64-bit integer and floating-point widths.
		struct Handler
		{
			virtual ~Handler() = default;

			virtual void jsonBeginArray() = 0;
			virtual void jsonEndArray() = 0;
			virtual void jsonBeginMap() = 0;
			virtual void jsonEndMap() = 0;
			virtual void jsonNull() = 0;
			virtual void jsonString(const std::string& value) = 0;
			virtual void jsonKey(const std::string& key) = 0;
			virtual void jsonBool(const bool& value) = 0;
			virtual void jsonInt32(const sint32& value) = 0;
			virtual void jsonInt64(const sint64& value) = 0;
			virtual void jsonReal32(const real32& value) = 0;
			virtual void jsonReal64(const real64& value) = 0;
			virtual void uaBool(sint64 element_count, Parser& parser) = 0;
			virtual void uaReal16(sint64 element_count, Parser& parser) = 0;
			virtual void uaReal32(sint64 element_count, Parser& parser) = 0;
			virtual void uaReal64(sint64 element_count, Parser& parser) = 0;
			virtual void uaInt8(sint64 element_count, Parser& parser) = 0;
			virtual void uaInt16(sint64 element_count, Parser& parser) = 0;
			virtual void uaInt32(sint64 element_count, Parser& parser) = 0;
			virtual void uaInt64(sint64 element_count, Parser& parser) = 0;
			virtual void uaUInt8(sint64 element_count, Parser& parser) = 0;
			virtual void uaUInt16(sint64 element_count, Parser& parser) = 0;
			virtual void uaString(sint64 element_count, Parser& parser) = 0;
		};


		struct Token
		{
			enum Type
			{
				// JSON identifiers used in binary encoding and parsing
				JID_NULL                = 0x00,
				JID_MAP_BEGIN           = 0x7b, // '{'
				JID_MAP_END             = 0x7d, // '}'
				JID_ARRAY_BEGIN         = 0x5b, // '['
				JID_ARRAY_END           = 0x5d, // ']'
				JID_BOOL                = 0x10,
				JID_INT8                = 0x11,
				JID_INT16               = 0x12,
				JID_INT32               = 0x13,
				JID_INT64               = 0x14,
				JID_REAL16              = 0x18,
				JID_REAL32              = 0x19,
				JID_REAL64              = 0x1a,
				JID_UINT8               = 0x21,
				JID_UINT16              = 0x22,
				JID_STRING              = 0x27,
				JID_FALSE               = 0x30,
				JID_TRUE                = 0x31,
				JID_TOKENDEF            = 0x2b, // triggers on-the-fly string definition
				JID_TOKENREF            = 0x26, // references a previous defined string
				JID_TOKENUNDEF          = 0x2d,
				JID_UNIFORM_ARRAY       = 0x40,
				JID_KEY_SEPARATOR       = 0x3a,
				JID_VALUE_SEPARATOR     = 0x2c,
				JID_MAGIC               = 0x7f
			};

			using Value = std::variant<
				bool,
				sbyte,
				sword,
				sint32,
				sint64,
				real32,
				real64,
				ubyte,
				uword,
				std::string>;
			Token();
			void event(Parser& parser, bool key = false);

		private:
			friend struct Parser;

			Type                                type; // also encodes value type
			Value                              value;
			Type                              uaType; // additional type 
		};


		// UTILITY FUNCTIONS ======================================

		template<typename T, typename Variant>
		struct VariantIndex;

		template<typename T, typename... Rest>
		struct VariantIndex<T, std::variant<T, Rest...>> : std::integral_constant<size_t, 0>
		{
		};

		template<typename T, typename First, typename... Rest>
		struct VariantIndex<T, std::variant<First, Rest...>>
			: std::integral_constant<size_t, 1 + VariantIndex<T, std::variant<Rest...>>::value>
		{
		};

		template<typename T, typename Variant>
		inline constexpr size_t variantIndex = VariantIndex<T, Variant>::value;

		template<typename>
		inline constexpr bool alwaysFalse = false;

		template<typename T>
		[[nodiscard]] constexpr Token::Type uniformArrayToken()
		{
			using ValueType = std::remove_cv_t<T>;
			if constexpr (std::is_same_v<ValueType, real32>)
				return Token::JID_REAL32;
			else if constexpr (std::is_same_v<ValueType, real64>)
				return Token::JID_REAL64;
			else if constexpr (std::is_same_v<ValueType, bool>)
				return Token::JID_BOOL;
			else if constexpr (std::is_same_v<ValueType, sbyte>)
				return Token::JID_INT8;
			else if constexpr (std::is_same_v<ValueType, sword>)
				return Token::JID_INT16;
			else if constexpr (std::is_same_v<ValueType, sint32>)
				return Token::JID_INT32;
			else if constexpr (std::is_same_v<ValueType, sint64>)
				return Token::JID_INT64;
			else if constexpr (std::is_same_v<ValueType, ubyte>)
				return Token::JID_UINT8;
			else if constexpr (std::is_same_v<ValueType, uword>)
				return Token::JID_UINT16;
			else
				static_assert(alwaysFalse<ValueType>, "Unsupported uniform JSON array type");
		}

		template<typename T>
		T fromString(const std::string& value)
		{
			std::istringstream stream(value);
			T result{};
			stream >> result;
			if( stream.fail() )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::malformed_input, "Unable to parse JSON scalar " + value, -1, ""});
			return result;
		}

		template<class T>
		inline std::string toString(const T& t)
		{
			std::ostringstream stream;
			stream << t;
			return stream.str();
		}
		template<class T>
		inline std::string toString(const T& t, int precision)
		{
			std::ostringstream stream;
			stream.precision( precision );
			stream << t;
			return stream.str();
		}

		// Parser ==================================================
		struct ParserLimits
		{
			sint64 maxInputBytes = 1024LL * 1024LL * 1024LL;
			sint64 maxStringBytes = 64 * 1024 * 1024;
			sint64 maxUniformArrayElements = 64 * 1024 * 1024;
			size_t maxNestingDepth = 1024;
		};

		struct Parser
		{
			enum State
			{
				// Parsing states
				STATE_INVALID           = -1,
				STATE_START             = 0,
				STATE_COMPLETE          = 1,
				STATE_MAP_START         = 2,
				STATE_MAP_SEPERATOR     = 3,
				STATE_MAP_NEED_VALUE    = 4,
				STATE_MAP_GOT_VALUE     = 5,
				STATE_MAP_NEED_KEY      = 6,
				STATE_ARRAY_START       = 7,
				STATE_ARRAY_NEED_VALUE  = 8,
				STATE_ARRAY_GOT_VALUE   = 9
			};

			Parser();
			explicit Parser( const ParserLimits &limits );

			[[nodiscard]] bool parse(std::istream& input, Handler& handler);
			[[nodiscard]] bool parse(
				std::istream& input,
				Handler& handler,
				DiagnosticList& diagnostics);

			bool                          parseStream();
			void             validateSeekableInputSize();
			void                 validateTrailingInput();
			void validateUniformArrayPayload( Token::Type type, sint64 elementCount );
			bool                  readToken( Token &t );
			bool            readBinaryToken( Token &t, ubyte test );
			bool     readASCIIToken( Token &t, char c );
			bool           readBinaryStringDefinition();
			bool                       undefineString();
			void                   pushState( State s );
			void                    setState( State s );
			void                             popState();
			[[noreturn]] void fail( DiagnosticCategory category, const std::string &message, sint64 offset = -1 );


			template<typename T>
			[[nodiscard]] T read();
			template<typename T>
			void read(std::span<T> destination);
			sint64                         readLength();
			std::string              readBinaryString();
			std::string               readASCIIString();
			bool                  tryReadASCIIChar( char &value );


		private:
			friend struct Token;

			[[nodiscard]] bool parseInternal(
				std::istream& input,
				Handler& event_handler,
				DiagnosticList* diagnostics);

			State                                 state;
			std::stack<State>                stateStack;
			Handler                            *handler;
			std::istream                        *stream;
			bool                                 binary;
			sint64                              byteOffset;
			sint64                              tokenOffset;
			sint64                         knownInputBytes;

			std::map<sint64, std::string>       strings; // common strings are references by ids
			ParserLimits                         limits;
		};



		template<typename T>
		T Parser::read()
		{
			std::array<T, 1> value{};
			read(std::span<T>(value));
			return value.front();
		}

		template<typename T>
		void Parser::read(std::span<T> destination)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			if (!stream)
				throw std::runtime_error("Parser::read has no input stream");
			if (destination.empty())
				return;
			const uint64 element_count = static_cast<uint64>(destination.size());
			const uint64 maximum_bytes = static_cast<uint64>(std::numeric_limits<std::streamsize>::max());
			if (element_count > maximum_bytes / sizeof(T))
				throw std::length_error("Parser::read byte count exceeds streamsize range");

			const std::streamsize byte_count =
				static_cast<std::streamsize>(element_count * sizeof(T));
			if (byteOffset < 0 || byteOffset > limits.maxInputBytes
				|| byte_count > limits.maxInputBytes - byteOffset)
			{
				fail(DiagnosticCategory::malformed_input, "Parser input byte limit exceeded", byteOffset);
			}
			if (knownInputBytes >= 0 && (byteOffset > knownInputBytes
				|| byte_count > knownInputBytes - byteOffset))
			{
				fail(DiagnosticCategory::malformed_input,
					"Parser::read encountered truncated input", knownInputBytes);
			}
			stream->read(reinterpret_cast<char*>(destination.data()), byte_count);
			const std::streamsize bytes_read = stream->gcount();
			byteOffset += static_cast<sint64>(bytes_read);
			if (bytes_read != byte_count)
				fail(DiagnosticCategory::malformed_input,
					"Parser::read encountered truncated input", byteOffset);
		}


		// Writer ==================================================
		struct Writer
		{
			virtual ~Writer() = default;
			virtual void                       jsonBeginArray() = 0;
			virtual void                         jsonEndArray() = 0;
			virtual void                         jsonBeginMap() = 0;
			virtual void                           jsonEndMap() = 0;
			virtual void                           jsonNull() = 0;
			virtual void jsonString( const std::string &value ) = 0;
			virtual void      jsonKey( const std::string &key ) = 0;
			virtual void          jsonBool( const bool &value ) = 0;
			virtual void         jsonInt( const sint64 &value ) = 0; // specific type written will depend on range in which value lies
			virtual void        jsonUInt8( const ubyte &value ) = 0; // specific type written will depend on range in which value lies
			virtual void         jsonInt8( const sbyte &value ) = 0; // specific type written will depend on range in which value lies
			virtual void       jsonInt32( const sint32 &value ) = 0; // specific type written will depend on range in which value lies
			virtual void       jsonInt64( const sint64 &value ) = 0; // specific type written will depend on range in which value lies
			virtual void      jsonReal32( const real32 &value ) = 0;
			virtual void      jsonReal64( const real64 &value ) = 0;
		};

		class BinaryWriter final : public Writer
		{
		public:
			explicit BinaryWriter(std::ostream& output);

			bool jsonMagic();
			virtual void                               jsonBeginArray()override;
			virtual void                                 jsonEndArray()override;
			virtual void                                 jsonBeginMap()override;
			virtual void                                   jsonEndMap()override;
			virtual void                                   jsonNull()override;
			virtual void         jsonString( const std::string &value )override;
			virtual void              jsonKey( const std::string &key )override;
			virtual void                  jsonBool( const bool &value )override;
			virtual void                 jsonInt( const sint64 &value )override;
			virtual void                jsonUInt8( const ubyte &value )override;
			virtual void                 jsonInt8( const sbyte &value )override;
			virtual void               jsonInt32( const sint32 &value )override;
			virtual void               jsonInt64( const sint64 &value )override;

			virtual void              jsonReal32( const real32 &value )override;
			virtual void              jsonReal64( const real64 &value )override;


			template<typename T>
			bool jsonUniformArray(std::span<const T> data);
			template<typename T>
			bool jsonUniformArray(const std::vector<T>& data);
			bool jsonUniformArray(const std::vector<bool>& data);
			bool jsonUniformArrayReal16(std::span<const uword> data);

			bool                                      writeId( Token::Type id );
			bool                            writeLength( const sint64 &length );


			template<typename T>
			bool write(const T& value);
			template<typename T>
			bool write(std::span<const T> values);



		private:
			bool writeBoolUniformArray(std::span<const bool> data);

			std::ostream& stream;
		};

		template<typename T>
		bool BinaryWriter::write(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			return write(std::span<const T>(&value, 1));
		}

		template<typename T>
		bool BinaryWriter::write(std::span<const T> values)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			if (values.empty())
				return stream.good();
			const uint64 element_count = static_cast<uint64>(values.size());
			const uint64 maximum_bytes =
				static_cast<uint64>(std::numeric_limits<std::streamsize>::max());
			if (element_count > maximum_bytes / sizeof(T))
				throw std::length_error("BinaryWriter::write byte count exceeds streamsize range");
			const std::streamsize byte_count =
				static_cast<std::streamsize>(element_count * sizeof(T));
			stream.write(reinterpret_cast<const char*>(values.data()), byte_count);
			return stream.good();
		}

		template<typename T>
		bool BinaryWriter::jsonUniformArray(std::span<const T> data)
		{
			using ValueType = std::remove_cv_t<T>;
			if constexpr (std::is_same_v<ValueType, bool>)
			{
				return writeBoolUniformArray(data);
			}
			else
			{
				if (data.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()))
					throw std::length_error("BinaryWriter::jsonUniformArray element count exceeds sint64 range");
				const sint64 element_count = static_cast<sint64>(data.size());
				const Token::Type type = uniformArrayToken<ValueType>();
				return writeId(Token::JID_UNIFORM_ARRAY)
					&& write<ubyte>(static_cast<ubyte>(type))
					&& writeLength(element_count)
					&& write(data);
			}
		}

		template<typename T>
		bool BinaryWriter::jsonUniformArray(const std::vector<T>& data)
		{
			return jsonUniformArray(std::span<const T>(data));
		}


		// ASCIIWriter ==================================================
		class ASCIIWriter final : public Writer
		{
		public:
			explicit ASCIIWriter(std::ostream& output);

			virtual void                               jsonBeginArray()override;
			virtual void                                 jsonEndArray()override;
			virtual void                                 jsonBeginMap()override;
			virtual void                                   jsonEndMap()override;
			virtual void                                   jsonNull()override;
			virtual void         jsonString( const std::string &value )override;
			virtual void              jsonKey( const std::string &key )override;
			virtual void                  jsonBool( const bool &value )override;
			virtual void                 jsonInt( const sint64 &value )override;
			virtual void                jsonUInt8( const ubyte &value )override;
			virtual void                 jsonInt8( const sbyte &value )override;
			virtual void               jsonInt32( const sint32 &value )override;
			virtual void               jsonInt64( const sint64 &value )override;

			virtual void              jsonReal32( const real32 &value )override;
			virtual void              jsonReal64( const real64 &value )override;


			void                               write( const std::string &text );

			void                                                 writeNewline();
			void                                                  writePrefix();

		private:
			int indentLevel = 0;
			bool firstItem = false;
			bool gotKey = false;
			std::stack<Token::Type> stack;
			std::string prefix;
			std::ostream& stream;
		};



		// JSONLogger ==================================================
		class JSONLogger final : public Handler
		{
		public:
			JSONLogger() : out(std::cout), indentLevel(0)
			{
			}
			explicit JSONLogger(std::ostream& outputStream) : out(outputStream), indentLevel(0)
			{
			}

			void indent()
			{
				for( int i=0;i<indentLevel;++i )
					out << "\t";std::flush(out);
			}

			virtual void jsonBeginArray()
			{
				indent();
				out << "jsonBeginArray\n";std::flush(out);
				++indentLevel;
			}

			virtual void jsonEndArray()
			{
				--indentLevel;
				indent();
				out << "jsonEndArray\n";std::flush(out);
			}

			virtual void jsonString( const std::string &value )
			{
				indent();
				out << "jsonString " << value << "\n";std::flush(out);
			}

			virtual void jsonKey( const std::string &key )
			{
				indent();
				out << "jsonKey " << key << "\n";std::flush(out);
			}

			virtual void jsonBool( const bool &value )
			{
				indent();
				out << "jsonBool " << value << "\n";std::flush(out);
			}

			virtual void jsonInt32( const sint32 &value )
			{
				indent();
				out << "jsonInt32 " << value << "\n";std::flush(out);
			}

			virtual void jsonInt64( const sint64 &value )
			{
				indent();
				out << "jsonInt64 " << value << "\n";std::flush(out);
			}

			virtual void jsonReal32( const real32 &value )
			{
				indent();
				out << "jsonReal32 " << value << "\n";std::flush(out);
			}

			virtual void jsonReal64( const real64 &value )
			{
				indent();
				out << "jsonReal64 " << value << "\n";std::flush(out);
			}

			virtual void jsonBeginMap()
			{
				indent();
				out << "jsonBeginMap\n";std::flush(out);
				++indentLevel;
			}

			virtual void jsonEndMap()
			{
				--indentLevel;
				indent();
				out << "jsonEndMap\n";std::flush(out);
			}

			void jsonNull() override
			{
				indent();
				out << "jsonNull\n";std::flush(out);
			}

			void uaBool(sint64 element_count, Parser& parser) override
			{
				indent();
				if (element_count < 0)
					throw std::length_error("JSONLogger::uaBool received a negative element count");
				sint64 elements_remaining = element_count;
				out << "jsonArray [";
				while (elements_remaining > 0)
				{
					const uint32 bits = parser.read<uint32>();
					const int bit_count = static_cast<int>(std::min<sint64>(elements_remaining, 32));
					for (int bit_index = 0; bit_index < bit_count; ++bit_index)
						out << ((bits & (uint32(1) << bit_index)) != 0 ? 1 : 0) << " ";
					elements_remaining -= bit_count;
				}
				out << "]\n";
				std::flush(out);
			}

			void uaReal16(sint64 element_count, Parser& parser) override
			{
				indent();
				out << "jsonArray<real16> (" << element_count << ") [";
				for (sint64 element_index = 0; element_index < element_count; ++element_index)
					out << halfBitsToFloat(parser.read<uword>()) << " ";
				out << "]---\n";
				std::flush(out);
			}

			void uaReal32(sint64 element_count, Parser& parser) override
			{
				uniformArray<real32>(element_count, parser, "<real32>");
			}

			void uaReal64(sint64 element_count, Parser& parser) override
			{
				uniformArray<real64>(element_count, parser, "<real64>");
			}

			void uaInt8(sint64 element_count, Parser& parser) override
			{
				uniformArray<sbyte>(element_count, parser, "<int8>");
			}

			void uaInt16(sint64 element_count, Parser& parser) override
			{
				uniformArray<sword>(element_count, parser, "<int16>");
			}

			void uaInt32(sint64 element_count, Parser& parser) override
			{
				uniformArray<sint32>(element_count, parser, "<int32>");
			}

			void uaInt64(sint64 element_count, Parser& parser) override
			{
				uniformArray<sint64>(element_count, parser, "<sint64>");
			}

			void uaUInt8(sint64 element_count, Parser& parser) override
			{
				indent();
				if (element_count < 0)
					throw std::length_error("JSONLogger::uaUInt8 received a negative element count");
				out << "jsonArray<uint8> [";
				sint64 elements_remaining = element_count;
				while (elements_remaining > 0)
				{
					const size_t chunk_size = static_cast<size_t>(std::min<sint64>(elements_remaining, 4096));
					std::vector<ubyte> data(chunk_size);
					parser.read(std::span<ubyte>(data));
					for (ubyte value : data)
						out << static_cast<int>(value) << " ";
					elements_remaining -= static_cast<sint64>(chunk_size);
				}
				out << "]\n";
				std::flush(out);
			}

			void uaUInt16(sint64 element_count, Parser& parser) override
			{
				uniformArray<uword>(element_count, parser, "<uint16>");
			}

			void uaString(sint64 element_count, Parser& parser) override
			{
				indent();
				if (element_count < 0)
					throw std::length_error("JSONLogger::uaString received a negative element count");
				out << "jsonArray<string> [";
				for (sint64 element_index = 0; element_index < element_count; ++element_index)
					out << parser.readBinaryString() << " ";
				out << "]\n";
				std::flush(out);
			}

			template<typename T>
			void uniformArray(sint64 element_count, Parser& parser, const std::string& type_name = {})
			{
				indent();
				if (element_count < 0)
					throw std::length_error("JSONLogger uniform array received a negative element count");
				out << "jsonArray" << type_name << " (" << element_count << ") [";
				sint64 elements_remaining = element_count;
				while (elements_remaining > 0)
				{
					const size_t chunk_size = static_cast<size_t>(std::min<sint64>(elements_remaining, 4096));
					std::vector<T> data(chunk_size);
					parser.read(std::span<T>(data));
					for (const T& value : data)
						out << value << " ";
					elements_remaining -= static_cast<sint64>(chunk_size);
				}
				out << "]---\n";
				std::flush(out);
			}

		private:
			std::ostream& out;
			int indentLevel = 0;
		};

		// JSONCPP ==================================================

		// Value -------------

		struct Array;
		using ArrayPtr = std::shared_ptr<Array>;
		using ConstArrayPtr = std::shared_ptr<const Array>;
		struct Object;
		using ObjectPtr = std::shared_ptr<Object>;
		using ConstObjectPtr = std::shared_ptr<const Object>;

		class Value
		{
		public:
			using Variant = std::variant<
				bool,
				sint32,
				real32,
				real64,
				std::string,
				ubyte,
				sint64>;

			Value() = default;

			[[nodiscard]] bool isNull() const noexcept;
			[[nodiscard]] bool isArray() const noexcept;
			[[nodiscard]] bool isObject() const noexcept;
			[[nodiscard]] bool isString() const noexcept;

			template<typename T>
			[[nodiscard]] T as() const;

			[[nodiscard]] ArrayPtr asArray();
			[[nodiscard]] ConstArrayPtr asArray() const;
			[[nodiscard]] ObjectPtr asObject();
			[[nodiscard]] ConstObjectPtr asObject() const;

			[[nodiscard]] Variant& variant() noexcept;
			[[nodiscard]] const Variant& variant() const noexcept;

			[[nodiscard]] static Value createArray();
			[[nodiscard]] static Value createArray(ArrayPtr array);
			[[nodiscard]] static Value createObject();
			[[nodiscard]] static Value createObject(ObjectPtr object);

			template<typename T>
			[[nodiscard]] static Value create(const T& value);

		private:
			enum class Kind
			{
				null,
				scalar,
				object,
				array,
			};

			Kind kind_ = Kind::null;
			Variant scalar_ = false;
			ObjectPtr object_;
			ArrayPtr array_;

			friend struct Object;
			friend struct Array;
		};


		template<typename Destination, typename Source>
		[[nodiscard]] Destination convertScalarValue(const Source& source)
		{
			using SourceType = std::remove_cvref_t<Source>;
			if constexpr (std::is_same_v<Destination, std::string>)
			{
				if constexpr (std::is_same_v<SourceType, std::string>)
					return source;
				else
					return toString(source);
			}
			else if constexpr (std::is_same_v<SourceType, std::string>)
			{
				if constexpr (std::is_same_v<Destination, bool>)
					return false;
				else
					return fromString<Destination>(source);
			}
			else
			{
				return static_cast<Destination>(source);
			}
		}

		template<typename T>
		T Value::as() const
		{
			if (kind_ != Kind::scalar)
				throw std::logic_error("JSON Value does not contain a scalar");
			return std::visit(
				[](const auto& source) { return convertScalarValue<T>(source); },
				scalar_);
		}

		template<typename T>
		Value Value::create(const T& value)
		{
			Value result;
			result.kind_ = Kind::scalar;
			result.scalar_ = value;
			return result;
		}


		struct Array
		{
			using ValueList = std::vector<Value>;

			[[nodiscard]] static ArrayPtr create();

			template<typename T>
			[[nodiscard]] T get(int index) const;
			[[nodiscard]] ObjectPtr object(int index) const;
			[[nodiscard]] ArrayPtr array(int index) const;
			[[nodiscard]] Value value(int index) const;
			[[nodiscard]] sint64 size() const;
			[[nodiscard]] bool isUniform() const noexcept;
			[[nodiscard]] std::span<const Value> elements() const noexcept;
			[[nodiscard]] std::span<const std::byte> uniformData() const noexcept;
			[[nodiscard]] sint64 uniformElementCount() const noexcept;
			[[nodiscard]] int uniformTypeIndex() const noexcept;

			void setUniformStorage(
				int type_index,
				sint64 element_count,
				std::span<const std::byte> data);

			template<typename T>
			void appendValue(const T& value);
			void append(const Value& value);
			void append(ObjectPtr object);
			void append(ArrayPtr array);

		private:
			ValueList values_;
			bool uses_uniform_storage_ = false;
			std::vector<std::byte> uniform_data_;
			sint64 uniform_element_count_ = 0;
			int uniform_type_index_ = -1;
		};

		template<typename T>
		T Array::get(int index) const
		{
			return value(index).as<T>();
		}

		template<typename T>
		void Array::appendValue(const T& value)
		{
			append(Value::create<T>(value));
		}

		struct Object
		{
			using EntryMap = std::map<std::string, Value>;

			[[nodiscard]] static ObjectPtr create();
			[[nodiscard]] bool contains(const std::string& key) const;

			template<typename T>
			[[nodiscard]] T get(const std::string& key, T default_value = T()) const;
			[[nodiscard]] ObjectPtr object(const std::string& key) const;
			[[nodiscard]] ArrayPtr array(const std::string& key) const;
			[[nodiscard]] Value value(const std::string& key) const;
			[[nodiscard]] std::vector<std::string> keys() const;
			[[nodiscard]] sint64 size() const;
			[[nodiscard]] const EntryMap& entries() const noexcept;

			template<typename T>
			void appendValue(const std::string& key, const T& value);
			void append(const std::string& key, const Value& value);
			void append(const std::string& key, ObjectPtr object);
			void append(const std::string& key, ArrayPtr array);

		private:
			EntryMap entries_;
		};

		template<typename T>
		T Object::get(const std::string& key, T default_value) const
		{
			const auto entry = entries_.find(key);
			return entry == entries_.end() ? default_value : entry->second.as<T>();
		}

		template<typename T>
		void Object::appendValue(const std::string& key, const T& value)
		{
			append(key, Value::create<T>(value));
		}

		// JSONReader ========================================================
		// this will read json into cpp json structures (Object,Array,Value)
		class JSONReader final : public Handler
		{
		public:
			using StackItem = std::pair<Value, std::string>;

			JSONReader() = default;

			[[nodiscard]] Value root() const;

			void jsonBeginArray() override;
			void jsonEndArray() override;
			void jsonBeginMap() override;
			void jsonEndMap() override;
			void jsonNull() override;
			void jsonString(const std::string& value) override;
			void jsonKey(const std::string& key) override;
			void jsonBool(const bool& value) override;
			void jsonInt32(const sint32& value) override;
			void jsonInt64(const sint64& value) override;
			void jsonReal32(const real32& value) override;
			void jsonReal64(const real64& value) override;
			void uaBool(sint64 element_count, Parser& parser) override;
			void uaReal16(sint64 element_count, Parser& parser) override;
			void uaReal32(sint64 element_count, Parser& parser) override;
			void uaReal64(sint64 element_count, Parser& parser) override;
			void uaInt8(sint64 element_count, Parser& parser) override;
			void uaInt16(sint64 element_count, Parser& parser) override;
			void uaInt32(sint64 element_count, Parser& parser) override;
			void uaInt64(sint64 element_count, Parser& parser) override;
			void uaUInt8(sint64 element_count, Parser& parser) override;
			void uaUInt16(sint64 element_count, Parser& parser) override;
			void uaString(sint64 element_count, Parser& parser) override;

		private:
			template<typename T>
			void jsonValue(const T& value);
			template<typename T, typename Source>
			void jsonUniformArray(sint64 element_count, Parser& parser);

			void pushContainer();
			void popContainer();
			Value root_;
			std::stack<StackItem> stack_;
			std::string next_key_;
		};

		template<typename T>
		void JSONReader::jsonValue(const T& value)
		{
			if (root_.isArray())
			{
				root_.asArray()->append(Value::create<T>(value));
			}
			else if (root_.isObject())
			{
				root_.asObject()->append(next_key_, Value::create<T>(value));
				next_key_.clear();
			}
			else if (root_.isNull() && stack_.empty())
			{
				root_ = Value::create<T>(value);
			}
			else
			{
				throw std::runtime_error("JSONReader::jsonValue has no active container");
			}
		}

		template<typename T, typename Source>
		void JSONReader::jsonUniformArray(sint64 element_count, Parser& parser)
		{
			if (element_count < 0)
				throw std::length_error("JSONReader::jsonUniformArray received a negative element count");
			const size_t count = static_cast<size_t>(element_count);
			if (count > std::numeric_limits<size_t>::max() / sizeof(T)
				|| count > std::numeric_limits<size_t>::max() / sizeof(Source))
			{
				throw std::length_error("JSONReader::jsonUniformArray allocation size overflow");
			}

			constexpr size_t uniform_type_index = variantIndex<T, Value::Variant>;
			std::vector<T> converted_data;
			converted_data.reserve(count);
			sint64 elements_remaining = element_count;
			constexpr size_t chunk_capacity = 4096;
			while (elements_remaining > 0)
			{
				const size_t chunk_size = static_cast<size_t>(
					std::min<sint64>(elements_remaining, chunk_capacity));
				std::vector<Source> source_data(chunk_size);
				parser.read(std::span<Source>(source_data));
				for (const Source& source_value : source_data)
					converted_data.push_back(static_cast<T>(source_value));
				elements_remaining -= static_cast<sint64>(chunk_size);
			}

			Value uniform_array_value = Value::createArray();
			ArrayPtr uniform_array = uniform_array_value.asArray();
			const size_t destination_bytes = count * sizeof(T);
			const auto bytes = std::span<const std::byte>(
				reinterpret_cast<const std::byte*>(converted_data.data()),
				destination_bytes);
			uniform_array->setUniformStorage(
				static_cast<int>(uniform_type_index),
				element_count,
				bytes);

			if (root_.isArray())
				root_.asArray()->append(uniform_array_value);
			else if (root_.isObject())
				root_.asObject()->append(next_key_, uniform_array_value);
			else if (root_.isNull() && stack_.empty())
				root_ = uniform_array_value;
			else
				throw std::runtime_error("JSONReader::jsonUniformArray has no active container");
		}


		// JSONWriter =========================================
		class JSONWriter final
		{
		public:
			explicit JSONWriter(std::ostream& output, bool binary = false)
			{
				if (binary)
					writer_ = std::make_unique<BinaryWriter>(output);
				else
					writer_ = std::make_unique<ASCIIWriter>(output);
			}

			~JSONWriter() = default;

			bool                    write( ObjectPtr object );
			bool                        write( ArrayPtr arr );
			bool                        write( Value &value );

			//used in combination with visitor pattern
			void operator()( bool value ){writer_->jsonBool(value);}
			void operator()( ubyte value ){writer_->jsonUInt8(value);}
			void operator()( sbyte value ){writer_->jsonInt8(value);}
			void operator()( sint32 value ){writer_->jsonInt32(value);}
			void operator()( sint64 value ){writer_->jsonInt64(value);}
			void operator()( real32 value ){writer_->jsonReal32(value);}
			void operator()( real64 value ){writer_->jsonReal64(value);}
			void operator()( const std::string &value ){writer_->jsonString(value);}
		private:
			std::unique_ptr<Writer> writer_;
		};

	}
}

