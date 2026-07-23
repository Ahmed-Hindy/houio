#include <houio/HouGeoIO.h>
#include <houio/GeometryIO.h>

#include <cstring>
#include <limits>
#include <stdexcept>

namespace houio
{
	namespace
	{
		void appendDiagnostics( DiagnosticList *destination, DiagnosticList diagnostics )
		{
			if( !destination )
				return;
			for( Diagnostic &diagnostic : diagnostics )
				destination->push_back(std::move(diagnostic));
		}

		[[noreturn]] void throwReadFailure(
			const DiagnosticList &diagnostics, const std::string &fallbackMessage )
		{
			for( const Diagnostic &diagnostic : diagnostics )
			{
				if( diagnostic.severity == DiagnosticSeverity::error )
					throw DiagnosticException(diagnostic);
			}
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::io,
				fallbackMessage, -1, "file"});
		}

		size_t checkedConversionCount( sint64 count, const std::string &description )
		{
			if( count < 0 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::conversion,
					description + " cannot be negative", -1, "conversion"});
			if( count > static_cast<sint64>(std::numeric_limits<int>::max()) )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					description + " exceeds the simplified Geometry index range", -1, "conversion"});
			return static_cast<size_t>(count);
		}

		void validateDomainAttribute( const HouGeoAdapter::AttributeAdapter::Ptr &attribute, sint64 expectedCount,
			const std::string &path )
		{
			if( !attribute )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry encountered a null attribute", -1, path});
			if( attribute->getTupleSize() <= 0 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry attribute has an invalid tuple size", -1, path});
			if( attribute->getNumElements() != expectedCount )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry attribute element count does not match its domain", -1, path});
		}

		HouGeoAdapter::RawPointer::Ptr requireRawAttributeData(
			const HouGeoAdapter::AttributeAdapter::Ptr &attribute, const std::string &path )
		{
			HouGeoAdapter::RawPointer::Ptr rawPointer = attribute->getRawPointer();
			if( attribute->getNumElements() > 0 && (!rawPointer || !rawPointer->ptr) )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry attribute has no raw data", -1, path});
			return rawPointer;
		}

		template<typename T>
		T readRawScalar( const void *rawData, size_t scalarIndex )
		{
			T value{};
			const auto *bytes = static_cast<const unsigned char*>(rawData);
			std::memcpy(&value, bytes + scalarIndex * sizeof(T), sizeof(T));
			return value;
		}

		size_t attributeElementBytes( const Attribute::Ptr &attribute )
		{
			if( !attribute || attribute->numComponents() <= 0 || attribute->elementComponentSize() <= 0 )
				throw std::runtime_error( "Converted attribute has invalid component metadata" );
			const size_t componentCount = static_cast<size_t>(attribute->numComponents());
			const size_t componentBytes = static_cast<size_t>(attribute->elementComponentSize());
			if( componentCount > std::numeric_limits<size_t>::max() / componentBytes )
				throw std::length_error( "Converted attribute element size overflow" );
			return componentCount * componentBytes;
		}

		struct JsonScalarWriter
		{
			json::BinaryWriter &writer;

			void operator()( bool value )const { writer.jsonBool(value); }
			void operator()( sint32 value )const { writer.jsonInt32(value); }
			void operator()( real32 value )const { writer.jsonReal32(value); }
			void operator()( real64 value )const { writer.jsonReal64(value); }
			void operator()( const std::string &value )const { writer.jsonString(value); }
			void operator()( ubyte value )const { writer.jsonUInt8(value); }
			void operator()( sint64 value )const { writer.jsonInt64(value); }
		};

		template<typename T>
		std::vector<T> copyUniformValues( const json::ArrayPtr &array )
		{
			if( array->uniform_element_count < 0 )
				throw std::runtime_error( "Cannot export a uniform array with a negative element count" );
			const size_t elementCount = static_cast<size_t>(array->uniform_element_count);
			if( elementCount > std::numeric_limits<size_t>::max() / sizeof(T) )
				throw std::length_error( "Uniform JSON array byte count overflow" );
			if( elementCount * sizeof(T) != array->uniform_data.size() )
				throw std::runtime_error( "Cannot export a uniform array with inconsistent storage" );
			std::vector<T> values(elementCount);
			if( elementCount > 0 )
				std::memcpy(values.data(), array->uniform_data.data(), elementCount * sizeof(T));
			return values;
		}

		void writeJsonValue( json::BinaryWriter &writer, json::Value value );

		void writeJsonArray( json::BinaryWriter &writer, const json::ArrayPtr &array )
		{
			if( !array )
				throw std::runtime_error( "Cannot export a null JSON array" );
			if( array->isUniform() )
			{
				switch( array->uniform_type_index )
				{
				case 1:
					writer.jsonUniformArray(copyUniformValues<sint32>(array));
					return;
				case 2:
					writer.jsonUniformArray(copyUniformValues<real32>(array));
					return;
				case 3:
					writer.jsonUniformArray(copyUniformValues<real64>(array));
					return;
				case 5:
					writer.jsonUniformArray(copyUniformValues<ubyte>(array));
					return;
				case 6:
					writer.jsonUniformArray(copyUniformValues<sint64>(array));
					return;
				default:
					throw std::runtime_error( "Cannot export an unsupported uniform JSON array type" );
				}
			}

			writer.jsonBeginArray();
			for( const json::Value &item : array->values )
				writeJsonValue(writer, item);
			writer.jsonEndArray();
		}

		void writeJsonObject( json::BinaryWriter &writer, const json::ObjectPtr &object )
		{
			if( !object )
				throw std::runtime_error( "Cannot export a null JSON object" );
			writer.jsonBeginMap();
			for( const auto &entry : object->m_values )
			{
				writer.jsonKey(entry.first);
				writeJsonValue(writer, entry.second);
			}
			writer.jsonEndMap();
		}

		void writeJsonValue( json::BinaryWriter &writer, json::Value value )
		{
			if( value.isArray() )
				writeJsonArray(writer, value.asArray());
			else if( value.isObject() )
				writeJsonObject(writer, value.asObject());
			else if( value.isNull() )
				throw std::runtime_error( "Cannot export a null JSON value" );
			else
			{
				JsonScalarWriter scalarWriter{writer};
				std::visit(scalarWriter, value.getVariant());
			}
		}
	}

	HouGeo::Ptr HouGeoIO::import( std::istream *in )
	{
		return import(in, json::ParserLimits(), nullptr);
	}

	HouGeo::Ptr HouGeoIO::import( std::istream *in, DiagnosticList *diagnostics )
	{
		return import(in, json::ParserLimits(), diagnostics);
	}

	HouGeo::Ptr HouGeoIO::import( std::istream *in, const json::ParserLimits &limits )
	{
		return import(in, limits, nullptr);
	}

	HouGeo::Ptr HouGeoIO::import( std::istream *in, const json::ParserLimits &limits, DiagnosticList *diagnostics )
	{
		json::JSONReader reader;
		json::Parser parser(limits);
		if( !parser.parse(in, &reader, diagnostics) )
			return HouGeo::Ptr();

		try
		{
			json::ArrayPtr rootArray = reader.getRoot().asArray();
			if( !rootArray )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::import expected a flattened root array", -1, "root"});

			HouGeo::Ptr houGeo = HouGeo::create();
			houGeo->load(HouGeo::toObject(rootArray));
			return houGeo;
		}
		catch( const DiagnosticException &exception )
		{
			if( diagnostics )
			{
				appendDiagnostic(diagnostics, exception.diagnostic());
				return HouGeo::Ptr();
			}
			throw;
		}
		catch( const std::exception &exception )
		{
			Diagnostic diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				exception.what(), -1, "root"};
			if( diagnostics )
			{
				appendDiagnostic(diagnostics, std::move(diagnostic));
				return HouGeo::Ptr();
			}
			throw DiagnosticException(std::move(diagnostic));
		}
	}

	Geometry::Ptr HouGeoIO::importGeometry( const std::string &path )
	{
		GeometryReadResult<Geometry::Ptr> result = GeometryIO::readGeometry(path);
		if( !result )
			throwReadFailure(result.diagnostics, "HouGeoIO::importGeometry failed for " + path);
		return result.value;
	}

	Geometry::Ptr HouGeoIO::importGeometry( const std::string &path, DiagnosticList *diagnostics )
	{
		if( !diagnostics )
			return importGeometry(path);
		GeometryReadResult<Geometry::Ptr> result = GeometryIO::readGeometry(path);
		appendDiagnostics(diagnostics, std::move(result.diagnostics));
		return result.value;
	}

	ScalarField::Ptr HouGeoIO::importVolume( const std::string &path )
	{
		GeometryReadResult<ScalarField::Ptr> result = GeometryIO::readVolume(path);
		if( !result )
			throwReadFailure(result.diagnostics, "HouGeoIO::importVolume failed for " + path);
		return result.value;
	}

	ScalarField::Ptr HouGeoIO::importVolume( const std::string &path, DiagnosticList *diagnostics )
	{
		if( !diagnostics )
			return importVolume(path);
		GeometryReadResult<ScalarField::Ptr> result = GeometryIO::readVolume(path);
		appendDiagnostics(diagnostics, std::move(result.diagnostics));
		return result.value;
	}

	Geometry::Ptr HouGeoIO::convertToGeometry(HouGeo::Ptr houGeo, HouGeoAdapter::Primitive::Ptr houPrim )
	{
		return convertToGeometry(houGeo, houPrim, nullptr);
	}

	Geometry::Ptr HouGeoIO::convertToGeometry(HouGeo::Ptr houGeo, HouGeoAdapter::Primitive::Ptr houPrim,
		DiagnosticList *diagnostics )
	{
		try
		{
			if( !houGeo )
				throw std::invalid_argument( "HouGeoIO::convertToGeometry received null geometry" );

			Geometry::Ptr result;

		// Cast and validate the domain sizes before allocating convenience geometry.
		const sint64 numPoints = houGeo->pointcount();
		const sint64 numVertices = houGeo->vertexcount();
		const size_t pointCount = checkedConversionCount(numPoints, "Point count");
		const size_t vertexCount = checkedConversionCount(numVertices, "Vertex count");

		// we only support geometry with non mixed primitives (e.g. triangles only)
		// this code checks if that is the case...
		int numVerticesPerPoly = 0;
		if( houPrim )
		{
			HouGeo::HouPoly::Ptr poly = std::dynamic_pointer_cast<HouGeo::HouPoly>(houPrim);
			if( !poly )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeoIO::convertToGeometry supports only polygon primitives", -1, "conversion.primitive"});
			const int numPolys = poly->numPolys();
			if( numPolys < 0 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry polygon count cannot be negative", -1, "conversion.primitive"});

			size_t observedVertexCount = 0;
			numVerticesPerPoly = numPolys > 0 ? poly->numVertices(0) : 0;
			for( int polygonIndex=0;polygonIndex<numPolys;++polygonIndex )
			{
				const int polygonVertexCount = poly->numVertices(polygonIndex);
				if( polygonVertexCount != numVerticesPerPoly )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
						"HouGeoIO::convertToGeometry requires a constant polygon vertex count", -1,
						"conversion.primitive"});
				if( polygonVertexCount < 0 || static_cast<size_t>(polygonVertexCount) > vertexCount - observedVertexCount )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
						"HouGeoIO::convertToGeometry polygon vertices exceed the vertex domain", -1,
						"conversion.primitive"});
				poly->vertices(polygonIndex);
				observedVertexCount += static_cast<size_t>(polygonVertexCount);
			}
			if( observedVertexCount != vertexCount )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry polygon vertex total does not match vertexcount", -1,
					"conversion.primitive"});
		}

		// create the right kind of geometry depending on vertexcount per primitive (point, line or triangle geometry)
		if( !houPrim )
		{
			result = Geometry::createPointGeometry();
		}
		else
		if( numVerticesPerPoly == 2 )
		{
			result = Geometry::createLineGeometry();
		}
		else
		if( numVerticesPerPoly == 3 )
		{
			result = Geometry::createTriangleGeometry();
		}
		else
		if( numVerticesPerPoly == 4 )
		{
			result = Geometry::createQuadGeometry();
		}
		if( !result )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
				"HouGeoIO::convertToGeometry supports only lines, triangles, and quads", -1,
				"conversion.primitive"});

		// attributes ---
		std::vector<Attribute::Ptr> geoAttrs;

		std::vector<std::string> pointAttributesNames;
		houGeo->getPointAttributeNames(pointAttributesNames);

		std::vector<std::string> vertexAttributesNames;
		houGeo->getVertexAttributeNames(vertexAttributesNames);

		// convert point attributes ---
		for( std::vector<std::string>::iterator it = pointAttributesNames.begin(), end = pointAttributesNames.end();  it != end; ++it )
		{
			std::string attrName = *it;
			HouGeoAdapter::AttributeAdapter::Ptr houAttr = houGeo->getPointAttribute(attrName);
			const std::string attributePath = "attributes.pointattributes." + attrName;
			validateDomainAttribute(houAttr, numPoints, attributePath);
			if( houAttr->getType() != HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC )
			{
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry skips non-numeric point attribute " + attrName,
					-1, attributePath});
				continue;
			}
			const int numComponents = houAttr->getTupleSize();

			Attribute::Ptr attr;
			if( attrName == "P" )
			{
				if( numComponents < 3 )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: P requires at least three components" );
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);

				attr = result->getAttr("P");
				if( !attr )
					throw std::runtime_error( "HouGeoIO::convertToGeometry target geometry has no P attribute" );
				for( size_t pointIndex=0;pointIndex<pointCount;++pointIndex )
				{
					math::Vec3f position;
					const size_t tupleOffset = pointIndex * static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
					{
						position = math::Vec3f(halfBitsToFloat(readRawScalar<uword>(rawPointer->ptr, tupleOffset)),
							halfBitsToFloat(readRawScalar<uword>(rawPointer->ptr, tupleOffset + 1)),
							halfBitsToFloat(readRawScalar<uword>(rawPointer->ptr, tupleOffset + 2)));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						position = math::Vec3f(readRawScalar<real32>(rawPointer->ptr, tupleOffset),
							readRawScalar<real32>(rawPointer->ptr, tupleOffset + 1),
							readRawScalar<real32>(rawPointer->ptr, tupleOffset + 2));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						position = math::Vec3f(
							static_cast<real32>(readRawScalar<real64>(rawPointer->ptr, tupleOffset)),
							static_cast<real32>(readRawScalar<real64>(rawPointer->ptr, tupleOffset + 1)),
							static_cast<real32>(readRawScalar<real64>(rawPointer->ptr, tupleOffset + 2)));
					}
					else
						throw std::runtime_error( "HouGeoIO::convertToGeometry: unsupported P storage" );
					attr->appendElement(position);
				}
			}else
			if( (attrName == "UV")||(attrName == "uv") )
			{
				if( numComponents < 2 )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: UV requires at least two components" );
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);

				attrName = "UV";
				attr = Attribute::createV2f();
				for( size_t pointIndex=0;pointIndex<pointCount;++pointIndex )
				{
					math::Vec2f uv;
					const size_t tupleOffset = pointIndex * static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
					{
						uv = math::Vec2f(halfBitsToFloat(readRawScalar<uword>(rawPointer->ptr, tupleOffset)),
							halfBitsToFloat(readRawScalar<uword>(rawPointer->ptr, tupleOffset + 1)));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						uv = math::Vec2f(readRawScalar<real32>(rawPointer->ptr, tupleOffset),
							readRawScalar<real32>(rawPointer->ptr, tupleOffset + 1));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						uv = math::Vec2f(static_cast<real32>(readRawScalar<real64>(rawPointer->ptr, tupleOffset)),
							static_cast<real32>(readRawScalar<real64>(rawPointer->ptr, tupleOffset + 1)));
					}
					else
						throw std::runtime_error( "HouGeoIO::convertToGeometry: unsupported UV storage" );
					attr->appendElement(uv);
				}
			}else
			if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
			{
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::HALF,
					static_cast<const unsigned char*>(rawPointer ? rawPointer->ptr : nullptr), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
			{
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::FLOAT,
					static_cast<const unsigned char*>(rawPointer ? rawPointer->ptr : nullptr), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
			{
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::INT,
					static_cast<const unsigned char*>(rawPointer ? rawPointer->ptr : nullptr), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64 )
			{
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::INT64,
					static_cast<const unsigned char*>(rawPointer ? rawPointer->ptr : nullptr), houAttr->getNumElements());
			}
			else
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry cannot convert point attribute " + attrName,
					-1, "attributes.pointattributes." + attrName});

			if( attr )
				result->setAttr(attrName, attr);
		}

		Attribute::Ptr convertedPositions = result->getAttr("P");
		if( !convertedPositions || convertedPositions->numElements() != static_cast<int>(pointCount) )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeoIO::convertToGeometry requires one converted P value per point", -1,
				"attributes.pointattributes.P"});

		// convert vertex attributes ---
		std::vector<std::pair<Attribute::Ptr, Attribute::Ptr>> vertex2pointAttr;
		for( std::vector<std::string>::iterator it = vertexAttributesNames.begin(), end = vertexAttributesNames.end();  it != end; ++it )
		{
			std::string attrName = *it;
			HouGeoAdapter::AttributeAdapter::Ptr houAttr = houGeo->getVertexAttribute(attrName);
			const std::string attributePath = "attributes.vertexattributes." + attrName;
			validateDomainAttribute(houAttr, numVertices, attributePath);
			if( houAttr->getType() != HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC )
			{
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry skips non-numeric vertex attribute " + attrName,
					-1, attributePath});
				continue;
			}
			const int numComponents = houAttr->getTupleSize();

			Attribute::Ptr attr;
			if( (attrName == "UV")||(attrName == "uv") )
			{
				if( numComponents < 2 )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: vertex UV requires at least two components" );
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);

				attrName = "UV";
				attr = Attribute::createV2f();

				for( size_t vertexIndex=0;vertexIndex<vertexCount;++vertexIndex )
				{
					math::Vec2f uv;
					const size_t tupleOffset = vertexIndex * static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
					{
						uv = math::Vec2f(halfBitsToFloat(readRawScalar<uword>(rawPointer->ptr, tupleOffset)),
							halfBitsToFloat(readRawScalar<uword>(rawPointer->ptr, tupleOffset + 1)));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						uv = math::Vec2f(readRawScalar<real32>(rawPointer->ptr, tupleOffset),
							readRawScalar<real32>(rawPointer->ptr, tupleOffset + 1));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						uv = math::Vec2f(static_cast<real32>(readRawScalar<real64>(rawPointer->ptr, tupleOffset)),
							static_cast<real32>(readRawScalar<real64>(rawPointer->ptr, tupleOffset + 1)));
					}
					else
						throw std::runtime_error( "HouGeoIO::convertToGeometry: unsupported vertex UV storage" );
					attr->appendElement(uv);
				}
			}else
			if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
			{
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::HALF,
					static_cast<const unsigned char*>(rawPointer ? rawPointer->ptr : nullptr), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
			{
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::FLOAT,
					static_cast<const unsigned char*>(rawPointer ? rawPointer->ptr : nullptr), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64 )
			{
				HouGeoAdapter::RawPointer::Ptr rawPointer = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::INT64,
					static_cast<const unsigned char*>(rawPointer ? rawPointer->ptr : nullptr), houAttr->getNumElements());
			}

			if( !attr )
			{
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry cannot convert vertex attribute " + attrName,
					-1, "attributes.vertexattributes." + attrName});
				continue;
			}

			// create point attribute which we will derive from this vertex attribute
			Attribute::Ptr pointAttr = attr->copy();
			pointAttr->resize(pointCount);
			pointAttr->fillZero();

			result->setAttr( attrName, pointAttr );
			vertex2pointAttr.push_back( std::make_pair( attr, pointAttr ) );
		}


		// only done when we have primitives...
		if( houPrim )
		{
			HouGeo::HouPoly::Ptr poly = std::dynamic_pointer_cast<HouGeo::HouPoly>(houPrim);
			if( !poly )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeoIO::convertToGeometry expected a polygon primitive", -1, "conversion.primitive"});
			const int numPolys = poly->numPolys();

			// Geometry has no face-varying domain, so points are split when vertex values differ.
			std::vector<bool> pointsToSplit(pointCount, false);
			std::vector<bool> pointsInitialized(pointCount, false);
			std::vector<int> firstVertex(pointCount, -1);

			size_t globalVertexIndex = 0;
			for( int polygonIndex=0;polygonIndex<numPolys;++polygonIndex )
			{
				const int polygonVertexCount = poly->numVertices(polygonIndex);
				const int *polygonVertices = poly->vertices(polygonIndex);
				for( int localVertexIndex=0;localVertexIndex<polygonVertexCount;++localVertexIndex, ++globalVertexIndex )
				{
					if( globalVertexIndex >= vertexCount )
						throw std::runtime_error( "Polygon traversal exceeded the vertex domain" );
					const int point = polygonVertices[localVertexIndex];
					if( point < 0 || static_cast<size_t>(point) >= pointCount )
						throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
							"Polygon references a point outside pointcount", -1, "conversion.primitive"});
					const size_t pointIndex = static_cast<size_t>(point);
					if( pointsToSplit[pointIndex] )
						continue;

					if( firstVertex[pointIndex] >= 0 )
					{
						for( const auto &attributePair : vertex2pointAttr )
						{
							const size_t elementBytes = attributeElementBytes(attributePair.first);
							if( std::memcmp(attributePair.first->getRawPointer(static_cast<int>(globalVertexIndex)),
								attributePair.first->getRawPointer(firstVertex[pointIndex]), elementBytes) != 0 )
							{
								pointsToSplit[pointIndex] = true;
								break;
							}
						}
					}
					else
					{
						firstVertex[pointIndex] = static_cast<int>(globalVertexIndex);
					}
				}
			}
			if( globalVertexIndex != vertexCount )
				throw std::runtime_error( "Polygon traversal did not consume the complete vertex domain" );

			globalVertexIndex = 0;
			for( int polygonIndex=0;polygonIndex<numPolys;++polygonIndex )
			{
				const int polygonVertexCount = poly->numVertices(polygonIndex);
				const int *polygonVertices = poly->vertices(polygonIndex);
				std::vector<unsigned int> vertices;
				vertices.reserve(static_cast<size_t>(polygonVertexCount));

				for( int localVertexIndex=0;localVertexIndex<polygonVertexCount;++localVertexIndex, ++globalVertexIndex )
				{
					const int point = polygonVertices[localVertexIndex];
					if( point < 0 || static_cast<size_t>(point) >= pointCount || globalVertexIndex >= vertexCount )
						throw std::runtime_error( "Polygon traversal contains an invalid point or vertex index" );
					const size_t pointIndex = static_cast<size_t>(point);
					unsigned int finalPointIndex = static_cast<unsigned int>(pointIndex);

					if( pointsToSplit[pointIndex] && pointsInitialized[pointIndex] )
						finalPointIndex = result->duplicatePoint(finalPointIndex);
					else if( !pointsInitialized[pointIndex] )
						pointsInitialized[pointIndex] = true;

					for( const auto &attributePair : vertex2pointAttr )
					{
						const size_t elementBytes = attributeElementBytes(attributePair.first);
						std::memcpy(attributePair.second->getRawPointer(static_cast<int>(finalPointIndex)),
							attributePair.first->getRawPointer(static_cast<int>(globalVertexIndex)), elementBytes);
					}
					vertices.push_back(finalPointIndex);
				}

				if( polygonVertexCount == 2 )
					result->addLine(vertices[0], vertices[1]);
				else if( polygonVertexCount == 3 )
					result->addTriangle(vertices[0], vertices[1], vertices[2]);
				else if( polygonVertexCount == 4 )
					result->addQuad(vertices[0], vertices[1], vertices[2], vertices[3]);
			}

			// Houdini polygons are clockwise; the convenience geometry expects counter-clockwise order.
			result->reverse();
		}
			return result;
		}
		catch( const DiagnosticException &exception )
		{
			if( diagnostics )
			{
				appendDiagnostic(diagnostics, exception.diagnostic());
				return Geometry::Ptr();
			}
			throw;
		}
		catch( const std::exception &exception )
		{
			Diagnostic diagnostic{DiagnosticSeverity::error, DiagnosticCategory::conversion,
				exception.what(), -1, "conversion"};
			if( diagnostics )
			{
				appendDiagnostic(diagnostics, std::move(diagnostic));
				return Geometry::Ptr();
			}
			throw DiagnosticException(std::move(diagnostic));
		}
	}

	void HouGeoIO::makeLog( const std::string &path, std::ostream *output )
	{
		std::ifstream input( path.c_str(), std::ios_base::in | std::ios_base::binary );
		json::JSONLogger logger(*output);
		json::Parser parser;
		parser.parse( &input, &logger );
	}



	HouGeo::Ptr HouGeoIO::adaptVolume( ScalarField::Ptr volume )
	{
		if( !volume )
			return HouGeo::Ptr();
		HouGeo::Ptr houGeo = std::make_shared<HouGeo>();
		houGeo->addPrimitive(volume);
		return houGeo;
	}

	bool HouGeoIO::exportVolume( const std::string &filename, ScalarField::Ptr volume )
	{
		return static_cast<bool>(GeometryIO::writeVolume(filename, volume));
	}

	bool HouGeoIO::xport( const std::string &filename, ScalarField::Ptr volume )
	{
		return exportVolume(filename, volume);
	}

	HouGeo::Ptr HouGeoIO::adaptGeometry( Geometry::Ptr geometry )
	{
		if( !geometry )
			return HouGeo::Ptr();
		HouGeo::Ptr houdiniGeometry = std::make_shared<HouGeo>();

		std::vector<std::string> pointAttributeNames;
		geometry->getAttrNames(pointAttributeNames);
		for( const std::string &name : pointAttributeNames )
		{
			Attribute::Ptr sourceAttribute = geometry->getAttr(name);

			// Houdini stores P as a four-component point position in this representation.
			if( name == "P" && sourceAttribute->numComponents() == 3 )
			{
				Attribute::Ptr promotedAttribute = std::make_shared<Attribute>(4, Attribute::FLOAT);
				const int elementCount = sourceAttribute->numElements();
				for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
				{
					const math::V3f position = sourceAttribute->get<math::V3f>(elementIndex);
					promotedAttribute->appendElement<math::V4f>(
						math::V4f(position.x, position.y, position.z, 1.0f));
				}
				sourceAttribute = promotedAttribute;
			}

			HouGeo::HouAttribute::Ptr houdiniAttribute =
				std::make_shared<HouGeo::HouAttribute>(name, sourceAttribute);
			houdiniGeometry->setPointAttribute(houdiniAttribute);
		}

		if( geometry->numPrimitives() > 0 )
		{
			HouGeo::HouTopology::Ptr topology = std::make_shared<HouGeo::HouTopology>();
			topology->indexBuffer.reserve(geometry->m_indexBuffer.size());
			for( const unsigned int pointIndex : geometry->m_indexBuffer )
			{
				if( pointIndex > static_cast<unsigned int>(std::numeric_limits<int>::max()) )
					throw std::overflow_error( "HouGeoIO::adaptGeometry point index exceeds int range" );
				topology->indexBuffer.push_back(static_cast<int>(pointIndex));
			}
			houdiniGeometry->setTopology(topology);

			// The simplified Geometry model uses a fixed vertex count, so all polygons form one run.
			HouGeo::HouPoly::Ptr polygonRun = std::make_shared<HouGeo::HouPoly>();
			const int primitiveCount = geometry->numPrimitives();
			const int verticesPerPrimitive = geometry->numPrimitiveVertices();
			polygonRun->m_numPolys = primitiveCount;
			polygonRun->m_perPolyVertexListOffset.assign(static_cast<size_t>(primitiveCount), 0);
			for( int primitiveIndex=1;primitiveIndex<primitiveCount;++primitiveIndex )
			{
				polygonRun->m_perPolyVertexListOffset[static_cast<size_t>(primitiveIndex)] =
					polygonRun->m_perPolyVertexListOffset[static_cast<size_t>(primitiveIndex - 1)]
					+ verticesPerPrimitive;
			}
			polygonRun->m_perPolyVertexCount.assign(static_cast<size_t>(primitiveCount), verticesPerPrimitive);

			if( geometry->m_indexBuffer.size() > static_cast<size_t>(std::numeric_limits<int>::max()) )
				throw std::overflow_error( "HouGeoIO::adaptGeometry index buffer exceeds int range" );
			polygonRun->m_vertices.resize(geometry->m_indexBuffer.size());
			for( size_t vertexIndex=0;vertexIndex<geometry->m_indexBuffer.size();++vertexIndex )
				polygonRun->m_vertices[vertexIndex] = static_cast<int>(vertexIndex);

			polygonRun->m_closed = geometry->primitiveType() != Geometry::LINE;
			houdiniGeometry->addPrimitive(polygonRun);
		}

		return houdiniGeometry;
	}

	bool HouGeoIO::exportGeometry( const std::string &filename, Geometry::Ptr geometry )
	{
		return static_cast<bool>(GeometryIO::writeGeometry(filename, geometry));
	}

	bool HouGeoIO::xport( const std::string &filename, Geometry::Ptr geometry )
	{
		return exportGeometry(filename, geometry);
	}

	bool HouGeoIO::xport( const std::string& filename, const std::vector<math::V3f>& points )
	{
		std::map<std::string, std::vector<math::V3f>> pointAttributes;
		pointAttributes["P"] = points;
		return xport(filename, pointAttributes);
	}

	bool HouGeoIO::xport( const std::string& filename,
		const std::map<std::string, std::vector<math::V3f>>& pointAttributes )
	{
		Geometry::Ptr geometry = Geometry::createPointGeometry();

		for( const auto &entry : pointAttributes )
		{
			const std::string &name = entry.first;
			const std::vector<math::V3f> &values = entry.second;
			const int elementCount = static_cast<int>(values.size());
			if( elementCount == 0 )
				continue;

			Attribute::Ptr attribute = Attribute::createV3f(elementCount);
			memcpy(attribute->getRawPointer(), values.data(), sizeof(math::V3f) * static_cast<size_t>(elementCount));
			geometry->setAttr(name, attribute);
		}

		return xport(filename, geometry);
	}




	bool HouGeoIO::xport( std::ostream *output, HouGeoAdapter::Ptr geometry, bool binary )
	{
		return exportGeometry(output, geometry, binary);
	}

	bool HouGeoIO::exportGeometry( std::ostream *output, HouGeoAdapter::Ptr geometry, bool binary )
	{
		if( !output || !geometry || !output->good() || !binary )
			return false;

		json::BinaryWriter writer(output);
		ExportContext context(writer);

		const sint64 pointCount = geometry->pointcount();
		const sint64 vertexCount = geometry->vertexcount();
		const sint64 primitiveCount = geometry->primitivecount();
		if( pointCount < 0 || vertexCount < 0 || primitiveCount < 0 )
			throw std::runtime_error( "HouGeoIO::exportGeometry received negative geometry counts" );

		writer.jsonBeginArray();

		writer.jsonString( "pointcount" );
		writer.jsonInt( pointCount );

		writer.jsonString( "vertexcount" );
		writer.jsonInt( vertexCount );

		writer.jsonString( "primitivecount" );
		writer.jsonInt( primitiveCount );

		writer.jsonString( "topology" );
		writer.jsonBeginArray();
		if( geometry->getTopology() )
			exportTopology(context, geometry->getTopology());
		writer.jsonEndArray();

		writer.jsonString( "attributes" );
		writer.jsonBeginArray();

		writer.jsonString( "pointattributes" );
		writer.jsonBeginArray();
		std::vector<std::string> pointAttributeNames;
		geometry->getPointAttributeNames(pointAttributeNames);
		for( const std::string &name : pointAttributeNames )
			exportAttribute(context, geometry->getPointAttribute(name));
		writer.jsonEndArray();

		writer.jsonString( "vertexattributes" );
		writer.jsonBeginArray();
		std::vector<std::string> vertexAttributeNames;
		geometry->getVertexAttributeNames(vertexAttributeNames);
		for( const std::string &name : vertexAttributeNames )
			exportAttribute(context, geometry->getVertexAttribute(name));
		writer.jsonEndArray();

		writer.jsonString( "primitiveattributes" );
		writer.jsonBeginArray();
		std::vector<std::string> primitiveAttributeNames;
		geometry->getPrimitiveAttributeNames(primitiveAttributeNames);
		for( const std::string &name : primitiveAttributeNames )
			exportAttribute(context, geometry->getPrimitiveAttribute(name));
		writer.jsonEndArray();

		writer.jsonString( "globalattributes" );
		writer.jsonBeginArray();
		std::vector<std::string> globalAttributeNames;
		geometry->getGlobalAttributeNames(globalAttributeNames);
		for( const std::string &name : globalAttributeNames )
			exportAttribute(context, geometry->getGlobalAttribute(name));
		writer.jsonEndArray();

		writer.jsonEndArray();

		if( primitiveCount > 0 )
		{
			writer.jsonString( "primitives" );
			writer.jsonBeginArray();

			std::vector<HouGeoAdapter::Primitive::Ptr> primitives;
			geometry->getPrimitives(primitives);

			int topologyVertexOffset = 0;
			for( const HouGeoAdapter::Primitive::Ptr &primitive : primitives )
			{
				if( auto volume = std::dynamic_pointer_cast<HouGeoAdapter::VolumePrimitive>(primitive) )
				{
					exportPrimitive(context, volume);
					++topologyVertexOffset;
				}
				else if( auto polygonRun = std::dynamic_pointer_cast<HouGeoAdapter::PolyPrimitive>(primitive) )
				{
					exportPrimitive(context, polygonRun, topologyVertexOffset);
					for( int polygonIndex=0;polygonIndex<polygonRun->numPolys();++polygonIndex )
						topologyVertexOffset += polygonRun->numVertices(polygonIndex);
				}
			}
			writer.jsonEndArray();
		}

		std::vector<std::string> pointGroupNames;
		geometry->getPointGroupNames(pointGroupNames);
		if( !pointGroupNames.empty() )
		{
			writer.jsonString("pointgroups");
			writer.jsonBeginArray();
			for( const std::string &name : pointGroupNames )
			{
				std::vector<bool> membership;
				if( !geometry->getPointGroupMembership(name, membership)
					|| membership.size() != static_cast<size_t>(pointCount) )
					throw std::runtime_error( "HouGeoIO::exportGeometry invalid point group " + name );
				exportGroup(context, name, membership);
			}
			writer.jsonEndArray();
		}

		std::vector<std::string> vertexGroupNames;
		geometry->getVertexGroupNames(vertexGroupNames);
		if( !vertexGroupNames.empty() )
		{
			writer.jsonString("vertexgroups");
			writer.jsonBeginArray();
			for( const std::string &name : vertexGroupNames )
			{
				std::vector<bool> membership;
				if( !geometry->getVertexGroupMembership(name, membership)
					|| membership.size() != static_cast<size_t>(vertexCount) )
					throw std::runtime_error( "HouGeoIO::exportGeometry invalid vertex group " + name );
				exportGroup(context, name, membership);
			}
			writer.jsonEndArray();
		}

		std::vector<std::string> primitiveGroupNames;
		geometry->getPrimitiveGroupNames(primitiveGroupNames);
		if( !primitiveGroupNames.empty() )
		{
			writer.jsonString("primitivegroups");
			writer.jsonBeginArray();
			for( const std::string &name : primitiveGroupNames )
			{
				std::vector<bool> membership;
				if( !geometry->getPrimitiveGroupMembership(name, membership)
					|| membership.size() != static_cast<size_t>(primitiveCount) )
					throw std::runtime_error( "HouGeoIO::exportGeometry invalid primitive group " + name );
				exportGroup(context, name, membership);
			}
			writer.jsonEndArray();
		}

		writer.jsonEndArray();
		return output->good();
	}






	bool HouGeoIO::exportAttribute( ExportContext &context, HouGeoAdapter::AttributeAdapter::Ptr attribute )
	{
		if( !attribute )
			return false;
		json::BinaryWriter &writer = context.writer;

		std::string attributeType;
		std::string storageName;
		const int sourceTupleSize = attribute->getTupleSize();
		const std::string name = attribute->getName();
		const bool promotePosition = name == "P" && sourceTupleSize == 3;
		const int exportTupleSize = promotePosition ? 4 : sourceTupleSize;
		const int elementCount = attribute->getNumElements();

		if( attribute->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC )
			attributeType = "numeric";
		else if( attribute->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_STRING )
			attributeType = "string";
		else if( attribute->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_DICT )
			attributeType = "dict";
		else
			throw std::runtime_error( "HouGeoIO::exportAttribute: unsupported type for attribute " + name );

		if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
			storageName = "fpreal16";
		else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
			storageName = "fpreal32";
		else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
			storageName = "fpreal64";
		else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
			storageName = "int32";
		else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64 )
			storageName = "int64";

		if( attribute->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC && storageName.empty() )
			throw std::runtime_error( "HouGeoIO::exportAttribute: unsupported storage for attribute " + name );

		if( name == "P" && exportTupleSize != 4 )
			throw std::runtime_error( "HouGeoIO::exportAttribute: P must contain either three or four components" );

		writer.jsonBeginArray();

		writer.jsonBeginArray();
		writer.jsonString( "scope" );
		writer.jsonString( "public" );
		writer.jsonString( "type" );
		writer.jsonString(attributeType);
		writer.jsonString( "name" );
		writer.jsonString( name );
		writer.jsonString( "options" );
		writer.jsonBeginMap();
		writer.jsonEndMap();
		writer.jsonEndArray();

		writer.jsonBeginArray();
		if( attribute->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC )
		{
			writer.jsonString( "size" );
			writer.jsonInt( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString( storageName );

			writer.jsonString( "values" );
			writer.jsonBeginArray();
			writer.jsonString( "size" );
			writer.jsonInt( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString( storageName );
			writer.jsonString( "pagesize" );
			writer.jsonInt( 1024 );
			writer.jsonString( "rawpagedata" );

			HouGeoAdapter::RawPointer::Ptr rawPointer = attribute->getRawPointer();
			if( !rawPointer || (!rawPointer->ptr && elementCount > 0) )
				throw std::runtime_error( "HouGeoIO::exportAttribute: missing data for attribute " + name );

			if( promotePosition )
			{
				if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
				{
					const uword* source = static_cast<const uword*>(rawPointer->ptr);
					std::vector<uword> promotedData(static_cast<size_t>(elementCount) * 4u);
					for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
					{
						const size_t sourceOffset = static_cast<size_t>(elementIndex) * 3u;
						const size_t destinationOffset = static_cast<size_t>(elementIndex) * 4u;
						promotedData[destinationOffset] = source[sourceOffset];
						promotedData[destinationOffset + 1u] = source[sourceOffset + 1u];
						promotedData[destinationOffset + 2u] = source[sourceOffset + 2u];
						promotedData[destinationOffset + 3u] = floatToHalfBits(1.0f);
					}
					writer.jsonUniformArrayReal16(promotedData.data(), static_cast<sint64>(promotedData.size()));
				}
				else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				{
					const real32* source = static_cast<const real32*>(rawPointer->ptr);
					std::vector<real32> promotedData(static_cast<size_t>(elementCount) * 4u);
					for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
					{
						const size_t sourceOffset = static_cast<size_t>(elementIndex) * 3u;
						const size_t destinationOffset = static_cast<size_t>(elementIndex) * 4u;
						promotedData[destinationOffset] = source[sourceOffset];
						promotedData[destinationOffset + 1u] = source[sourceOffset + 1u];
						promotedData[destinationOffset + 2u] = source[sourceOffset + 2u];
						promotedData[destinationOffset + 3u] = 1.0f;
					}
					writer.jsonUniformArray(promotedData);
				}
				else
					throw std::runtime_error( "HouGeoIO::exportAttribute: unsupported three-component P storage" );
			}
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
				writer.jsonUniformArrayReal16(static_cast<const uword*>(rawPointer->ptr), elementCount * sourceTupleSize);
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				writer.jsonUniformArray<real32>( static_cast<const real32*>(rawPointer->ptr), elementCount * sourceTupleSize );
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
				writer.jsonUniformArray<real64>( static_cast<const real64*>(rawPointer->ptr), elementCount * sourceTupleSize );
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
				writer.jsonUniformArray<sint32>( static_cast<const sint32*>(rawPointer->ptr), elementCount * sourceTupleSize );
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64 )
				writer.jsonUniformArray<sint64>( static_cast<const sint64*>(rawPointer->ptr), elementCount * sourceTupleSize );

			writer.jsonEndArray();
		}
		else if( attribute->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_STRING )
		{
			writer.jsonString( "size" );
			writer.jsonInt( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString( "int32" );

			std::map<std::string, sint32> stringLookup;
			std::vector<std::string> stringTable;
			std::vector<sint32> stringIndices;
			stringTable.reserve(static_cast<size_t>(elementCount));
			stringIndices.reserve(static_cast<size_t>(elementCount));
			for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
			{
				const std::string value = attribute->getString(elementIndex);
				auto insertion = stringLookup.emplace(value, static_cast<sint32>(stringTable.size()));
				if( insertion.second )
					stringTable.push_back(value);
				stringIndices.push_back(insertion.first->second);
			}

			writer.jsonString( "strings" );
			writer.jsonBeginArray();
			for( const std::string &value : stringTable )
				writer.jsonString(value);
			writer.jsonEndArray();

			writer.jsonString( "indices" );
			writer.jsonBeginArray();
			writer.jsonString( "size" );
			writer.jsonInt32( 1 );
			writer.jsonString( "storage" );
			writer.jsonString( "int32" );
			writer.jsonString( "pagesize" );
			writer.jsonInt32( 1024 );
			writer.jsonString( "rawpagedata" );
			writer.jsonUniformArray(stringIndices);
			writer.jsonEndArray();
		}else if( attribute->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_DICT )
		{
			std::vector<std::shared_ptr<json::Object>> dictionaries;
			dictionaries.reserve(static_cast<size_t>(elementCount));
			for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
			{
				std::shared_ptr<json::Object> dictionary = attribute->getDictionary(elementIndex);
				if( !dictionary )
					throw std::runtime_error( "HouGeoIO::exportAttribute: invalid dictionary data for attribute " + name );
				dictionaries.push_back(std::move(dictionary));
			}

			writer.jsonString( "size" );
			writer.jsonInt( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString( "int32" );
			writer.jsonString( "dicts" );
			writer.jsonBeginArray();
			for( const std::shared_ptr<json::Object> &dictionary : dictionaries )
				writeJsonObject(writer, dictionary);
			writer.jsonEndArray();

			std::vector<sint32> dictionaryIndices(static_cast<size_t>(elementCount));
			for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
				dictionaryIndices[static_cast<size_t>(elementIndex)] = static_cast<sint32>(elementIndex);
			writer.jsonString( "indices" );
			writer.jsonBeginArray();
			writer.jsonString( "size" );
			writer.jsonInt32( 1 );
			writer.jsonString( "storage" );
			writer.jsonString( "int32" );
			writer.jsonString( "pagesize" );
			writer.jsonInt32( 1024 );
			writer.jsonString( "rawpagedata" );
			writer.jsonUniformArray(dictionaryIndices);
			writer.jsonEndArray();
		}

		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportTopology( ExportContext &context, HouGeoAdapter::Topology::Ptr topology )
	{
		if( !topology )
			return false;
		json::BinaryWriter &writer = context.writer;

		std::vector<sint32> indices;
		topology->getIndices(indices);

		bool requires32BitIndices = false;
		for( const sint32 index : indices )
		{
			if( index < 0 )
				throw std::runtime_error( "HouGeoIO::exportTopology cannot export negative point indices" );
			if( index > std::numeric_limits<sint16>::max() )
				requires32BitIndices = true;
		}

		writer.jsonString( "pointref" );
		writer.jsonBeginArray();
		writer.jsonString( "indices" );
		if( requires32BitIndices )
		{
			writer.jsonUniformArray<sint32>(indices);
		}
		else
		{
			std::vector<sint16> compactIndices;
			compactIndices.reserve(indices.size());
			for( const sint32 index : indices )
				compactIndices.push_back(static_cast<sint16>(index));
			writer.jsonUniformArray<sint16>(compactIndices);
		}
		writer.jsonEndArray();

		return true;
	}


	bool HouGeoIO::exportGroup( ExportContext &context, const std::string &name, const std::vector<bool> &membership )
	{
		if( name.empty() )
			throw std::runtime_error( "HouGeoIO::exportGroup requires a non-empty name" );
		json::BinaryWriter &writer = context.writer;

		std::vector<sbyte> encodedMembership;
		encodedMembership.reserve(membership.size());
		for( const bool selected : membership )
			encodedMembership.push_back(static_cast<sbyte>(selected ? 1 : 0));

		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("name");
		writer.jsonString(name);
		writer.jsonEndArray();

		writer.jsonBeginArray();
		writer.jsonString("selection");
		writer.jsonBeginArray();
		writer.jsonString("unordered");
		writer.jsonBeginArray();
		writer.jsonString("i8");
		writer.jsonUniformArray(encodedMembership);
		writer.jsonEndArray();
		writer.jsonEndArray();
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context, HouGeoAdapter::VolumePrimitive::Ptr volume )
	{
		if( !volume )
			return false;
		json::BinaryWriter &writer = context.writer;
		const math::V3i resolution = volume->getResolution();

		writer.jsonBeginArray();

		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString("Volume");
		writer.jsonEndArray();

		writer.jsonBeginArray();
		writer.jsonString("vertex");
		writer.jsonInt32(volume->getVertex());

		writer.jsonString("transform");
		const math::M44f translation = math::M44f::TranslationMatrix(volume->getTransform().getTranslation());
		const math::M44f rotationScale = math::M44f::ScaleMatrix(0.5f)
			* math::M44f::TranslationMatrix(1.0, 1.0, 1.0)
			* volume->getTransform() * translation.inverted();
		const math::M33f transform( rotationScale.ma[0], rotationScale.ma[1], rotationScale.ma[2],
			rotationScale.ma[4], rotationScale.ma[5], rotationScale.ma[6],
			rotationScale.ma[8], rotationScale.ma[9], rotationScale.ma[10]);
		writer.jsonUniformArray<real32>(transform.ma.data(), 9);

		writer.jsonString("res");
		writer.jsonUniformArray<sint32>( &resolution.x, 3 );

		writer.jsonString("border");
		writer.jsonBeginMap();
		writer.jsonKey("type");
		writer.jsonString("constant");
		writer.jsonKey("value");
		writer.jsonReal32(0.0f);
		writer.jsonEndMap();

		writer.jsonString("compression");
		writer.jsonBeginMap();
		writer.jsonKey("tolerance");
		writer.jsonReal32(0.0f);
		writer.jsonEndMap();

		writer.jsonString("voxels");
		writer.jsonBeginArray();
		writer.jsonString("tiledarray");
		writer.jsonBeginArray();
		writer.jsonString("version");
		writer.jsonInt32( 1 );

		writer.jsonString("compressiontypes");
		writer.jsonBeginArray();
		writer.jsonString("raw");
		writer.jsonString("rawfull");
		writer.jsonString("constant");
		writer.jsonString("fpreal16");
		writer.jsonString("FP32Range");
		writer.jsonEndArray();

		writer.jsonString("tiles");
		writer.jsonBeginArray();

		const math::V3i tileCount(
			resolution.x / 16 + (resolution.x % 16 != 0 ? 1 : 0),
			resolution.y / 16 + (resolution.y % 16 != 0 ? 1 : 0),
			resolution.z / 16 + (resolution.z % 16 != 0 ? 1 : 0));
		math::Vec3i voxelOffset;
		math::Vec3i tileResolution;
		std::vector<real32> tileValues;
		for( int tileZ=0;tileZ<tileCount.z;++tileZ )
		{
			voxelOffset.z = tileZ * 16;
			tileResolution.z = std::min(16, resolution.z - voxelOffset.z);
			for( int tileY=0;tileY<tileCount.y;++tileY )
			{
				voxelOffset.y = tileY * 16;
				tileResolution.y = std::min(16, resolution.y - voxelOffset.y);
				for( int tileX=0;tileX<tileCount.x;++tileX )
				{
					voxelOffset.x = tileX * 16;
					tileResolution.x = std::min(16, resolution.x - voxelOffset.x);

					writer.jsonBeginArray();
					writer.jsonString("compression");
					writer.jsonInt32(0);
					writer.jsonString("data");
					tileValues.clear();
					const math::V3i voxelEnd(
						voxelOffset.x + tileResolution.x,
						voxelOffset.y + tileResolution.y,
						voxelOffset.z + tileResolution.z);
					for( int voxelZ=voxelOffset.z;voxelZ<voxelEnd.z;++voxelZ )
						for( int voxelY=voxelOffset.y;voxelY<voxelEnd.y;++voxelY )
							for( int voxelX=voxelOffset.x;voxelX<voxelEnd.x;++voxelX )
								tileValues.push_back(volume->getVoxel(voxelX, voxelY, voxelZ));
					writer.jsonUniformArray(tileValues);
					writer.jsonEndArray();
				}
			}
		}

		writer.jsonEndArray();
		writer.jsonEndArray();
		writer.jsonEndArray();

		writer.jsonString("visualization");
		writer.jsonBeginMap();
		writer.jsonKey("mode");
		writer.jsonString(volume->getVisualizationMode());
		writer.jsonKey("iso");
		writer.jsonReal32(volume->getVisualizationIso());
		writer.jsonKey("density");
		writer.jsonReal32(volume->getVisualizationDensity());
		writer.jsonEndMap();

		writer.jsonString("taperx");
		writer.jsonReal32(1.0f);
		writer.jsonString("tapery");
		writer.jsonReal32(1.0f);

		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::PolyPrimitive::Ptr polygonRun, int startVertex )
	{
		if( !polygonRun || startVertex < 0 || polygonRun->numPolys() <= 0 )
			return false;
		json::BinaryWriter &writer = context.writer;

		std::vector<sint32> vertexCounts;
		vertexCounts.reserve(static_cast<size_t>(polygonRun->numPolys()));
		for( int polygonIndex=0;polygonIndex<polygonRun->numPolys();++polygonIndex )
		{
			const int vertexCount = polygonRun->numVertices(polygonIndex);
			if( vertexCount <= 0 )
				throw std::runtime_error( "HouGeoIO::exportPrimitive: polygon has no vertices" );
			vertexCounts.push_back(vertexCount);
		}

		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString(polygonRun->closed() ? "Polygon_run" : "PolygonCurve_run");
		writer.jsonEndArray();

		writer.jsonBeginArray();
		writer.jsonString("startvertex");
		writer.jsonInt32(startVertex);
		writer.jsonString("nprimitives");
		writer.jsonInt32(polygonRun->numPolys());
		writer.jsonString("nvertices");
		writer.jsonUniformArray(vertexCounts);
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

}










