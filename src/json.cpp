

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

		void Token::event(Parser* p, bool key)
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
					p->handler->jsonKey(std::get<std::string>(value));
				else
					p->handler->jsonString(std::get<std::string>(value));
			}
			else if (type == JID_BOOL)
				p->handler->jsonBool(std::get<bool>(value));
			else if (type == JID_INT8)
				p->handler->jsonInt32(std::get<sbyte>(value));
			else if (type == JID_INT16)
				p->handler->jsonInt32(std::get<sword>(value));
			else if (type == JID_INT32)
				p->handler->jsonInt32(std::get<sint32>(value));
			else if (type == JID_INT64)
				p->handler->jsonInt64(std::get<sint64>(value));
			else if (type == JID_REAL16)
				p->handler->jsonReal32(std::get<real32>(value));
			else if (type == JID_REAL32)
				p->handler->jsonReal32(std::get<real32>(value));
			else if (type == JID_REAL64)
				p->handler->jsonReal64(std::get<real64>(value));
			else if (type == JID_UINT8)
				p->handler->jsonInt32(std::get<ubyte>(value));
			else if (type == JID_UINT16)
				p->handler->jsonInt32(std::get<uword>(value));
			else if (type == JID_UNIFORM_ARRAY)
			{
				sint64 numElements = std::get<sint64>(value);
				switch( uaType )
				{
				case Token::JID_BOOL:p->handler->uaBool(numElements, *p);break;
				case Token::JID_INT8:p->handler->uaInt8(numElements, *p);break;
				case Token::JID_INT16:p->handler->uaInt16(numElements, *p);break;
				case Token::JID_INT32:p->handler->uaInt32(numElements, *p);break;
				case Token::JID_INT64:p->handler->uaInt64(numElements, *p);break;
				case Token::JID_REAL16:p->handler->uaReal16(numElements, *p);break;
				case Token::JID_REAL32:p->handler->uaReal32(numElements, *p);break;
				case Token::JID_REAL64:p->handler->uaReal64(numElements, *p);break;
				case Token::JID_UINT8:p->handler->uaUInt8(numElements, *p);break;
				case Token::JID_STRING:p->handler->uaString(numElements, *p);break;
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

		bool Parser::parse(std::istream& input, Handler& event_handler)
		{
			return parse(&input, &event_handler, nullptr);
		}

		bool Parser::parse(
			std::istream& input,
			Handler& event_handler,
			DiagnosticList& output_diagnostics)
		{
			return parse(&input, &event_handler, &output_diagnostics);
		}

		bool Parser::parse(std::istream* in, Handler* h)
		{
			return parse(in, h, nullptr);
		}

		bool Parser::parse(std::istream* in, Handler* h, DiagnosticList* outputDiagnostics)
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

			if (stateStack.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
				fail(DiagnosticCategory::malformed_input, "Parser state depth exceeds int range", tokenOffset);
			const int previous_depth = static_cast<int>(stateStack.size());
			stateStack.pop();

			if (previous_depth == 1)
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
			auto appendCharacter = [&]( char value )
			{
				if( static_cast<sint64>(result.size()) >= limits.maxStringBytes )
					fail(DiagnosticCategory::malformed_input, "Parser ASCII-string byte limit exceeded", byteOffset);
				result.push_back(value);
			};
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
					case '/': appendCharacter(c); break;
					case 'b': appendCharacter('\b'); break;
					case 'f': appendCharacter('\f'); break;
					case 'n': appendCharacter('\n'); break;
					case 'r': appendCharacter('\r'); break;
					case 't': appendCharacter('\t'); break;
					case 'u':
						fail(DiagnosticCategory::unsupported_input, "Parser does not support Unicode escape sequences");
					default:
						appendCharacter('\\');
						appendCharacter(c);
						break;
					}
				}
				else if( c == '"' )
				{
					return result;
				}
				else
				{
					appendCharacter(c);
				}
			}
		}







		// Writer ==================================================


		BinaryWriter::BinaryWriter(std::ostream& output)
			: stream(&output)
		{
			jsonMagic();
		}

		BinaryWriter::BinaryWriter(std::ostream* output)
		{
			if (!output)
				throw std::invalid_argument("BinaryWriter requires a valid output stream");
			stream = output;
			jsonMagic();
		}

		bool BinaryWriter::writeId(Token::Type id)
		{
			return write<ubyte>(static_cast<ubyte>(id));
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

		void BinaryWriter::jsonInt(const sint64& value)
		{
			if (value >= std::numeric_limits<sbyte>::min()
				&& value <= std::numeric_limits<sbyte>::max())
			{
				writeId(Token::JID_INT8);
				write<sbyte>(static_cast<sbyte>(value));
			}
			else if (value >= std::numeric_limits<sword>::min()
				&& value <= std::numeric_limits<sword>::max())
			{
				writeId(Token::JID_INT16);
				write<sword>(static_cast<sword>(value));
			}
			else if (value >= std::numeric_limits<sint32>::min()
				&& value <= std::numeric_limits<sint32>::max())
			{
				writeId(Token::JID_INT32);
				write<sint32>(static_cast<sint32>(value));
			}
			else
			{
				writeId(Token::JID_INT64);
				write<sint64>(value);
			}
		}

		void BinaryWriter::jsonUInt8(const ubyte& value)
		{
			writeId(Token::JID_UINT8);
			write<ubyte>(value);
		}

		void BinaryWriter::jsonInt8(const sbyte& value)
		{
			writeId(Token::JID_INT8);
			write<sbyte>(value);
		}

		void BinaryWriter::jsonInt32(const sint32& value)
		{
			writeId(Token::JID_INT32);
			write<sint32>(value);
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

		bool BinaryWriter::jsonUniformArrayReal16(std::span<const uword> data)
		{
			if (data.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()))
				throw std::length_error("BinaryWriter::jsonUniformArrayReal16 element count exceeds sint64 range");
			const sint64 element_count = static_cast<sint64>(data.size());
			return writeId(Token::JID_UNIFORM_ARRAY)
				&& write<sbyte>(static_cast<sbyte>(Token::JID_REAL16))
				&& writeLength(element_count)
				&& write(data);
		}

		bool BinaryWriter::jsonUniformArrayReal16(
			const uword* data,
			sint64 element_count)
		{
			if (element_count < 0)
				throw std::length_error("BinaryWriter::jsonUniformArrayReal16 received a negative element count");
			if (element_count > 0 && !data)
				throw std::invalid_argument("BinaryWriter::jsonUniformArrayReal16 received null data");
			return jsonUniformArrayReal16(
				std::span<const uword>(data, static_cast<size_t>(element_count)));
		}

		void BinaryWriter::jsonBool( const bool &value )
		{
			if( value )
				writeId( Token::JID_TRUE );
			else
				writeId( Token::JID_FALSE );
		}



		// ASCIIWriter =============================================

		ASCIIWriter::ASCIIWriter(std::ostream& output)
			: stream(&output)
		{
		}

		ASCIIWriter::ASCIIWriter(std::ostream* output)
		{
			if (!output)
				throw std::invalid_argument("ASCIIWriter requires a valid output stream");
			stream = output;
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
		ArrayPtr Value::asArray()
		{
			return array_;
		}

		ConstArrayPtr Value::asArray() const
		{
			return array_;
		}

		ObjectPtr Value::asObject()
		{
			return object_;
		}

		ConstObjectPtr Value::asObject() const
		{
			return object_;
		}

		Value::Variant& Value::getVariant() noexcept
		{
			return scalar_;
		}

		const Value::Variant& Value::getVariant() const noexcept
		{
			return scalar_;
		}

		bool Value::isNull() const noexcept
		{
			return kind_ == Kind::null;
		}

		bool Value::isArray() const noexcept
		{
			return kind_ == Kind::array;
		}

		bool Value::isObject() const noexcept
		{
			return kind_ == Kind::object;
		}

		bool Value::isString() const noexcept
		{
			return kind_ == Kind::scalar && std::holds_alternative<std::string>(scalar_);
		}

		Value Value::createArray()
		{
			return createArray(Array::create());
		}

		Value Value::createArray(ArrayPtr array)
		{
			if (!array)
				throw std::invalid_argument("Value::createArray received a null array");
			Value result;
			result.kind_ = Kind::array;
			result.array_ = std::move(array);
			return result;
		}

		Value Value::createObject()
		{
			return createObject(Object::create());
		}

		Value Value::createObject(ObjectPtr object)
		{
			if (!object)
				throw std::invalid_argument("Value::createObject received a null object");
			Value result;
			result.kind_ = Kind::object;
			result.object_ = std::move(object);
			return result;
		}

		// Array ----
		ArrayPtr Array::create()
		{
			return std::make_shared<Array>();
		}

		bool Array::isUniform() const noexcept
		{
			return uses_uniform_storage_;
		}

		std::span<const Value> Array::elements() const noexcept
		{
			return values_;
		}

		std::span<const std::byte> Array::uniformData() const noexcept
		{
			return uniform_data_;
		}

		sint64 Array::uniformElementCount() const noexcept
		{
			return uniform_element_count_;
		}

		int Array::uniformTypeIndex() const noexcept
		{
			return uniform_type_index_;
		}

		void Array::setUniformStorage(
			int type_index,
			sint64 element_count,
			std::span<const std::byte> data)
		{
			if (type_index < 0)
				throw std::invalid_argument("Uniform array type index cannot be negative");
			if (element_count < 0)
				throw std::invalid_argument("Uniform array element count cannot be negative");
			if (!values_.empty())
				throw std::logic_error("Uniform storage cannot replace expanded array values");
			uses_uniform_storage_ = true;
			uniform_type_index_ = type_index;
			uniform_element_count_ = element_count;
			uniform_data_.assign(data.begin(), data.end());
		}

		void Array::append(const Value& value)
		{
			if (uses_uniform_storage_)
				throw std::logic_error("Expanded values cannot be appended to a uniform array");
			values_.push_back(value);
		}

		void Array::append(ObjectPtr object)
		{
			append(Value::createObject(std::move(object)));
		}

		void Array::append(ArrayPtr array)
		{
			append(Value::createArray(std::move(array)));
		}

		sint64 Array::size() const
		{
			if (uses_uniform_storage_)
				return uniform_element_count_;
			if (values_.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()))
				throw std::length_error("Array size exceeds sint64 range");
			return static_cast<sint64>(values_.size());
		}

		Value Array::getValue(int index) const
		{
			const sint64 element_count = size();
			if (index < 0 || static_cast<sint64>(index) >= element_count)
				throw std::out_of_range("Array index is out of range");
			if (!uses_uniform_storage_)
				return values_[static_cast<size_t>(index)];
			if (uniform_element_count_ > 0 && uniform_data_.empty())
				throw std::runtime_error("Uniform array has no storage");

			const size_t element_index = static_cast<size_t>(index);
			switch (uniform_type_index_)
			{
			case 0:
			{
				bool value = false;
				std::memcpy(&value, uniform_data_.data() + sizeof(bool) * element_index, sizeof(value));
				return Value::create<bool>(value);
			}
			case 1:
			{
				sint32 value = 0;
				std::memcpy(&value, uniform_data_.data() + sizeof(sint32) * element_index, sizeof(value));
				return Value::create<sint32>(value);
			}
			case 2:
			{
				real32 value = 0.0f;
				std::memcpy(&value, uniform_data_.data() + sizeof(real32) * element_index, sizeof(value));
				return Value::create<real32>(value);
			}
			case 3:
			{
				real64 value = 0.0;
				std::memcpy(&value, uniform_data_.data() + sizeof(real64) * element_index, sizeof(value));
				return Value::create<real64>(value);
			}
			case 4:
				throw std::runtime_error( "Uniform string arrays use expanded storage" );
			case 5:
			{
				ubyte value = 0;
				std::memcpy(&value, uniform_data_.data() + sizeof(ubyte) * element_index, sizeof(value));
				return Value::create<ubyte>(value);
			}
			case 6:
			{
				sint64 value = 0;
				std::memcpy(&value, uniform_data_.data() + sizeof(sint64) * element_index, sizeof(value));
				return Value::create<sint64>(value);
			}
			default:
				throw std::runtime_error( "Uniform array has an invalid storage type" );
			}
		}


		ObjectPtr Array::getObject(int index) const
		{
			const Value value = getValue(index);
			return value.isObject()
				? std::const_pointer_cast<Object>(value.asObject())
				: nullptr;
		}

		ArrayPtr Array::getArray(int index) const
		{
			const Value value = getValue(index);
			return value.isArray()
				? std::const_pointer_cast<Array>(value.asArray())
				: nullptr;
		}

		// Object ----
		ObjectPtr Object::create()
		{
			return std::make_shared<Object>();
		}

		bool Object::hasKey(const std::string& key) const
		{
			return entries_.contains(key);
		}

		Value Object::getValue(const std::string& key) const
		{
			const auto entry = entries_.find(key);
			return entry == entries_.end() ? Value() : entry->second;
		}

		void Object::getKeys(std::vector<std::string>& keys) const
		{
			keys.clear();
			keys.reserve(entries_.size());
			for (const auto& [key, value] : entries_)
			{
				static_cast<void>(value);
				keys.push_back(key);
			}
		}

		ObjectPtr Object::getObject(const std::string& key) const
		{
			const Value value = getValue(key);
			return value.isObject()
				? std::const_pointer_cast<Object>(value.asObject())
				: nullptr;
		}

		ArrayPtr Object::getArray(const std::string& key) const
		{
			const Value value = getValue(key);
			return value.isArray()
				? std::const_pointer_cast<Array>(value.asArray())
				: nullptr;
		}

		sint64 Object::size() const
		{
			if (entries_.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()))
				throw std::length_error("Object size exceeds sint64 range");
			return static_cast<sint64>(entries_.size());
		}

		const Object::EntryMap& Object::entries() const noexcept
		{
			return entries_;
		}

		void Object::append(const std::string& key, const Value& value)
		{
			if (!entries_.emplace(key, value).second)
				throw std::runtime_error("Object contains duplicate key " + key);
		}

		void Object::append(const std::string& key, ObjectPtr object)
		{
			append(key, Value::createObject(std::move(object)));
		}

		void Object::append(const std::string& key, ArrayPtr array)
		{
			append(key, Value::createArray(std::move(array)));
		}

		// JSONReader ===============================================

		Value JSONReader::getRoot() const
		{
			return root_;
		}

		void JSONReader::pushContainer()
		{
			if (!root_.isNull())
				stack_.emplace(root_, next_key_);
		}

		void JSONReader::popContainer()
		{
			if (stack_.empty())
				return;
			Value completed_container = root_;
			StackItem parent = stack_.top();
			stack_.pop();
			root_ = std::move(parent.first);
			if (root_.isArray())
				root_.asArray()->append(completed_container);
			else if (root_.isObject())
				root_.asObject()->append(parent.second, completed_container);
			else
				throw std::runtime_error("JSONReader parent is not a container");
		}

		void JSONReader::jsonBeginArray()
		{
			pushContainer();
			root_ = Value::createArray();
		}

		void JSONReader::jsonEndArray()
		{
			if (!root_.isArray())
				throw std::runtime_error("JSONReader received an array end without an array");
			popContainer();
		}

		void JSONReader::jsonBeginMap()
		{
			pushContainer();
			root_ = Value::createObject();
		}

		void JSONReader::jsonEndMap()
		{
			if (!root_.isObject())
				throw std::runtime_error("JSONReader received a map end without a map");
			popContainer();
		}

		void JSONReader::jsonKey(const std::string& key)
		{
			if (!root_.isObject())
				throw std::runtime_error("JSONReader received a key outside an object");
			next_key_ = key;
		}

		void JSONReader::jsonString(const std::string& value)
		{
			jsonValue<std::string>(value);
		}

		void JSONReader::jsonBool(const bool& value)
		{
			jsonValue<bool>(value);
		}

		void JSONReader::jsonInt32(const sint32& value)
		{
			jsonValue<sint32>(value);
		}

		void JSONReader::jsonInt64(const sint64& value)
		{
			jsonValue<sint64>(value);
		}

		void JSONReader::jsonReal32(const real32& value)
		{
			jsonValue<real32>(value);
		}

		void JSONReader::jsonReal64(const real64& value)
		{
			jsonValue<real64>(value);
		}

		void JSONReader::uaBool(sint64 element_count, Parser& parser)
		{
			if (element_count < 0)
				throw std::length_error("JSONReader::uaBool received a negative element count");
			jsonBeginArray();
			sint64 elements_remaining = element_count;
			while (elements_remaining > 0)
			{
				const uint32 bits = parser.read<uint32>();
				const int bit_count = static_cast<int>(std::min<sint64>(elements_remaining, 32));
				elements_remaining -= bit_count;
				for (int bit_index = 0; bit_index < bit_count; ++bit_index)
					jsonBool((bits & (uint32{1} << bit_index)) != 0);
			}
			jsonEndArray();
		}

		void JSONReader::uaReal16(sint64 element_count, Parser& parser)
		{
			if (element_count < 0)
				throw std::length_error("JSONReader::uaReal16 received a negative element count");
			jsonBeginArray();
			for (sint64 element_index = 0; element_index < element_count; ++element_index)
				jsonReal32(halfBitsToFloat(parser.read<uword>()));
			jsonEndArray();
		}

		void JSONReader::uaReal32(sint64 element_count, Parser& parser)
		{
			jsonUniformArray<real32, real32>(element_count, parser);
		}

		void JSONReader::uaReal64(sint64 element_count, Parser& parser)
		{
			jsonUniformArray<real64, real64>(element_count, parser);
		}

		void JSONReader::uaInt8(sint64 element_count, Parser& parser)
		{
			jsonUniformArray<sint32, sbyte>(element_count, parser);
		}

		void JSONReader::uaInt16(sint64 element_count, Parser& parser)
		{
			jsonUniformArray<sint32, sword>(element_count, parser);
		}

		void JSONReader::uaInt32(sint64 element_count, Parser& parser)
		{
			jsonUniformArray<sint32, sint32>(element_count, parser);
		}

		void JSONReader::uaInt64(sint64 element_count, Parser& parser)
		{
			jsonUniformArray<sint64, sint64>(element_count, parser);
		}

		void JSONReader::uaUInt8(sint64 element_count, Parser& parser)
		{
			jsonUniformArray<ubyte, ubyte>(element_count, parser);
		}

		void JSONReader::uaString(sint64 element_count, Parser& parser)
		{
			if (element_count < 0)
				throw std::length_error("JSONReader::uaString received a negative element count");
			jsonBeginArray();
			for (sint64 element_index = 0; element_index < element_count; ++element_index)
				jsonString(parser.readBinaryString());
			jsonEndArray();
		}


		// JSONWriter =========================================

		bool JSONWriter::write(ObjectPtr object)
		{
			if (!object)
				throw std::invalid_argument("JSONWriter::write received a null object");
			writer_->jsonBeginMap();
			for (const auto& [key, value] : object->entries())
			{
				writer_->jsonKey(key);
				Value writable_value = value;
				write(writable_value);
			}
			writer_->jsonEndMap();
			return true;
		}

		bool JSONWriter::write(ArrayPtr array)
		{
			if (!array)
				throw std::invalid_argument("JSONWriter::write received a null array");
			writer_->jsonBeginArray();
			for (const Value& value : array->elements())
			{
				Value writable_value = value;
				write(writable_value);
			}
			writer_->jsonEndArray();
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
				std::visit(*this, value.getVariant());
			}
			return false;
		}

	}
}
