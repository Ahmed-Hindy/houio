

#include <houio/json.h>
#include <algorithm>
#include <cctype>
#include <cstring>







namespace houio
{
	namespace json
	{
		namespace
		{
			bool isDefinedTokenType( ubyte value )
			{
				switch( value )
				{
				case Token::JID_NULL:
				case Token::JID_MAP_BEGIN:
				case Token::JID_MAP_END:
				case Token::JID_ARRAY_BEGIN:
				case Token::JID_ARRAY_END:
				case Token::JID_BOOL:
				case Token::JID_INT8:
				case Token::JID_INT16:
				case Token::JID_INT32:
				case Token::JID_INT64:
				case Token::JID_REAL16:
				case Token::JID_REAL32:
				case Token::JID_REAL64:
				case Token::JID_UINT8:
				case Token::JID_UINT16:
				case Token::JID_STRING:
				case Token::JID_FALSE:
				case Token::JID_TRUE:
				case Token::JID_TOKENDEF:
				case Token::JID_TOKENREF:
				case Token::JID_TOKENUNDEF:
				case Token::JID_UNIFORM_ARRAY:
				case Token::JID_KEY_SEPARATOR:
				case Token::JID_VALUE_SEPARATOR:
				case Token::JID_MAGIC:
					return true;
				default:
					return false;
				}
			}

			bool isSupportedUniformArrayType( ubyte value )
			{
				switch( value )
				{
				case Token::JID_BOOL:
				case Token::JID_INT8:
				case Token::JID_INT16:
				case Token::JID_INT32:
				case Token::JID_INT64:
				case Token::JID_REAL16:
				case Token::JID_REAL32:
				case Token::JID_REAL64:
				case Token::JID_UINT8:
				case Token::JID_STRING:
					return true;
				default:
					return false;
				}
			}
		}

		// Token ==================================================
		Token::Token() : type(JID_NULL), value(0), uaType(JID_NULL)
		{
		}

		void Token::event( Parser *p, int key )
		{
			if (type == JID_ARRAY_BEGIN)
				p->handler->jsonBeginArray();
			else if (type == JID_ARRAY_END)
				p->handler->jsonEndArray();
			if (type == JID_MAP_BEGIN)
				p->handler->jsonBeginMap();
			else if (type == JID_MAP_END)
				p->handler->jsonEndMap();
			else if (type == JID_STRING)
			{
				if( key )
					p->handler->jsonKey( ttl::var::get<std::string>( value ) );
				else
					p->handler->jsonString( ttl::var::get<std::string>( value ) );
			}
			else if (type == JID_BOOL)
				p->handler->jsonBool( ttl::var::get<bool>( value ) );
			else if (type == JID_INT8)
				p->handler->jsonInt32( ttl::var::get<sbyte>( value ) );
			else if (type == JID_INT16)
				p->handler->jsonInt32( ttl::var::get<sword>( value ) );
			else if (type == JID_INT32)
				p->handler->jsonInt32( ttl::var::get<sint32>( value ) );
			else if (type == JID_INT64)
				p->handler->jsonInt64( ttl::var::get<sint64>( value ) );
			else if (type == JID_REAL16)
				p->handler->jsonReal32( ttl::var::get<real32>( value ) );
			else if (type == JID_REAL32)
				p->handler->jsonReal32( ttl::var::get<real32>( value ) );
			else if (type == JID_REAL64)
				p->handler->jsonReal64( ttl::var::get<real64>( value ) );
			else if (type == JID_UINT8)
				p->handler->jsonInt32( ttl::var::get<ubyte>( value ) );
			else if (type == JID_UINT16)
				p->handler->jsonInt32( ttl::var::get<uword>( value ) );
			else if (type == JID_UNIFORM_ARRAY)
			{
				sint64 numElements = ttl::var::get<sint64>( value );
				switch( uaType )
				{
				case Token::JID_BOOL:p->handler->uaBool( numElements, p );break;
				case Token::JID_INT8:p->handler->uaInt8( numElements, p );break;
				case Token::JID_INT16:p->handler->uaInt16( numElements, p );break;
				case Token::JID_INT32:p->handler->uaInt32( numElements, p );break;
				case Token::JID_INT64:p->handler->uaInt64( numElements, p );break;
				case Token::JID_REAL16:p->handler->uaReal16( numElements, p );break;
				case Token::JID_REAL32:p->handler->uaReal32( numElements, p );break;
				case Token::JID_REAL64:p->handler->uaReal64( numElements, p );break;
				case Token::JID_UINT8:p->handler->uaUInt8( numElements, p );break;
				case Token::JID_STRING:p->handler->uaString( numElements, p );break;
				case Token::JID_NULL:
				case Token::JID_MAP_BEGIN:
				case Token::JID_MAP_END:
				case Token::JID_ARRAY_BEGIN:
				case Token::JID_ARRAY_END:
				case Token::JID_FALSE:
				case Token::JID_TRUE:
				case Token::JID_TOKENDEF:
				case Token::JID_TOKENREF:
				case Token::JID_TOKENUNDEF:
				case Token::JID_UNIFORM_ARRAY:
				case Token::JID_KEY_SEPARATOR:
				case Token::JID_VALUE_SEPARATOR:
				case Token::JID_MAGIC:
				case Token::JID_UINT16:
				default:
					p->fail(DiagnosticCategory::unsupported_input,
						"Token::event encountered an unsupported uniform-array type");
				};
			}




		}





		// Parser ==================================================

		Parser::Parser() : Parser(ParserLimits())
		{
		}

		Parser::Parser( const ParserLimits &parserLimits ) :
			state(STATE_INVALID),
			handler(nullptr),
			stream(nullptr),
			binary(false),
			byteOffset(0),
			tokenOffset(-1),
			knownInputBytes(-1),
			limits(parserLimits)
		{
			if( limits.maxInputBytes < 0 || limits.maxStringBytes < 0 || limits.maxUniformArrayElements < 0 )
				throw std::invalid_argument( "Parser limits cannot be negative" );
			if( limits.maxNestingDepth == 0 )
				throw std::invalid_argument( "Parser nesting depth must be greater than zero" );
		}

		bool Parser::parse( std::istream *in, Handler *h )
		{
			return parse(in, h, nullptr);
		}

		bool Parser::parse( std::istream *in, Handler *h, DiagnosticList *outputDiagnostics )
		{
			if( !in || !h || !in->good() )
			{
				Diagnostic diagnostic{DiagnosticSeverity::error, DiagnosticCategory::io,
					"Parser requires a readable input stream and a valid handler", 0, ""};
				if( outputDiagnostics )
					appendDiagnostic(outputDiagnostics, std::move(diagnostic));
				return false;
			}

			state = STATE_START;
			stateStack = std::stack<State>();
			strings.clear();
			binary = false;
			byteOffset = 0;
			tokenOffset = -1;
			knownInputBytes = -1;
			handler = h;
			stream = in;

			try
			{
				validateSeekableInputSize();
				if( !parseStream() )
					fail(DiagnosticCategory::malformed_input, "Parser state machine rejected the input", byteOffset);
				validateTrailingInput();
				return true;
			}
			catch( const DiagnosticException &exception )
			{
				if( outputDiagnostics )
				{
					Diagnostic diagnostic = exception.diagnostic();
					if( diagnostic.byteOffset < 0 )
						diagnostic.byteOffset = byteOffset;
					appendDiagnostic(outputDiagnostics, std::move(diagnostic));
					return false;
				}
				throw;
			}
			catch( const std::exception &exception )
			{
				Diagnostic diagnostic{DiagnosticSeverity::error, DiagnosticCategory::malformed_input,
					exception.what(), byteOffset, ""};
				if( outputDiagnostics )
				{
					appendDiagnostic(outputDiagnostics, std::move(diagnostic));
					return false;
				}
				throw;
			}
		}

		[[noreturn]] void Parser::fail( DiagnosticCategory category, const std::string &message, sint64 offset )
		{
			if( offset < 0 )
				offset = byteOffset;
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, category, message, offset, ""});
		}

		void Parser::validateSeekableInputSize()
		{
			const std::ios::iostate originalState = stream->rdstate();
			const std::istream::pos_type originalPosition = stream->tellg();
			if( originalPosition == std::istream::pos_type(-1) )
			{
				stream->clear(originalState);
				return;
			}

			stream->clear();
			stream->seekg(0, std::ios::end);
			const std::istream::pos_type endPosition = stream->tellg();
			if( endPosition == std::istream::pos_type(-1) || endPosition < originalPosition )
			{
				stream->clear();
				stream->seekg(originalPosition);
				stream->clear(originalState);
				return;
			}

			const std::streamoff inputBytes = endPosition - originalPosition;
			stream->clear();
			stream->seekg(originalPosition);
			if( !stream->good() )
				fail(DiagnosticCategory::io, "Parser could not restore the input stream position", 0);
			stream->clear(originalState);

			if( inputBytes > std::numeric_limits<sint64>::max() )
				fail(DiagnosticCategory::malformed_input, "Parser input size exceeds the supported range", 0);
			knownInputBytes = static_cast<sint64>(inputBytes);
			if( knownInputBytes > limits.maxInputBytes )
				fail(DiagnosticCategory::malformed_input, "Parser input byte limit exceeded", 0);
		}

		void Parser::validateTrailingInput()
		{
			bool lineComment = false;
			char value = 0;
			while( stream->get(value) )
			{
				if( byteOffset >= limits.maxInputBytes )
					fail(DiagnosticCategory::malformed_input, "Parser input byte limit exceeded", byteOffset);
				++byteOffset;
				if( binary )
					fail(DiagnosticCategory::malformed_input, "Parser encountered trailing binary data", byteOffset - 1);

				if( lineComment )
				{
					if( value == '\n' || value == '\r' )
						lineComment = false;
					continue;
				}
				if( std::isspace(static_cast<unsigned char>(value)) )
					continue;
				if( value == '/' )
				{
					char secondSlash = 0;
					if( !stream->get(secondSlash) )
						fail(DiagnosticCategory::malformed_input, "Parser encountered trailing data", byteOffset - 1);
					if( byteOffset >= limits.maxInputBytes )
						fail(DiagnosticCategory::malformed_input, "Parser input byte limit exceeded", byteOffset);
					++byteOffset;
					if( secondSlash != '/' )
						fail(DiagnosticCategory::malformed_input, "Parser encountered trailing data", byteOffset - 2);
					lineComment = true;
					continue;
				}
				fail(DiagnosticCategory::malformed_input, "Parser encountered trailing data", byteOffset - 1);
			}
			if( stream->bad() )
				fail(DiagnosticCategory::io, "Parser failed while checking trailing input", byteOffset);
		}

		void Parser::validateUniformArrayPayload( Token::Type type, sint64 elementCount )
		{
			if( elementCount < 0 )
				fail(DiagnosticCategory::malformed_input, "Parser uniform array has a negative element count");
			if( type == Token::JID_STRING )
				return;

			const uint64 count = static_cast<uint64>(elementCount);
			auto checkedPayloadSize = [&]( uint64 elementSize )
			{
				if( elementSize != 0 && count > std::numeric_limits<uint64>::max() / elementSize )
					fail(DiagnosticCategory::malformed_input, "Parser uniform-array payload size overflow");
				return count * elementSize;
			};

			uint64 payloadBytes = 0;
			switch( type )
			{
			case Token::JID_BOOL:
				if( count > std::numeric_limits<uint64>::max() - 31 )
					fail(DiagnosticCategory::malformed_input, "Parser uniform-array payload size overflow");
				payloadBytes = ((count + 31) / 32) * sizeof(uint32);
				break;
			case Token::JID_INT8:
			case Token::JID_UINT8:
				payloadBytes = count;
				break;
			case Token::JID_INT16:
			case Token::JID_REAL16:
				payloadBytes = checkedPayloadSize(sizeof(uword));
				break;
			case Token::JID_INT32:
			case Token::JID_REAL32:
				payloadBytes = checkedPayloadSize(sizeof(sint32));
				break;
			case Token::JID_INT64:
			case Token::JID_REAL64:
				payloadBytes = checkedPayloadSize(sizeof(sint64));
				break;
			default:
				fail(DiagnosticCategory::unsupported_input, "Parser encountered an unsupported uniform-array type");
			}

			if( payloadBytes > static_cast<uint64>(std::numeric_limits<sint64>::max()) )
				fail(DiagnosticCategory::malformed_input, "Parser uniform-array payload exceeds the supported range");
			const sint64 signedPayloadBytes = static_cast<sint64>(payloadBytes);
			if( byteOffset < 0 || byteOffset > limits.maxInputBytes
				|| signedPayloadBytes > limits.maxInputBytes - byteOffset )
				fail(DiagnosticCategory::malformed_input, "Parser input byte limit exceeded", byteOffset);
			if( knownInputBytes >= 0 && (byteOffset > knownInputBytes
				|| signedPayloadBytes > knownInputBytes - byteOffset) )
				fail(DiagnosticCategory::malformed_input, "Parser uniform-array payload exceeds the available input", knownInputBytes);
		}

		bool Parser::parseStream()
		{
			Token t;

			while(state != STATE_COMPLETE)
			{
				bool popped = false;

				if( !readToken( t ) )
					return false;

				// expecting values ---------------------
				if( (state == STATE_START)||
					(state == STATE_ARRAY_START)||
					(state == STATE_ARRAY_NEED_VALUE)||
					(state == STATE_MAP_NEED_VALUE))
				{
					State newState = STATE_INVALID;
					const bool scalarToken =
						t.type == Token::JID_NULL
						|| t.type == Token::JID_BOOL
						|| t.type == Token::JID_INT8
						|| t.type == Token::JID_INT16
						|| t.type == Token::JID_INT32
						|| t.type == Token::JID_INT64
						|| t.type == Token::JID_REAL16
						|| t.type == Token::JID_REAL32
						|| t.type == Token::JID_REAL64
						|| t.type == Token::JID_UINT8
						|| t.type == Token::JID_UINT16
						|| t.type == Token::JID_STRING
						|| t.type == Token::JID_UNIFORM_ARRAY;

					if( t.type == Token::JID_ARRAY_BEGIN )
						newState = STATE_ARRAY_START;
					else if( t.type == Token::JID_MAP_BEGIN )
						newState = STATE_MAP_START;
					else if( (t.type == Token::JID_ARRAY_END)||(t.type == Token::JID_MAP_END) )
					{
						if( stateStack.empty() )
							fail(DiagnosticCategory::malformed_input, "Parser encountered an unmatched closing token", tokenOffset);
						if( state == STATE_MAP_NEED_VALUE )
							fail(DiagnosticCategory::malformed_input, "Parser encountered a map end before its value", tokenOffset);
						if( !binary && state == STATE_ARRAY_NEED_VALUE )
							fail(DiagnosticCategory::malformed_input, "Parser encountered an array end after a trailing separator", tokenOffset);
						const bool arrayState = state == STATE_ARRAY_START || state == STATE_ARRAY_NEED_VALUE;
						if( (arrayState && t.type != Token::JID_ARRAY_END)
							|| (!arrayState && t.type != Token::JID_MAP_END) )
							fail(DiagnosticCategory::malformed_input, "Parser encountered a mismatched closing token", tokenOffset);
						popState();
						popped = true;
					}else if( !scalarToken )
					{
						fail(DiagnosticCategory::malformed_input, "Parser expected a value", tokenOffset);
					}

					const bool topLevelScalar = state == STATE_START && scalarToken;
					if( newState != STATE_INVALID )
						pushState( newState );
					else if( !popped && !topLevelScalar )
					{
						if( state == STATE_MAP_NEED_VALUE )
							setState( STATE_MAP_GOT_VALUE );
						else
							setState( STATE_ARRAY_GOT_VALUE );
					}

					// call event handler for current token
					t.event( this );
					if( topLevelScalar )
					{
						state = STATE_COMPLETE;
						return true;
					}
				}else
				// expecting keys ---------------------
				if( (state == STATE_MAP_START)||
					(state == STATE_MAP_NEED_KEY) )
				{
					// if we got a key
					if( t.type == Token::JID_STRING )
					{
						// we will expect a key value seperator next
						setState( STATE_MAP_SEPERATOR );
						t.event( this, 1 );
					}else
					if( t.type == Token::JID_MAP_END )
					{
						if( !binary && state == STATE_MAP_NEED_KEY )
							fail(DiagnosticCategory::malformed_input, "Parser encountered a map end after a trailing separator", tokenOffset);
						popState();
						t.event( this );
						if( stateStack.empty() )
							return true;
					}else
					{
						fail(DiagnosticCategory::malformed_input, "Parser expected a map key or map end", tokenOffset);
					}
				}else
				if( state == STATE_MAP_SEPERATOR )
				{
					if( t.type == Token::JID_KEY_SEPARATOR )
						setState( STATE_MAP_NEED_VALUE );
					else
						fail(DiagnosticCategory::malformed_input, "Parser expected a map key separator", tokenOffset);
				}else
				if( state == STATE_MAP_GOT_VALUE )
				{
					if( t.type == Token::JID_MAP_END )
					{
						popState();
						t.event( this );
						if( stateStack.empty() )
							return true;
					}else
					if( t.type == Token::JID_VALUE_SEPARATOR )
						setState( STATE_MAP_NEED_KEY );
					else
						fail(DiagnosticCategory::malformed_input, "Parser expected a map separator or map end", tokenOffset);
				}else
				if( state == STATE_ARRAY_GOT_VALUE )
				{
					if( t.type == Token::JID_ARRAY_END )
					{
						popState();
						t.event( this );
						if( stateStack.empty() )
							return true;
					}else
					if( t.type == Token::JID_VALUE_SEPARATOR )
						setState( STATE_ARRAY_NEED_VALUE );
					else
						fail(DiagnosticCategory::malformed_input, "Parser expected an array separator or array end", tokenOffset);
				}

			}


			return true;
		}

		bool Parser::readToken( Token &t )
		{
			tokenOffset = byteOffset;
			ubyte c = read<ubyte>();

			if(!stream->good())
				return false;
			
			// binary?
			if( (Token::Type)c == Token::JID_MAGIC )
			{
				// check
				uint32 magic = read<uint32>();
				if( magic == 0x624a534e )  // BINARY_MAGIC = 0x624a534e
					binary = true;
				else
					fail(DiagnosticCategory::malformed_input, "Parser encountered an invalid binary JSON magic value", tokenOffset);
				return readToken(t);
			}else
				if( binary )
				{
					return readBinaryToken( t, c );
				}
				else
					return readASCIIToken( t, c );

			return true;
		}

		bool Parser::readBinaryToken( Token &t, ubyte test )
		{
			auto assignTokenType = [&]( ubyte value, sint64 offset )
			{
				if( !isDefinedTokenType(value) )
				{
					std::ostringstream message;
					message << "Parser encountered unknown binary token id " << static_cast<int>(value);
					fail(DiagnosticCategory::malformed_input, message.str(), offset);
				}
				t.type = static_cast<Token::Type>(value);
			};

			assignTokenType(test, tokenOffset);
			while( (t.type == Token::JID_TOKENDEF) || (t.type == Token::JID_TOKENUNDEF) )
			{
				if(t.type == Token::JID_TOKENDEF)
					readBinaryStringDefinition();
				else
					undefineString();
				const ubyte nextToken = read<ubyte>();
				assignTokenType(nextToken, byteOffset - 1);
			};

			switch( t.type )
			{
			case Token::JID_ARRAY_BEGIN:
			case Token::JID_ARRAY_END:
			case Token::JID_MAP_BEGIN:
			case Token::JID_MAP_END:
				return true;
			case Token::JID_TOKENREF:
				{
					// Common strings may be encoded using a shared string table.
					const sint64 stringId = readLength();
					const auto stringIterator = strings.find(stringId);
					if( stringIterator == strings.end() )
						fail(DiagnosticCategory::malformed_input, "Parser::readBinaryToken referenced an undefined string token", tokenOffset);
					t.type = Token::JID_STRING;
					t.value = stringIterator->second;
				}return true;
			case Token::JID_TRUE: t.value = true;t.type = Token::JID_BOOL; return true;
			case Token::JID_FALSE: t.value = false;t.type = Token::JID_BOOL; return true;
			case Token::JID_BOOL: t.value = read<sbyte>() != 0;t.type = Token::JID_BOOL; return true;
			case Token::JID_INT8: t.value = read<sbyte>();return true;
			case Token::JID_INT16: t.value = read<sword>();return true;
			case Token::JID_INT32: t.value = read<sint32>();return true;
			case Token::JID_INT64: t.value = read<sint64>();return true;
			case Token::JID_REAL16: t.value = halfBitsToFloat(read<uword>());return true;
			case Token::JID_REAL32: t.value = read<real32>();return true;
			case Token::JID_REAL64: t.value = read<real64>();return true;
			case Token::JID_UINT8: t.value = read<ubyte>();return true;
			case Token::JID_UINT16: t.value = read<uword>();return true;
			case Token::JID_STRING: t.value = readBinaryString();return true;
			case Token::JID_UNIFORM_ARRAY:
				{
					const ubyte uniformType = read<ubyte>();
					if( !isSupportedUniformArrayType(uniformType) )
						fail(DiagnosticCategory::unsupported_input, "Parser::readBinaryToken encountered an unsupported uniform-array type", byteOffset - 1);
					t.uaType = static_cast<Token::Type>(uniformType);

					const sint64 elementCount = readLength();
					if( elementCount > limits.maxUniformArrayElements )
						fail(DiagnosticCategory::malformed_input, "Parser uniform-array element limit exceeded");
					validateUniformArrayPayload(t.uaType, elementCount);
					t.value = elementCount;
					return true;
				}
			default:
				fail(DiagnosticCategory::unsupported_input, "Parser::readToken encountered an unsupported token id", tokenOffset);
			};


			return false;
		}

		bool Parser::readASCIIToken( Token &t, char c )
		{
			// Read an ASCII token (returns a _jValue or None)
			while( (c == ' ')||(c == '\r')||(c == '\n')||(c == '\t') )
			{
				c = read<char>();
			};

			// We support // style comments
			if( c == '/' )
			{
				c = read<char>();
				if( c == '/' )
				{
					while(true)
					{
						c = read<char>();
						if( (c == '\n')||(c == '\r') )
							return this->readToken( t );
					};
				}
			}

			if( c == '{' ) t.type = Token::JID_MAP_BEGIN;
			else
			if( c == '}' ) t.type = Token::JID_MAP_END;
			else
			if( c == '[' ) t.type = Token::JID_ARRAY_BEGIN;
			else
			if( c == ']' ) t.type = Token::JID_ARRAY_END;
			else
			if( c == ',' ) t.type = Token::JID_VALUE_SEPARATOR;
			else
			if( c == ':' ) t.type = Token::JID_KEY_SEPARATOR;
			else
			if( c == '"' )
			{
				std::string s = readASCIIString();
				t.type = Token::JID_STRING;
				t.value = s;
			}
			else
			{
				// read number -----------------

				//Catch-all parsing method which handles
				//	- null
				//	- true
				//	- false
				//	- numbers
				//Any unquoted string which doesn't match the null, true, false
				//tokens is considered to be a number.  We throw warnings on
				//non-number values, but set the value to 0.  This is not ideal since
				//a NAN or infinity is not preserved.

				std::string string = "";
				bool isReal = false;

				while(true)
				{
					string.append( &c, 1 );
					if( !tryReadASCIIChar(c) )
						break;
					if( (c == ' ')||(c == '\r')||(c == '\n')||(c == '\t')||(c == '/')||(c == '{')||(c == '}')||(c == '[')||(c == ']')||(c == ',')||(c == ':')||(c == '"') )
					{
						stream->unget();
						if( stream->good() && byteOffset > 0 )
							--byteOffset;
						break;
					}else
					if( (c == '.')||(c == 'e')||(c == 'E') )
						isReal = true;
				}

				if( string.empty() )
					return true;

				std::transform(string.begin(), string.end(), string.begin(), [](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});

				if( string == "null" ) t.type = Token::JID_NULL;
				else
				if( string == "false" ){ t.type = Token::JID_BOOL; t.value = false;}
				else
				if( string == "true" ){ t.type = Token::JID_BOOL; t.value = true;}
				else
				if( isReal )
				{
					t.type = Token::JID_REAL32;
					t.value = fromString<float>( string );
				}else
				{
					t.type = Token::JID_INT64;
					t.value = fromString<sint64>( string );
				}
			}

			return true;
		}

		void Parser::pushState( State s )
		{
			if( stateStack.size() >= limits.maxNestingDepth )
				fail(DiagnosticCategory::malformed_input, "Parser nesting depth limit exceeded");
			stateStack.push(s);
			state = s;
		}

		void Parser::popState()
		{
			if( stateStack.empty() )
				fail(DiagnosticCategory::malformed_input, "Parser state stack underflow", tokenOffset);

			int n = (int)stateStack.size();
			stateStack.pop();

			if( n == 1 )
				state = STATE_COMPLETE;
			else
				state = stateStack.top();

						
			if( (state == STATE_MAP_NEED_VALUE)||(state == STATE_MAP_START))
				setState( STATE_MAP_GOT_VALUE );
			else if( (state == STATE_ARRAY_NEED_VALUE)||(state == STATE_ARRAY_START))
				setState( STATE_ARRAY_GOT_VALUE );

			// return true
		}

		void Parser::setState( State s )
		{
			State newState = s;

			if( this->binary )
			{
				// here we will do some built in direct state transitions
				if( newState == STATE_ARRAY_GOT_VALUE )
					newState = STATE_ARRAY_NEED_VALUE;
				else
				if( newState == STATE_MAP_SEPERATOR )
					newState = STATE_MAP_NEED_VALUE;
				else
				if( newState == STATE_MAP_GOT_VALUE )
					newState = STATE_MAP_NEED_KEY;
			}
			if( stateStack.empty() )
				fail(DiagnosticCategory::malformed_input, "Parser state update requires an active container", tokenOffset);
			state = newState;
			stateStack.top() = newState;
		}


		// Read an id followed by an encoded string.  There is no handle
		// callback, but rather, the string is stored in the shared string
		// Token map.
		bool Parser::readBinaryStringDefinition()
		{
			sint64 l = readLength();
			std::string s = readBinaryString();
			strings[l]  = s;
			return true;
		}

		bool Parser::undefineString()
		{
			fail(DiagnosticCategory::unsupported_input, "Parser does not support string-token undefinition", tokenOffset);
		}

		sint64 Parser::readLength()
		{
			// In the binary format, length is encoded in a multi-byte format.
			//For lenthgs < 0xf1 (240) the lenths are stored as a single byte.
			//If the length is longer than 240 bytes, the value of the first byte
			//determines the number of bytes that follow used to store the
			//length.  Currently, the only supported values for the binary byte
			//0xf2 = 2 bytes (16 bit unsigned length)
			//0xf4 = 4 bytes (32 bit unsigned length)
			//0xf8 = 8 bytes (64 bit signed length)
			//Other values (i.e. 0xf1 or 0xfa) are considered errors.
			const ubyte n = read<ubyte>();
			sint64 length = 0;
			switch( n )
			{
			case 0xf2:length = read<uword>();break;
			case 0xf4:length = read<uint32>();break;
			case 0xf8:length = read<sint64>();break;
			default:
				if( n < 0xf1 )
					length = n;
				else
				{
					std::ostringstream stringStream;
					stringStream << "unknown length id " << static_cast<int>(n);
					fail(DiagnosticCategory::malformed_input, stringStream.str(), byteOffset - 1);
				}
			}
			if( length < 0 )
				fail(DiagnosticCategory::malformed_input, "Parser::readLength decoded a negative length");
			return length;
		}

		std::string Parser::readBinaryString()
		{
			// A binary string stores an encoded byte length followed by raw bytes.
			const sint64 length = readLength();
			if( length > limits.maxStringBytes )
				fail(DiagnosticCategory::malformed_input, "Parser binary-string byte limit exceeded");

			std::string value(static_cast<size_t>(length), '\0');
			if( length > 0 )
				read(value.data(), length);
			return value;
		}

		bool Parser::tryReadASCIIChar( char &value )
		{
			if( byteOffset < 0 || byteOffset >= limits.maxInputBytes )
				fail(DiagnosticCategory::malformed_input, "Parser input byte limit exceeded", byteOffset);
			if( knownInputBytes >= 0 && byteOffset >= knownInputBytes )
				return false;
			if( stream->get(value) )
			{
				++byteOffset;
				return true;
			}
			if( stream->eof() )
				return false;
			fail(DiagnosticCategory::io, "Parser failed while reading ASCII input", byteOffset);
		}

		// Read a quoted string one character at a time.
		std::string Parser::readASCIIString()
		{
			std::string result;
			while( true )
			{
				char c = read<char>();
				if( c == '\\' )
				{
					c = read<char>();
					switch( c )
					{
					case '"':
					case '\\':
					case '/': result.push_back(c); break;
					case 'b': result.push_back('\b'); break;
					case 'f': result.push_back('\f'); break;
					case 'n': result.push_back('\n'); break;
					case 'r': result.push_back('\r'); break;
					case 't': result.push_back('\t'); break;
					case 'u':
						fail(DiagnosticCategory::unsupported_input, "Parser does not support Unicode escape sequences");
					default:
						result.push_back('\\');
						result.push_back(c);
						break;
					}
				}
				else if( c == '"' )
				{
					return result;
				}
				else
				{
					result.push_back(c);
				}
			}
		}







		// Writer ==================================================


		BinaryWriter::BinaryWriter( std::ostream *out )
		{
			if( !out )
				throw std::invalid_argument( "BinaryWriter requires a valid output stream" );
			stream = out;
			jsonMagic();
		}

		bool BinaryWriter::writeId( Token::Type id )
		{
			return write<ubyte>( (ubyte)id );
		}

		bool BinaryWriter::writeLength( const sint64 &length )
		{
			if( length < 0 )
				throw std::length_error( "BinaryWriter::writeLength cannot encode a negative length" );
			if( length < 0xf1 )
				return write<ubyte>(static_cast<ubyte>(length));
			if( length <= std::numeric_limits<uword>::max() )
			{
				return write<ubyte>(0xf2) && write<uword>(static_cast<uword>(length));
			}
			if( length <= std::numeric_limits<uint32>::max() )
			{
				return write<ubyte>(0xf4) && write<uint32>(static_cast<uint32>(length));
			}
			return write<ubyte>(0xf8) && write<sint64>(length);
		}

		bool BinaryWriter::jsonMagic()
		{
			return writeId( Token::JID_MAGIC ) && write<uint32>( 0x624a534e );// BINARY_MAGIC = 0x624a534e
		}

		void BinaryWriter::jsonBeginArray()
		{
			writeId( Token::JID_ARRAY_BEGIN);
		}

		void BinaryWriter::jsonEndArray()
		{
			writeId( Token::JID_ARRAY_END);
		}

		void BinaryWriter::jsonBeginMap()
		{
			writeId( Token::JID_MAP_BEGIN);
		}

		void BinaryWriter::jsonEndMap()
		{
			writeId( Token::JID_MAP_END);
		}

		void BinaryWriter::jsonString( const std::string &value )
		{
			if( value.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()) )
				throw std::length_error( "BinaryWriter::jsonString exceeds sint64 range" );
			const sint64 length = static_cast<sint64>(value.size());
			writeId(Token::JID_STRING);
			writeLength(length);
			if( !value.empty() )
				write<char>(value.data(), length);
		}

		void BinaryWriter::jsonKey( const std::string &key )
		{
			jsonString(key);
		}

		void BinaryWriter::jsonInt( const sint64 &value )
		{
			if( (value >= -0x80) && (value < 0x80) )
			{
				writeId( Token::JID_INT8 );
				write<sbyte>( (sbyte)value );
			}else if( (value >= -0x8000) && (value < 0x8000) )
			{
				writeId( Token::JID_INT16 );
				write<sword>( (sword)value );
			}else if( (value >= -(sint64)0x80000000) && (value < (sint64)0x80000000) )
			{
				writeId( Token::JID_INT32 );
				write<sint32>( (sint32)value );
			}else
			{
				writeId( Token::JID_INT64 );
				write<sint64>( value );
			}
		}

		void BinaryWriter::jsonUInt8( const ubyte &value )
		{
			writeId( Token::JID_UINT8 );
			write<sbyte>( (ubyte)value );
		}

		void BinaryWriter::jsonInt8( const sbyte &value )
		{
			writeId( Token::JID_INT8 );
			write<sbyte>( (sbyte)value );
		}

		void BinaryWriter::jsonInt32( const sint32 &value )
		{
			writeId( Token::JID_INT32 );
			write<sint32>( (sint32)value );
		}

		void BinaryWriter::jsonInt64( const sint64 &value )
		{
			writeId( Token::JID_INT64 );
			write<sint64>( value );
		}

		void BinaryWriter::jsonReal32( const real32 &value )
		{
			writeId( Token::JID_REAL32 );
			write<real32>(value );
		}

		void BinaryWriter::jsonReal64( const real64 &value )
		{
			writeId( Token::JID_REAL64 );
			write<real64>(value );
		}

		bool BinaryWriter::jsonUniformArrayReal16( const uword *data, sint64 numElements )
		{
			if( numElements < 0 )
				throw std::length_error( "BinaryWriter::jsonUniformArrayReal16 received a negative element count" );
			if( numElements > 0 && !data )
				throw std::invalid_argument( "BinaryWriter::jsonUniformArrayReal16 received null data" );
			return writeId(Token::JID_UNIFORM_ARRAY)
				&& write<sbyte>(static_cast<sbyte>(Token::JID_REAL16))
				&& writeLength(numElements)
				&& write<uword>(data, numElements);
		}

		void BinaryWriter::jsonBool( const bool &value )
		{
			if( value )
				writeId( Token::JID_TRUE );
			else
				writeId( Token::JID_FALSE );
		}



		// ASCIIWriter =============================================

		ASCIIWriter::ASCIIWriter( std::ostream *out )
		{
			stream = out;
			gotKey = false;
			firstItem = false;
			indentLevel = 0;
		}

		void ASCIIWriter::write( const std::string &text )
		{
			stream->write( text.c_str(), text.size() );
		}

		void ASCIIWriter::jsonBeginArray()
		{
			writePrefix();
			stack.push( Token::JID_ARRAY_BEGIN );
			writeNewline();
			write( "[" );
			++indentLevel;
			writeNewline();
			firstItem = true;
		}

		void ASCIIWriter::jsonEndArray()
		{
			firstItem = false;
			stack.pop();
			--indentLevel;
			writeNewline();
			write( "]" );
		}

		void ASCIIWriter::jsonBeginMap()
		{
			writePrefix();
			stack.push( Token::JID_MAP_BEGIN );
			writeNewline();
			write( "{" );
			++indentLevel;
			writeNewline();
			firstItem = true;
		}

		void ASCIIWriter::jsonEndMap()
		{
			firstItem = false;
			stack.pop();
			--indentLevel;
			writeNewline();
			write( "}" );
		}

		void ASCIIWriter::jsonString( const std::string &value )
		{
			writePrefix();
			write( "\"" + value + "\"" );
		}

		void ASCIIWriter::jsonKey( const std::string &key )
		{
			writePrefix();
			write( "\"" + key + "\"" );
			gotKey = true;
		}

		void ASCIIWriter::jsonInt( const sint64 &value )
		{
			writePrefix();
			write( toString<sint64>(value) );
		}

		void ASCIIWriter::jsonUInt8( const ubyte &value )
		{
			writePrefix();
			write( toString<ubyte>(value) );
		}

		void ASCIIWriter::jsonInt8( const sbyte &value )
		{
			writePrefix();
			write( toString<sbyte>(value) );
		}

		void ASCIIWriter::jsonInt32( const sint32 &value )
		{
			writePrefix();
			write( toString<sint32>(value) );
		}

		void ASCIIWriter::jsonInt64( const sint64 &value )
		{
			writePrefix();
			write( toString<sint64>(value) );
		}

		void ASCIIWriter::jsonReal32( const real32 &value )
		{
			writePrefix();
			std::string str = toString<real32>(value);
			write( str );
			// if string is not in scientific notation and doesnt contain a point
			// we will append one to make sure the value is loaded as float back in
			if( (str.find( 'e' ) == std::string::npos)&&(str.find( '.' ) == std::string::npos) )
				write( ".0" );
		}

		void ASCIIWriter::jsonReal64( const real64 &value )
		{
			writePrefix();
			write( toString<real64>(value) );
		}

		void ASCIIWriter::jsonBool( const bool &value )
		{
			writePrefix();
			if( value )
				write( "true" );
			else
				write( "false" );
		}

		void ASCIIWriter::writePrefix()
		{
			if( firstItem )
			{
				firstItem = false;
			}else
			if( gotKey )
			{
				gotKey = false;
				write( ":" );
			}else
			if( !stack.empty() )
			{
				if( (stack.top() == Token::JID_MAP_BEGIN)||
					(stack.top() == Token::JID_ARRAY_BEGIN))
				{
					write( ", " );
					writeNewline();
				}
				else
				{
					write( ", " );
				}
			}
		}

		void ASCIIWriter::writeNewline()
		{
			write( "\n" );
			for( int i=0;i<indentLevel;++i )
				write( "\t" );
		}



		// JSONCPP ==================================================

		
		// Value ----
		Value::Value() : m_type(Value::TYPE_NULL)
		{
		}

		void Value::cpyTo( char *dst )const
		{
			if( !dst )
				throw std::invalid_argument( "Value::cpyTo received a null destination" );
			switch( m_value.which() )
			{
			case 0: std::memcpy(dst, &ttl::var::get<bool>(m_value), sizeof(bool));break;
			case 1: std::memcpy(dst, &ttl::var::get<sint32>(m_value), sizeof(sint32));break;
			case 2: std::memcpy(dst, &ttl::var::get<real32>(m_value), sizeof(real32));break;
			case 3: std::memcpy(dst, &ttl::var::get<real64>(m_value), sizeof(real64));break;
			case 4: throw std::invalid_argument( "Value::cpyTo does not support strings" );
			case 5: std::memcpy(dst, &ttl::var::get<ubyte>(m_value), sizeof(ubyte));break;
			case 6: std::memcpy(dst, &ttl::var::get<sint64>(m_value), sizeof(sint64));break;
			default: throw std::runtime_error( "Value::cpyTo encountered an invalid variant type" );
			}
		}

		ArrayPtr Value::asArray()
		{
			return m_array;
		}

		ObjectPtr Value::asObject()
		{
			return m_object;
		}

		Value::Variant &Value::getVariant()
		{
			return m_value;
		}

		bool Value::isNull()const
		{
			return m_type==TYPE_NULL;
		}

		bool Value::isArray()const
		{
			return m_type==TYPE_ARRAY;
		}

		bool Value::isObject()const
		{
			return m_type==TYPE_OBJECT;
		}

		bool Value::isString()const
		{
			return m_value.which()==4;
		}

		Value Value::createArray()
		{
			Value v;
			v.m_type = TYPE_ARRAY;
			v.m_array = ArrayPtr( new Array() );
			return v;
		}
		Value Value::createArray(ArrayPtr array)
		{
			Value v;
			v.m_type = TYPE_ARRAY;
			v.m_array = array;
			return v;
		}

		Value Value::createObject()
		{
			Value v;
			v.m_type = TYPE_OBJECT;
			v.m_object = Object::create();
			return v;
		}

		Value Value::createObject(ObjectPtr obj)
		{
			Value v;
			v.m_type = TYPE_OBJECT;
			v.m_object = obj;
			return v;
		}

		// Array ----
		Array::Array() : m_isUniform(false), m_uniformdata(nullptr), m_numUniformElements(0), m_uniformType(-1)
		{
		}

		Array::~Array()
		{
			if( m_isUniform && m_uniformdata )
				free(m_uniformdata);
		}

		ArrayPtr Array::create()
		{
			return std::make_shared<Array>();
		}

		bool Array::isUniform()const
		{
			return m_isUniform;
		}

		void Array::append( const Value &value )
		{
			m_values.push_back( value );
		}

		void Array::append(ObjectPtr &object)
		{
			Value v;
			v.m_type = Value::TYPE_OBJECT;
			v.m_object = object;
			append(v);
		}

		void Array::append(ArrayPtr &array)
		{
			Value v;
			v.m_type = Value::TYPE_ARRAY;
			v.m_array = array;
			append(v);
		}

		sint64 Array::size()const
		{
			if( m_isUniform )
				return m_numUniformElements;
			if( m_values.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()) )
				throw std::length_error( "Array size exceeds sint64 range" );
			return static_cast<sint64>(m_values.size());
		}

		Value Array::getValue( const int index )
		{
			const sint64 elementCount = size();
			if( index < 0 || static_cast<sint64>(index) >= elementCount )
				throw std::out_of_range( "Array index is out of range" );
			if( !m_isUniform )
				return m_values[static_cast<size_t>(index)];
			if( !m_uniformdata )
				throw std::runtime_error( "Uniform array has no storage" );

			const size_t elementIndex = static_cast<size_t>(index);
			switch( m_uniformType )
			{
			case 0:
			{
				bool value = false;
				std::memcpy(&value, m_uniformdata + sizeof(bool) * elementIndex, sizeof(value));
				return Value::create<bool>(value);
			}
			case 1:
			{
				sint32 value = 0;
				std::memcpy(&value, m_uniformdata + sizeof(sint32) * elementIndex, sizeof(value));
				return Value::create<sint32>(value);
			}
			case 2:
			{
				real32 value = 0.0f;
				std::memcpy(&value, m_uniformdata + sizeof(real32) * elementIndex, sizeof(value));
				return Value::create<real32>(value);
			}
			case 3:
			{
				real64 value = 0.0;
				std::memcpy(&value, m_uniformdata + sizeof(real64) * elementIndex, sizeof(value));
				return Value::create<real64>(value);
			}
			case 4:
				throw std::runtime_error( "Uniform string arrays use expanded storage" );
			case 5:
			{
				ubyte value = 0;
				std::memcpy(&value, m_uniformdata + sizeof(ubyte) * elementIndex, sizeof(value));
				return Value::create<ubyte>(value);
			}
			case 6:
			{
				sint64 value = 0;
				std::memcpy(&value, m_uniformdata + sizeof(sint64) * elementIndex, sizeof(value));
				return Value::create<sint64>(value);
			}
			default:
				throw std::runtime_error( "Uniform array has an invalid storage type" );
			}
		}


		ObjectPtr                getObject( int index );
		ObjectPtr Array::getObject( int index )
		{
			Value v = getValue(index);
			if(v.isObject())
				return v.asObject();
			return ObjectPtr();
		}


		ArrayPtr Array::getArray( int index )
		{
			Value v = getValue(index);
			if(v.isArray())
				return v.asArray();
			return ArrayPtr();
		}

		// Object ----
		ObjectPtr Object::create()
		{
			return std::make_shared<Object>();
		}

		bool Object::hasKey( const std::string &key )
		{
			return m_values.find(key) != m_values.end();
		}

		Value Object::getValue( const std::string &key )
		{
			std::map<std::string, Value>::iterator it = m_values.find( key );
			if( it != m_values.end())
				return it->second;
			return Value();
		}

		void Object::getKeys( std::vector<std::string> &keys )
		{
			keys.clear();
			for( std::map<std::string, Value>::iterator it = m_values.begin(), end = m_values.end(); it != end; ++it )
				keys.push_back(it->first);
		}

		ObjectPtr Object::getObject( const std::string &key )
		{
			Value v = getValue(key);
			if(v.isObject())
				return v.asObject();
			return ObjectPtr();
		}

		ArrayPtr Object::getArray( const std::string &key )
		{
			Value v = getValue(key);
			if(v.isArray())
				return v.asArray();
			return ArrayPtr();
		}
		
		sint64 Object::size()const
		{
			if( m_values.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()) )
				throw std::length_error( "Object size exceeds sint64 range" );
			return static_cast<sint64>(m_values.size());
		}

		void Object::append( const std::string &key, const Value &value )
		{
			if( !m_values.emplace(key, value).second )
				throw std::runtime_error( "Object contains duplicate key " + key );
		}

		void Object::append( const std::string &key, ObjectPtr object )
		{
			Value v;
			v.m_type = Value::TYPE_OBJECT;
			v.m_object = object;
			append(key, v);
		}

		void Object::append(const std::string &key, ArrayPtr array)
		{
			Value v;
			v.m_type = Value::TYPE_ARRAY;
			v.m_array = array;
			append(key, v);
		}

		// JSONReader ===============================================

		JSONReader::JSONReader()
		{

		}

		Value JSONReader::getRoot()
		{
			return m_root;
		}

		void JSONReader::push()
		{
			if( !m_root.isNull() )
				m_stack.push( std::make_pair(m_root, nextKey) );
		}

		void JSONReader::pop()
		{
			if( !m_stack.empty() )
			{
				Value v = m_root;
				StackItem si = m_stack.top();
				m_root = si.first;
				m_stack.pop();
				if( m_root.isArray() )
					m_root.asArray()->append(v);
				else
				if( m_root.isObject() )
					m_root.asObject()->append(si.second, v);
			}
		}

		void JSONReader::jsonBeginArray()
		{
			push();
			m_root = Value::createArray();
		}

		void JSONReader::jsonEndArray()
		{
			pop();
		}

		void JSONReader::jsonBeginMap()
		{
			push();
			m_root = Value::createObject();
		}

		void JSONReader::jsonEndMap()
		{
			pop();
		}

		void JSONReader::jsonKey( const std::string &key )
		{
			nextKey = key;
		}

		void JSONReader::jsonString( const std::string &value )
		{
			jsonValue<std::string>(value);
		}

		void JSONReader::jsonBool( const bool &value )
		{
			jsonValue<bool>(value);
		}

		void JSONReader::jsonInt32( const sint32 &value )
		{
			jsonValue<sint32>(value);
		}

		void JSONReader::jsonInt64( const sint64 &value )
		{
			jsonValue<sint64>(value);
		}

		void JSONReader::jsonReal32( const real32 &value )
		{
			jsonValue<real32>(value);
		}

		void JSONReader::jsonReal64( const real64 &value )
		{
			jsonValue<real64>(value);
		}


		void JSONReader::uaBool( sint64 numElements, Parser *parser )
		{
			if( numElements < 0 )
				throw std::length_error( "JSONReader::uaBool received a negative element count" );

			jsonBeginArray();
			sint64 elementsRemaining = numElements;
			while( elementsRemaining > 0 )
			{
				const uint32 bits = parser->read<uint32>();
				const int bitCount = static_cast<int>(std::min<sint64>(elementsRemaining, 32));
				elementsRemaining -= bitCount;
				for( int bitIndex=0;bitIndex<bitCount;++bitIndex )
					jsonBool( (bits & (uint32(1) << bitIndex)) != 0 );
			}
			jsonEndArray();
		}

		void JSONReader::uaReal16( sint64 numElements, Parser *parser )
		{
			if( numElements < 0 )
				throw std::length_error( "JSONReader::uaReal16 received a negative element count" );
			jsonBeginArray();
			for( sint64 elementIndex=0;elementIndex<numElements;++elementIndex )
				jsonReal32(halfBitsToFloat(parser->read<uword>()));
			jsonEndArray();
		}

		void JSONReader::uaReal32( sint64 numElements, Parser *parser )
		{
			jsonUA<real32, real32>(numElements, parser);
		}

		void JSONReader::uaReal64( sint64 numElements, Parser *parser )
		{
			jsonUA<real64, real64>(numElements, parser);
		}

		void JSONReader::uaInt8( sint64 numElements, Parser *parser )
		{
			jsonUA<sint32, sbyte>(numElements, parser);
		}

		void JSONReader::uaInt16( sint64 numElements, Parser *parser )
		{
			jsonUA<sint32, sword>(numElements, parser);
		}

		void JSONReader::uaInt32( sint64 numElements, Parser *parser )
		{
			jsonUA<sint32, sint32>(numElements, parser);
		}

		void JSONReader::uaInt64( sint64 numElements, Parser *parser )
		{
			jsonUA<sint64, sint64>(numElements, parser);
		}

		void JSONReader::uaUInt8( sint64 numElements, Parser *parser )
		{
			jsonUA<ubyte, ubyte>(numElements, parser);
		}

		
		void JSONReader::uaString( sint64 numElements, Parser *parser )
		{
			jsonBeginArray();
			for(sint64 i=0;i<numElements;++i)
				jsonString( parser->readBinaryString() );
			jsonEndArray();
		}


		// JSONWriter =========================================

		bool JSONWriter::write( ObjectPtr object )
		{
			m_writer->jsonBeginMap();
			for( std::map<std::string, Value>::iterator it = object->m_values.begin(), end = object->m_values.end(); it != end; ++it )
			{
				m_writer->jsonKey( it->first );
				write( it->second );
			}
			m_writer->jsonEndMap();
			return true;
		}

		bool JSONWriter::write( ArrayPtr array )
		{
			m_writer->jsonBeginArray();
			for( std::vector<Value>::iterator it = array->m_values.begin(), end = array->m_values.end(); it != end; ++it )
				write( *it );
			m_writer->jsonEndArray();
			return true;
		}

		bool JSONWriter::write( Value &value )
		{
			if( value.isArray() )
				return write( value.asArray() );
			else
			if( value.isObject() )
				return write( value.asObject() );
			else
			if( !value.isNull() )
			{
				ttl::var::apply_visitor( *this, value.getVariant() );
			}
			return false;
		}

	}
}
