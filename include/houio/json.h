//
// lightweight json parser and writer. special about this one is that it supports binary json as
// specified by sideeffects' bgeo format (houdini12+).
// 
// todo: - transparent recasting of variants (e.g. value is int but queried as float...)
//       - support for uniform arrays
//       - support for ascii (read and write)
//
// bugs: Object::get with std::string is dodgy!
//	the following code crashes:
//		base::json::ObjectPtr o = base::json::Object::create();
//		std::string testShape = "asdasd";
//		o->append( "shape", base::json::Value::create( testShape ) );
//		int test = 0;
//		if( o->hasKey( "settings" ) )
//		{
//			test = o->get<int>("width");
//		}
//		std::string t = o->get<std::string>("shape");
//
#pragma once

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <vector>

#include <houio/Diagnostic.h>
#include <houio/HalfFloat.h>
#include <houio/types.h>
#include <ttl/var/variant.hpp>




namespace houio
{
	namespace json
	{
		struct Parser;

		// Scalar handlers preserve 32-bit and 64-bit integer and floating-point widths.
		struct Handler
		{
			virtual void                               jsonBeginArray() = 0;
			virtual void                                 jsonEndArray() = 0;
			virtual void                                 jsonBeginMap() = 0;
			virtual void                                   jsonEndMap() = 0;
			virtual void         jsonString( const std::string &value ) = 0;
			virtual void              jsonKey( const std::string &key ) = 0;
			virtual void                  jsonBool( const bool &value ) = 0;
			virtual void               jsonInt32( const sint32 &value ) = 0;
			virtual void               jsonInt64( const sint64 &value ) = 0;
			virtual void              jsonReal32( const real32 &value ) = 0;
			virtual void              jsonReal64( const real64 &value ) = 0;
			virtual void   uaBool( sint64 numElements, Parser *parser ) = 0;
			virtual void uaReal16( sint64 numElements, Parser *parser ) = 0;
			virtual void uaReal32( sint64 numElements, Parser *parser ) = 0;
			virtual void uaReal64( sint64 numElements, Parser *parser ) = 0;
			virtual void   uaInt8( sint64 numElements, Parser *parser ) = 0;
			virtual void  uaInt16( sint64 numElements, Parser *parser ) = 0;
			virtual void  uaInt32( sint64 numElements, Parser *parser ) = 0;
			virtual void  uaInt64( sint64 numElements, Parser *parser ) = 0;
			virtual void  uaUInt8( sint64 numElements, Parser *parser ) = 0;
			virtual void uaString( sint64 numElements, Parser *parser ) = 0;
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

			typedef ttl::var::variant<bool,           // bool
									  sbyte,          // int8
									  sword,          // int16
									  sint32,         // int32
									  sint64,         // int64
									  // TODO: real16
									  real32,         // real32
									  real64,         // real64
									  ubyte,          // uint8
									  uword,          // uint16
									  // no uint32?
									  std::string    // string
									  > Value;
			Token();
			void event( Parser *p, int key = false );


			Type                                type; // also encodes value type
			Value                              value;
			Type                              uaType; // additional type 
		};


		// UTILITY FUNCTIONS ======================================

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

			bool parse( std::istream *in, Handler *h );
			bool parse( std::istream *in, Handler *h, DiagnosticList *diagnostics );
			bool                          parseStream();
			void             validateSeekableInputSize();
			void                 validateTrailingInput();
			void validateUniformArrayPayload( Token::Type type, sint64 elementCount );
			bool                  readToken( Token &t );
			bool            readBinaryToken( Token &t, ubyte test = 0xff );
			bool     readASCIIToken( Token &t, char c );
			bool           readBinaryStringDefinition();
			bool                       undefineString();
			void                   pushState( State s );
			void                    setState( State s );
			void                             popState();
			[[noreturn]] void fail( DiagnosticCategory category, const std::string &message, sint64 offset = -1 );


			template<typename T>
			T                                    read();
			template<typename T>
			void     read( T *dst, sint64 numElements );
			sint64                         readLength();
			std::string              readBinaryString();
			std::string               readASCIIString();
			bool                  tryReadASCIIChar( char &value );


			// 
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
			T value{};
			read(&value, 1);
			return value;
		}
		template<typename T>
		void Parser::read( T *dst, sint64 numElements )
		{
			if( !stream )
				throw std::runtime_error( "Parser::read has no input stream" );
			if( numElements < 0 )
				throw std::length_error( "Parser::read received a negative element count" );
			if( numElements == 0 )
				return;
			if( !dst )
				throw std::invalid_argument( "Parser::read received a null destination" );

			const uint64 elementCount = static_cast<uint64>(numElements);
			const uint64 maximumBytes = static_cast<uint64>(std::numeric_limits<std::streamsize>::max());
			if( elementCount > maximumBytes / sizeof(T) )
				throw std::length_error( "Parser::read byte count exceeds streamsize range" );

			const std::streamsize byteCount = static_cast<std::streamsize>(elementCount * sizeof(T));
			if( byteOffset < 0 || byteOffset > limits.maxInputBytes
				|| byteCount > limits.maxInputBytes - byteOffset )
				fail(DiagnosticCategory::malformed_input, "Parser input byte limit exceeded", byteOffset);
			if( knownInputBytes >= 0 && (byteOffset > knownInputBytes
				|| byteCount > knownInputBytes - byteOffset) )
				fail(DiagnosticCategory::malformed_input, "Parser::read encountered truncated input", knownInputBytes);
			stream->read(reinterpret_cast<char*>(dst), byteCount);
			const std::streamsize bytesRead = stream->gcount();
			byteOffset += static_cast<sint64>(bytesRead);
			if( bytesRead != byteCount )
				fail(DiagnosticCategory::malformed_input, "Parser::read encountered truncated input", byteOffset);
		}


		// Writer ==================================================
		struct Writer
		{
			virtual                                   ~Writer(){};
			virtual void                       jsonBeginArray() = 0;
			virtual void                         jsonEndArray() = 0;
			virtual void                         jsonBeginMap() = 0;
			virtual void                           jsonEndMap() = 0;
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

		struct BinaryWriter : public Writer
		{
			BinaryWriter( std::ostream *out );

			bool jsonMagic();
			virtual void                               jsonBeginArray()override;
			virtual void                                 jsonEndArray()override;
			virtual void                                 jsonBeginMap()override;
			virtual void                                   jsonEndMap()override;
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
			bool                 jsonUniformArray( const std::vector<T> &data );
			template<typename T>
			bool          jsonUniformArray( const T *data, sint64 numElements );
			bool       jsonUniformArrayReal16( const uword *data, sint64 numElements );

			bool                                      writeId( Token::Type id );
			bool                            writeLength( const sint64 &length );


			template<typename T>
			bool                                            write( const T &v );
			template<typename T>
			bool                      write( const T *dst, sint64 numElements );



			// 
			std::ostream                                                *stream;
		};

		template<typename T>
		bool BinaryWriter::write( const T &v )
		{
			if( !stream )
				return false;
			stream->write(reinterpret_cast<const char*>(&v), static_cast<std::streamsize>(sizeof(T)));
			return stream->good();
		}
		template<typename T>
		bool BinaryWriter::write( const T *dst, sint64 numElements )
		{
			if( !stream )
				return false;
			if( numElements < 0 )
				throw std::length_error( "BinaryWriter::write received a negative element count" );
			if( numElements == 0 )
				return stream->good();
			if( !dst )
				throw std::invalid_argument( "BinaryWriter::write received a null source" );

			const uint64 elementCount = static_cast<uint64>(numElements);
			const uint64 maximumBytes = static_cast<uint64>(std::numeric_limits<std::streamsize>::max());
			if( elementCount > maximumBytes / sizeof(T) )
				throw std::length_error( "BinaryWriter::write byte count exceeds streamsize range" );

			const std::streamsize byteCount = static_cast<std::streamsize>(elementCount * sizeof(T));
			stream->write(reinterpret_cast<const char*>(dst), byteCount);
			return stream->good();
		}

		template<typename T>
		bool BinaryWriter::jsonUniformArray( const std::vector<T> &data )
		{
			Token::Type type = Token::JID_NULL;
			if( typeid(T) == typeid(real32) )
				type = Token::JID_REAL32;
			else if( typeid(T) == typeid(real64) )
				type = Token::JID_REAL64;
			else if( typeid(T) == typeid(bool) )
				type = Token::JID_BOOL;
			else if( typeid(T) == typeid(sbyte) )
				type = Token::JID_INT8;
			else if( typeid(T) == typeid(sword) )
				type = Token::JID_INT16;
			else if( typeid(T) == typeid(sint32) )
				type = Token::JID_INT32;
			else if( typeid(T) == typeid(sint64) )
				type = Token::JID_INT64;
			else if( typeid(T) == typeid(ubyte) )
				type = Token::JID_UINT8;
			else if( typeid(T) == typeid(uword) )
				type = Token::JID_UINT16;
			else
				throw std::runtime_error("BinaryWriter::jsonUniformArray: unable to handle type");
			
			if( data.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()) )
				throw std::length_error( "BinaryWriter::jsonUniformArray element count exceeds sint64 range" );
			const sint64 elementCount = static_cast<sint64>(data.size());
			writeId( Token::JID_UNIFORM_ARRAY );
			write<sbyte>( static_cast<sbyte>(type) );
			writeLength(elementCount);
			if( !data.empty() )
				write<T>(data.data(), elementCount);

			return true;
		}
		template<typename T>
		bool BinaryWriter::jsonUniformArray( const T *data, sint64 numElements )
		{
			Token::Type type = Token::JID_NULL;
			if( typeid(T) == typeid(real32) )
				type = Token::JID_REAL32;
			else if( typeid(T) == typeid(real64) )
				type = Token::JID_REAL64;
			else if( typeid(T) == typeid(bool) )
				type = Token::JID_BOOL;
			else if( typeid(T) == typeid(sbyte) )
				type = Token::JID_INT8;
			else if( typeid(T) == typeid(sword) )
				type = Token::JID_INT16;
			else if( typeid(T) == typeid(sint32) )
				type = Token::JID_INT32;
			else if( typeid(T) == typeid(sint64) )
				type = Token::JID_INT64;
			else if( typeid(T) == typeid(ubyte) )
				type = Token::JID_UINT8;
			else if( typeid(T) == typeid(uword) )
				type = Token::JID_UINT16;
			else
				throw std::runtime_error("BinaryWriter::jsonUniformArray: unable to handle type");
			
			if( numElements < 0 )
				throw std::length_error( "BinaryWriter::jsonUniformArray received a negative element count" );
			if( numElements > 0 && !data )
				throw std::invalid_argument( "BinaryWriter::jsonUniformArray received null data" );
			writeId( Token::JID_UNIFORM_ARRAY );
			write<sbyte>( static_cast<sbyte>(type) );
			writeLength(numElements);
			write<T>(data, numElements);

			return true;
		}


		// ASCIIWriter ==================================================
		struct ASCIIWriter : public Writer
		{
			ASCIIWriter( std::ostream *out );

			virtual void                               jsonBeginArray()override;
			virtual void                                 jsonEndArray()override;
			virtual void                                 jsonBeginMap()override;
			virtual void                                   jsonEndMap()override;
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

			int                                                     indentLevel;
			bool                                                      firstItem;
			bool                                                         gotKey;
			std::stack<Token::Type>                                       stack;
			std::string                                                  prefix;
			// 
			std::ostream                                                *stream;
		};



		// JSONLogger ==================================================
		struct JSONLogger : public Handler
		{
			JSONLogger() : out(std::cout), indentLevel(0)
			{
			}
			JSONLogger( std::ostream &outputStream ) : out(outputStream), indentLevel(0)
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

			virtual void uaBool( sint64 numElements, Parser *parser )
			{
				indent();
				if( numElements < 0 )
					throw std::length_error( "JSONLogger::uaBool received a negative element count" );
				sint64 elementsRemaining = numElements;
				out << "jsonArray [";
				while( elementsRemaining > 0 )
				{
					const uint32 bits = parser->read<uint32>();
					const int bitCount = static_cast<int>(std::min<sint64>(elementsRemaining, 32));
					for( int bitIndex=0;bitIndex<bitCount;++bitIndex )
						out << ((bits & (uint32(1) << bitIndex)) != 0 ? 1 : 0) << " ";
					elementsRemaining -= bitCount;
				}
				out << "]\n";
				std::flush(out);
			}

			virtual void uaReal16( sint64 numElements, Parser *parser )
			{
				indent();
				out << "jsonArray<real16> (" << numElements << ") [";
				for( sint64 elementIndex=0;elementIndex<numElements;++elementIndex )
					out << halfBitsToFloat(parser->read<uword>()) << " ";
				out << "]---\n";
				std::flush(out);
			}

			virtual void uaReal32( sint64 numElements, Parser *parser )
			{
				ua<real32>( numElements, parser, "<real32>" );
			}

			virtual void uaReal64( sint64 numElements, Parser *parser )
			{
				ua<real64>( numElements, parser, "<real64>" );
			}

			virtual void uaInt8( sint64 numElements, Parser *parser )
			{
				ua<sbyte>( numElements, parser, "<int8>" );
			}

			virtual void uaInt16( sint64 numElements, Parser *parser )
			{
				ua<sword>( numElements, parser, "<int16>" );
			}

			virtual void uaInt32( sint64 numElements, Parser *parser )
			{
				ua<sint32>( numElements, parser, "<int32>" );
			}

			virtual void uaInt64( sint64 numElements, Parser *parser )
			{
				ua<sint64>( numElements, parser, "<sint64>" );
			}

			virtual void uaUInt8( sint64 numElements, Parser *parser )
			{
				indent();
				if( numElements < 0 )
					throw std::length_error( "JSONLogger::uaUInt8 received a negative element count" );
				out << "jsonArray<uint8> [";
				sint64 elementsRemaining = numElements;
				while( elementsRemaining > 0 )
				{
					const size_t chunkSize = static_cast<size_t>(std::min<sint64>(elementsRemaining, 4096));
					std::vector<ubyte> data(chunkSize);
					parser->read<ubyte>(data.data(), static_cast<sint64>(chunkSize));
					for( ubyte value : data )
						out << static_cast<int>(value) << " ";
					elementsRemaining -= static_cast<sint64>(chunkSize);
				}
				out << "]\n";
				std::flush(out);
			}

			virtual void uaString( sint64 numElements, Parser *parser )
			{
				indent();
				if( numElements < 0 )
					throw std::length_error( "JSONLogger::uaString received a negative element count" );
				out << "jsonArray<string> [";
				for(sint64 i=0;i<numElements;++i)
					out << parser->readBinaryString() << " ";
				out << "]\n";
				std::flush(out);
			}

			template<typename T>
			void ua( sint64 numElements, Parser *parser, std::string type = "" )
			{
				indent();
				if( numElements < 0 )
					throw std::length_error( "JSONLogger uniform array received a negative element count" );
				out << "jsonArray"<<type<<" ("<< numElements << ") [";
				sint64 elementsRemaining = numElements;
				while( elementsRemaining > 0 )
				{
					const size_t chunkSize = static_cast<size_t>(std::min<sint64>(elementsRemaining, 4096));
					std::vector<T> data(chunkSize);
					parser->read<T>(data.data(), static_cast<sint64>(chunkSize));
					for( const T &value : data )
						out << value << " ";
					elementsRemaining -= static_cast<sint64>(chunkSize);
				}
				out << "]---\n";
				std::flush(out);
			}

			std::ostream &out;
			int indentLevel;
		};

		// JSONCPP ==================================================

		// Value -------------

		struct Array;
		typedef std::shared_ptr<Array> ArrayPtr;
		struct Object;
		typedef std::shared_ptr<Object> ObjectPtr;

		struct Value
		{

			typedef ttl::var::variant<bool,           // order
									  sint32,         // must
									  real32,         // not
									  real64,         // change
									  std::string,    // !!!!!! - because index is used in is* methods
									  ubyte,          // also: if you add something here you need to update Value::cpyTo, JSONWriter::operators
									  sint64
									  > Variant;

			Value();

			bool                           isNull()const;
			bool                          isArray()const;
			bool                         isObject()const;
			bool                         isString()const;

			template<typename T>
			const T                           as() const;

			void                 cpyTo( char *dst )const;

			ArrayPtr                           asArray();
			ObjectPtr                         asObject();

			Variant                        &getVariant();

			static Value                   createArray();
			static Value     createArray(ArrayPtr array);
			static Value                  createObject();
			static Value   createObject( ObjectPtr obj );

			template<typename T>
			static Value        create( const T &value );


		private:
			enum Type
			{
				TYPE_NULL,
				TYPE_VALUE,
				TYPE_OBJECT,
				TYPE_ARRAY
			};
			Type                               m_type; // value, array or object?
			Variant                           m_value;
			ObjectPtr                        m_object;
			ArrayPtr                          m_array;

			friend                             Object;
			friend                             Array;
		};


		// VariantConverter ================================

		// In order to conveniently extract data from variants we need to be able to convert between 
		// all the different variant types. This is done by using the visitor pattern in the as function of
		// the varient class. VariantConverter is the visitor which will its ()operator get called for all the different
		// variant types. The Converter is a templated class which has the destination type as its template argument
		// D. The operator is templated as well with the argument type (the type of the varient) as template argument.
		// By default a simple cast is done. Now to handle special cases where such a cast fails we do the following steps:
		// 1. create a specialized class template which has the destination type as template argument D
		// 2. overload the () operator with the variant type which produces the conflict and resolve it in the
		// definition of that overloaded function

		template<typename D>
		struct VariantConverter
		{
			D &dest;
			VariantConverter( D &_dest ) : dest(_dest){}
			void operator()( const std::string &value )
			{
				dest = fromString<D>(value);
			}
			template< typename T >
			void operator()( T d )
			{
				dest = static_cast<D>(d);
			}
		};

		template<>
		struct VariantConverter<bool>
		{
			typedef bool t_dest;
			t_dest &dest;
			VariantConverter( t_dest &_dest ) : dest(_dest){}

			void operator()(std::string /*x*/)
			{
				dest = false;
			}

			template< typename T >
			void operator()( T d )
			{
				dest = (t_dest)d;
			}
		};

		template<>
		struct VariantConverter<float>
		{
			typedef float t_dest;
			t_dest &dest;
			VariantConverter( t_dest &_dest ) : dest(_dest){}

			void operator()(std::string /*x*/)
			{
				//dest = false;
			}

			template< typename T >
			void operator()( T d )
			{
				dest = (t_dest)d;
			}
		};

		template<>
		struct VariantConverter<double>
		{
			typedef double t_dest;
			t_dest &dest;
			VariantConverter( t_dest &_dest ) : dest(_dest){}

			void operator()(std::string /*x*/)
			{
				// Keep the default destination value for incompatible string input.
			}

			template< typename T >
			void operator()( T d )
			{
				dest = static_cast<t_dest>(d);
			}
		};

		template<>
		struct VariantConverter<int>
		{
			typedef int t_dest;
			t_dest &dest;
			VariantConverter( t_dest &_dest ) : dest(_dest){}

			void operator()(std::string /*x*/)
			{
				//dest = false;
			}

			template< typename T >
			void operator()( T d )
			{
				dest = (t_dest)d;
			}
		};

		template<>
		struct VariantConverter<ubyte>
		{
			typedef ubyte t_dest;
			t_dest &dest;
			VariantConverter( t_dest &_dest ) : dest(_dest){}

			void operator()(std::string /*x*/)
			{
				//dest = false;
			}

			template< typename T >
			void operator()( T d )
			{
				dest = (t_dest)d;
			}
		};

		template<>
		struct VariantConverter<std::string>
		{
			typedef std::string t_dest;
			t_dest &dest;
			VariantConverter( t_dest &_dest ) : dest(_dest){}

			void operator()(std::string x)
			{
				dest = x;
			}

			template< typename T >
			void operator()( T d )
			{
				std::ostringstream stream;
				stream << d;
				dest = stream.str();
			}
		};


		template<typename T>
		const T Value::as()const
		{
			T dest = T();
			VariantConverter<T> conv(dest);
			ttl::var::apply_visitor(conv, m_value);
			return dest;
		}


		template<typename T>
		Value Value::create( const T &value )
		{
			Value v;
			v.m_type = TYPE_VALUE;
			v.m_value = value;
			return v;
		}


		// Array -------------
		struct Array
		{
			Array();
			~Array();

			static ArrayPtr               create();

			template<typename T>
			const T                  get( const int index );
			ObjectPtr                getObject( int index );
			ArrayPtr                  getArray( int index );

			Value               getValue( const int index );

			sint64                              size()const;
			bool                           isUniform()const;



			template<typename T>
			void                     appendValue( T value );
			//void                     append( Value &value );
			void                     append( const Value &value );
			void                     append(ObjectPtr &object );
			void                     append(ArrayPtr &array );

		//private:
			std::vector<Value>                     m_values;
			bool                                m_isUniform;
			unsigned char*                    m_uniformdata;
			sint64                     m_numUniformElements;
			int                               m_uniformType; // integer which equals Variant::which()
		};


		template<typename T>
		const T Array::get( const int index )
		{
			return getValue(index).as<T>();
		}

		template<typename T>
		void Array::appendValue( T value )
		{
			append(Value::create<T>(value));
		}

		// Object -------------
		struct Object
		{
			static ObjectPtr                                           create();
			bool                               hasKey( const std::string &key );

			template<typename T>
			T                        get( const std::string &key, T def = T() );
			ObjectPtr                       getObject( const std::string &key );
			ArrayPtr                         getArray( const std::string &key );


			Value                            getValue( const std::string &key );
			void                      getKeys( std::vector<std::string> &keys );
			sint64                                                  size()const;

			template<typename T>
			void appendValue( const std::string &key, const T& value );
			void                 append( const std::string &key, const Value &value );
			void             append( const std::string &key, ObjectPtr object );
			void             append( const std::string &key, ArrayPtr array );
		//private:
			std::map<std::string, Value>                               m_values;

		};


		template<typename T>
		T Object::get( const std::string &key, T def )
		{
			T result = def;
			std::map<std::string, Value>::iterator it = m_values.find( key );
			if( it != m_values.end())
				result = it->second.as<T>();
			return result;
		}

		template<typename T>
		void Object::appendValue( const std::string &key, const T& value )
		{
			append(key, Value::create<T>(value));
		}

		// JSONReader ========================================================
		// this will read json into cpp json structures (Object,Array,Value)
		struct JSONReader : public Handler
		{
			JSONReader();

			Value                                                getRoot();

			virtual void                                  jsonBeginArray();
			virtual void                                    jsonEndArray();
			virtual void                                    jsonBeginMap();
			virtual void                                      jsonEndMap();
			virtual void            jsonString( const std::string &value );
			virtual void                 jsonKey( const std::string &key );
			virtual void                     jsonBool( const bool &value );
			virtual void                  jsonInt32( const sint32 &value );
			virtual void                  jsonInt64( const sint64 &value );
			virtual void                 jsonReal32( const real32 &value );
			virtual void                 jsonReal64( const real64 &value );
			virtual void      uaBool( sint64 numElements, Parser *parser );
			virtual void    uaReal16( sint64 numElements, Parser *parser );
			virtual void    uaReal32( sint64 numElements, Parser *parser );
			virtual void    uaReal64( sint64 numElements, Parser *parser );
			virtual void      uaInt8( sint64 numElements, Parser *parser );
			virtual void     uaInt16( sint64 numElements, Parser *parser );
			virtual void     uaInt32( sint64 numElements, Parser *parser );
			virtual void     uaInt64( sint64 numElements, Parser *parser );
			virtual void     uaUInt8( sint64 numElements, Parser *parser );
			virtual void    uaString( sint64 numElements, Parser *parser );


		private:
			typedef std::pair<Value, std::string> StackItem; // holds value and nextKey

			template<typename T>
			void                               jsonValue( const T &value );
			template<typename T, typename S>
			void              jsonUA( sint64 numElements, Parser *parser );

			void                                                    push();
			void                                                     pop();
			Value                                                   m_root;
			std::stack<StackItem>                                  m_stack; // used during sax parsing
			std::string                                            nextKey; // used during sax parsing
		};

		template<typename T>
		void JSONReader::jsonValue( const T &value )
		{
			if( m_root.isArray() )
				m_root.asArray()->append(Value::create<T>(value));
			else
			if( m_root.isObject() )
			{
				m_root.asObject()->append(nextKey, Value::create<T>(value));
				nextKey = "JSONReader::jsonValue:invalid key";
			}else if( m_root.isNull() && m_stack.empty() )
			{
				m_root = Value::create<T>(value);
			}else
			{
				throw std::runtime_error("JSONReader::jsonValue: unknown container");
			}
		}

		template<typename T, typename S>
		void JSONReader::jsonUA( sint64 numElements, Parser *parser )
		{
			if( numElements < 0 )
				throw std::length_error( "JSONReader::jsonUA received a negative element count" );
			const size_t elementCount = static_cast<size_t>(numElements);
			if( elementCount > std::numeric_limits<size_t>::max() / sizeof(T)
				|| elementCount > std::numeric_limits<size_t>::max() / sizeof(S) )
				throw std::length_error( "JSONReader::jsonUA allocation size overflow" );

			typedef ttl::meta::find_equivalent_type<const T&, Value::Variant::list> found;
			std::vector<T> convertedData;
			sint64 elementsRemaining = numElements;
			constexpr size_t chunkCapacity = 4096;
			while( elementsRemaining > 0 )
			{
				const size_t chunkSize = static_cast<size_t>(std::min<sint64>(elementsRemaining, chunkCapacity));
				std::vector<S> sourceData(chunkSize);
				parser->read<S>(sourceData.data(), static_cast<sint64>(chunkSize));
				const size_t previousSize = convertedData.size();
				convertedData.resize(previousSize + chunkSize);
				for( size_t elementIndex=0;elementIndex<chunkSize;++elementIndex )
					convertedData[previousSize + elementIndex] = static_cast<T>(sourceData[elementIndex]);
				elementsRemaining -= static_cast<sint64>(chunkSize);
			}

			Value v = Value::createArray();
			ArrayPtr ua = v.asArray();
			ua->m_isUniform = true;
			ua->m_numUniformElements = numElements;
			ua->m_uniformType = found::index;

			const size_t destinationBytes = elementCount * sizeof(T);
			ua->m_uniformdata = static_cast<unsigned char*>(std::malloc(destinationBytes));
			if( destinationBytes > 0 && !ua->m_uniformdata )
				throw std::bad_alloc();
			if( destinationBytes > 0 )
				std::memcpy(ua->m_uniformdata, convertedData.data(), destinationBytes);

			if( m_root.isArray() )
				m_root.asArray()->append(v);
			else if( m_root.isObject() )
				m_root.asObject()->append(nextKey, v);
		}


		// JSONWriter =========================================
		struct JSONWriter
		{
			JSONWriter( std::ostream *out, bool binary = false )
			{
				if(binary)
					m_writer = new BinaryWriter(out);
				else
					m_writer = new ASCIIWriter(out);
			}

			~JSONWriter()
			{
				delete m_writer;
			}

			bool                    write( ObjectPtr object );
			bool                        write( ArrayPtr arr );
			bool                        write( Value &value );

			//used in combination with visitor pattern
			void operator()( bool value ){m_writer->jsonBool(value);}
			void operator()( ubyte value ){m_writer->jsonUInt8(value);}
			void operator()( sbyte value ){m_writer->jsonInt8(value);}
			void operator()( sint32 value ){m_writer->jsonInt32(value);}
			void operator()( sint64 value ){m_writer->jsonInt64(value);}
			void operator()( real32 value ){m_writer->jsonReal32(value);}
			void operator()( real64 value ){m_writer->jsonReal64(value);}
			void operator()( const std::string &value ){m_writer->jsonString(value);}
		private:
			Writer                                  *m_writer;
		};

	}
}

