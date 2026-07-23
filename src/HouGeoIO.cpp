#include <houio/HouGeoIO.h>
#include <houio/GeometryIO.h>

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

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

		size_t expectedAttributeBytes(
			const HouGeoAdapter::AttributeAdapter::Ptr& attribute,
			const std::string& path)
		{
			const int component_bytes = HouGeoAdapter::AttributeAdapter::storageSize(attribute->getStorage());
			if (component_bytes <= 0 || attribute->getTupleSize() <= 0 || attribute->getNumElements() < 0)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute has invalid numeric storage metadata", -1, path});
			const size_t component_count = static_cast<size_t>(attribute->getTupleSize());
			const size_t element_count = static_cast<size_t>(attribute->getNumElements());
			const size_t component_size = static_cast<size_t>(component_bytes);
			if (component_count > std::numeric_limits<size_t>::max() / component_size)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute tuple byte count overflow", -1, path});
			const size_t element_bytes = component_count * component_size;
			if (element_count > std::numeric_limits<size_t>::max() / element_bytes)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute byte count overflow", -1, path});
			return element_count * element_bytes;
		}

		HouGeoAdapter::RawDataView requireRawAttributeData(
			const HouGeoAdapter::AttributeAdapter::Ptr& attribute,
			const std::string& path)
		{
			const HouGeoAdapter::RawDataView raw_data = attribute->rawData();
			const size_t expected_bytes = expectedAttributeBytes(attribute, path);
			if (!raw_data.available())
			{
				if (expected_bytes == 0)
					return HouGeoAdapter::RawDataView(std::span<const std::byte>());
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute has no raw data", -1, path});
			}
			if (raw_data.sizeBytes() != expected_bytes)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute raw byte count does not match its metadata", -1, path});
			return raw_data;
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
		std::vector<T> copyUniformValues(const json::ArrayPtr& array)
		{
			const sint64 uniform_element_count = array->uniformElementCount();
			if (uniform_element_count < 0)
				throw std::runtime_error("Cannot export a uniform array with a negative element count");
			const size_t element_count = static_cast<size_t>(uniform_element_count);
			if (element_count > std::numeric_limits<size_t>::max() / sizeof(T))
				throw std::length_error("Uniform JSON array byte count overflow");
			const std::span<const std::byte> bytes = array->uniformData();
			if (element_count * sizeof(T) != bytes.size())
				throw std::runtime_error("Cannot export a uniform array with inconsistent storage");
			std::vector<T> values(element_count);
			if (element_count > 0)
				std::memcpy(values.data(), bytes.data(), bytes.size());
			return values;
		}

		template<typename T>
		std::vector<T> copyRawValues(
			const HouGeoAdapter::RawDataView& raw_data,
			size_t scalar_count,
			const std::string& description)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			if (!raw_data.available())
				throw std::runtime_error(description + " has no raw data");
			if (scalar_count > std::numeric_limits<size_t>::max() / sizeof(T))
				throw std::length_error(description + " byte count overflow");
			const size_t expected_bytes = scalar_count * sizeof(T);
			if (raw_data.sizeBytes() != expected_bytes)
				throw std::runtime_error(description + " raw byte count is inconsistent");
			std::vector<T> values(scalar_count);
			if (expected_bytes > 0)
				std::memcpy(values.data(), raw_data.bytes().data(), expected_bytes);
			return values;
		}

		void writeJsonValue( json::BinaryWriter &writer, json::Value value );

		void writeJsonArray( json::BinaryWriter &writer, const json::ArrayPtr &array )
		{
			if( !array )
				throw std::runtime_error( "Cannot export a null JSON array" );
			if( array->isUniform() )
			{
				switch (array->uniformTypeIndex())
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
			for (const json::Value& item : array->elements())
				writeJsonValue(writer, item);
			writer.jsonEndArray();
		}

		void writeJsonObject( json::BinaryWriter &writer, const json::ObjectPtr &object )
		{
			if( !object )
				throw std::runtime_error( "Cannot export a null JSON object" );
			writer.jsonBeginMap();
			for (const auto& entry : object->entries())
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

	HouGeo::Ptr HouGeoIO::import(std::istream& input)
	{
		return import(input, json::ParserLimits(), nullptr);
	}

	HouGeo::Ptr HouGeoIO::import(std::istream& input, DiagnosticList* diagnostics)
	{
		return import(input, json::ParserLimits(), diagnostics);
	}

	HouGeo::Ptr HouGeoIO::import(
		std::istream& input,
		const json::ParserLimits& limits)
	{
		return import(input, limits, nullptr);
	}

	HouGeo::Ptr HouGeoIO::import(
		std::istream& input,
		const json::ParserLimits& limits,
		DiagnosticList* diagnostics)
	{
		json::JSONReader reader;
		json::Parser parser(limits);
		const bool parsed = diagnostics
			? parser.parse(input, reader, *diagnostics)
			: parser.parse(input, reader);
		if (!parsed)
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
		const sint64 numPoints = houGeo->pointCount();
		const sint64 numVertices = houGeo->vertexCount();
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
				static_cast<void>(poly->vertices(polygonIndex));
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

		const std::vector<std::string> pointAttributesNames = houGeo->pointAttributeNames();
		const std::vector<std::string> vertexAttributesNames = houGeo->vertexAttributeNames();

		// convert point attributes ---
		for (const std::string& attribute_name : pointAttributesNames)
		{
			std::string attrName = attribute_name;
			HouGeoAdapter::AttributeAdapter::Ptr houAttr = houGeo->pointAttribute(attrName);
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
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);

				attr = result->getAttr("P");
				if( !attr )
					throw std::runtime_error( "HouGeoIO::convertToGeometry target geometry has no P attribute" );
				for( size_t pointIndex=0;pointIndex<pointCount;++pointIndex )
				{
					math::Vec3f position;
					const size_t tupleOffset = pointIndex * static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
					{
						position = math::Vec3f(
							halfBitsToFloat(raw_data.read<uword>(tupleOffset)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 1)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 2)));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						position = math::Vec3f(
							raw_data.read<real32>(tupleOffset),
							raw_data.read<real32>(tupleOffset + 1),
							raw_data.read<real32>(tupleOffset + 2));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						position = math::Vec3f(
							static_cast<real32>(raw_data.read<real64>(tupleOffset)),
							static_cast<real32>(raw_data.read<real64>(tupleOffset + 1)),
							static_cast<real32>(raw_data.read<real64>(tupleOffset + 2)));
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
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);

				attrName = "UV";
				attr = Attribute::createV2f();
				for( size_t pointIndex=0;pointIndex<pointCount;++pointIndex )
				{
					math::Vec2f uv;
					const size_t tupleOffset = pointIndex * static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
					{
						uv = math::Vec2f(
							halfBitsToFloat(raw_data.read<uword>(tupleOffset)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 1)));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						uv = math::Vec2f(
							raw_data.read<real32>(tupleOffset),
							raw_data.read<real32>(tupleOffset + 1));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						uv = math::Vec2f(
							static_cast<real32>(raw_data.read<real64>(tupleOffset)),
							static_cast<real32>(raw_data.read<real64>(tupleOffset + 1)));
					}
					else
						throw std::runtime_error( "HouGeoIO::convertToGeometry: unsupported UV storage" );
					attr->appendElement(uv);
				}
			}else
			if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::HALF,
					raw_data.bytes(), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::FLOAT,
					raw_data.bytes(), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::INT,
					raw_data.bytes(), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::INT64,
					raw_data.bytes(), houAttr->getNumElements());
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
		for (const std::string& attribute_name : vertexAttributesNames)
		{
			std::string attrName = attribute_name;
			HouGeoAdapter::AttributeAdapter::Ptr houAttr = houGeo->vertexAttribute(attrName);
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
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);

				attrName = "UV";
				attr = Attribute::createV2f();

				for( size_t vertexIndex=0;vertexIndex<vertexCount;++vertexIndex )
				{
					math::Vec2f uv;
					const size_t tupleOffset = vertexIndex * static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
					{
						uv = math::Vec2f(
							halfBitsToFloat(raw_data.read<uword>(tupleOffset)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 1)));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						uv = math::Vec2f(
							raw_data.read<real32>(tupleOffset),
							raw_data.read<real32>(tupleOffset + 1));
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						uv = math::Vec2f(
							static_cast<real32>(raw_data.read<real64>(tupleOffset)),
							static_cast<real32>(raw_data.read<real64>(tupleOffset + 1)));
					}
					else
						throw std::runtime_error( "HouGeoIO::convertToGeometry: unsupported vertex UV storage" );
					attr->appendElement(uv);
				}
			}else
			if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::HALF,
					raw_data.bytes(), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::FLOAT,
					raw_data.bytes(), houAttr->getNumElements());
			}
			else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::INT64,
					raw_data.bytes(), houAttr->getNumElements());
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
							const std::span<const std::byte> current_value =
								attributePair.first->elementBytes(globalVertexIndex);
							const std::span<const std::byte> first_value = attributePair.first->elementBytes(
								static_cast<size_t>(firstVertex[pointIndex]));
							if (current_value.size() != elementBytes || first_value.size() != elementBytes)
								throw std::runtime_error("Vertex attribute element byte size is inconsistent");
							if (std::memcmp(current_value.data(), first_value.data(), elementBytes) != 0)
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
						std::span<std::byte> destination_value =
							attributePair.second->elementBytes(static_cast<size_t>(finalPointIndex));
						const std::span<const std::byte> source_value =
							attributePair.first->elementBytes(globalVertexIndex);
						if (destination_value.size() != elementBytes || source_value.size() != elementBytes)
							throw std::runtime_error("Converted vertex attribute element byte size is inconsistent");
						std::memcpy(destination_value.data(), source_value.data(), elementBytes);
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

	void HouGeoIO::makeLog(const std::string& path, std::ostream& output)
	{
		std::ifstream input(path, std::ios_base::in | std::ios_base::binary);
		if (!input)
			throw std::runtime_error("HouGeoIO::makeLog could not open " + path);
		json::JSONLogger logger(output);
		json::Parser parser;
		if (!parser.parse(input, logger))
			throw std::runtime_error("HouGeoIO::makeLog could not parse the input geometry");
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
			const std::span<const Geometry::Index> geometry_indices = geometry->indexBuffer();
			topology->reserve(geometry_indices.size());
			for (const unsigned int point_index : geometry_indices)
			{
				if (point_index > static_cast<unsigned int>(std::numeric_limits<int>::max()))
					throw std::overflow_error("HouGeoIO::adaptGeometry point index exceeds int range");
				topology->appendIndex(static_cast<int>(point_index));
			}
			houdiniGeometry->setTopology(topology);

			// The simplified Geometry model uses a fixed vertex count, so all polygons form one run.
			HouGeo::HouPoly::Ptr polygon_run = std::make_shared<HouGeo::HouPoly>();
			const int primitive_count = geometry->numPrimitives();
			const int vertices_per_primitive = geometry->numPrimitiveVertices();
			std::vector<int> vertex_offsets(static_cast<size_t>(primitive_count), 0);
			for (int primitive_index = 1; primitive_index < primitive_count; ++primitive_index)
			{
				vertex_offsets[static_cast<size_t>(primitive_index)] =
					vertex_offsets[static_cast<size_t>(primitive_index - 1)]
					+ vertices_per_primitive;
			}
			std::vector<int> vertex_counts(
				static_cast<size_t>(primitive_count),
				vertices_per_primitive);

			if (geometry_indices.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
				throw std::overflow_error("HouGeoIO::adaptGeometry index buffer exceeds int range");
			std::vector<int> point_indices(geometry_indices.size());
			for (size_t vertex_index = 0; vertex_index < geometry_indices.size(); ++vertex_index)
				point_indices[vertex_index] = static_cast<int>(vertex_index);

			polygon_run->setPolygonData(
				primitive_count,
				std::move(vertex_counts),
				std::move(vertex_offsets),
				std::move(point_indices),
				geometry->primitiveType() != Geometry::LINE);
			houdiniGeometry->addPrimitive(polygon_run);
		}

		return houdiniGeometry;
	}

	bool HouGeoIO::exportGeometry( const std::string &filename, Geometry::Ptr geometry )
	{
		return static_cast<bool>(GeometryIO::writeGeometry(filename, geometry));
	}

	bool HouGeoIO::exportPoints(
		const std::string& filename,
		std::span<const math::V3f> points)
	{
		std::map<std::string, std::vector<math::V3f>> point_attributes;
		point_attributes["P"] = std::vector<math::V3f>(points.begin(), points.end());
		return exportPointAttributes(filename, point_attributes);
	}

	bool HouGeoIO::exportPointAttributes(
		const std::string& filename,
		const std::map<std::string, std::vector<math::V3f>>& point_attributes)
	{
		Geometry::Ptr geometry = Geometry::createPointGeometry();

		for( const auto &entry : point_attributes )
		{
			const std::string &name = entry.first;
			const std::vector<math::V3f> &values = entry.second;
			const int elementCount = static_cast<int>(values.size());
			if( elementCount == 0 )
				continue;

			Attribute::Ptr attribute = Attribute::createV3f(elementCount);
			const std::span<const std::byte> source_bytes = std::as_bytes(std::span(values));
			std::span<std::byte> destination_bytes = attribute->bytes();
			if (source_bytes.size() != destination_bytes.size())
				throw std::runtime_error("HouGeoIO::exportPointAttributes byte size mismatch");
			std::memcpy(destination_bytes.data(), source_bytes.data(), source_bytes.size());
			geometry->setAttr(name, attribute);
		}

		return exportGeometry(filename, geometry);
	}




	bool HouGeoIO::exportGeometry(
		std::ostream& output,
		HouGeoAdapter::Ptr geometry,
		bool binary)
	{
		if (!geometry || !output.good() || !binary)
			return false;

		json::BinaryWriter writer(output);
		ExportContext context(writer);

		const sint64 pointCount = geometry->pointCount();
		const sint64 vertexCount = geometry->vertexCount();
		const sint64 primitiveCount = geometry->primitiveCount();
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
		if (const HouGeoAdapter::Topology::Ptr geometry_topology = geometry->topology())
			exportTopology(context, geometry_topology);
		writer.jsonEndArray();

		writer.jsonString( "attributes" );
		writer.jsonBeginArray();

		writer.jsonString( "pointattributes" );
		writer.jsonBeginArray();
		for (const std::string& name : geometry->pointAttributeNames())
			exportAttribute(context, geometry->pointAttribute(name));
		writer.jsonEndArray();

		writer.jsonString( "vertexattributes" );
		writer.jsonBeginArray();
		for (const std::string& name : geometry->vertexAttributeNames())
			exportAttribute(context, geometry->vertexAttribute(name));
		writer.jsonEndArray();

		writer.jsonString( "primitiveattributes" );
		writer.jsonBeginArray();
		for (const std::string& name : geometry->primitiveAttributeNames())
			exportAttribute(context, geometry->primitiveAttribute(name));
		writer.jsonEndArray();

		writer.jsonString( "globalattributes" );
		writer.jsonBeginArray();
		for (const std::string& name : geometry->globalAttributeNames())
			exportAttribute(context, geometry->globalAttribute(name));
		writer.jsonEndArray();

		writer.jsonEndArray();

		if( primitiveCount > 0 )
		{
			writer.jsonString( "primitives" );
			writer.jsonBeginArray();

			const std::vector<HouGeoAdapter::Primitive::Ptr> primitives = geometry->primitives();

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

		const std::vector<std::string> point_group_names = geometry->pointGroupNames();
		if (!point_group_names.empty())
		{
			writer.jsonString("pointgroups");
			writer.jsonBeginArray();
			for (const std::string& name : point_group_names)
			{
				const auto membership = geometry->pointGroupMembership(name);
				if (!membership || membership->size() != static_cast<size_t>(pointCount))
					throw std::runtime_error("HouGeoIO::exportGeometry invalid point group " + name);
				exportGroup(context, name, *membership);
			}
			writer.jsonEndArray();
		}

		const std::vector<std::string> vertex_group_names = geometry->vertexGroupNames();
		if (!vertex_group_names.empty())
		{
			writer.jsonString("vertexgroups");
			writer.jsonBeginArray();
			for (const std::string& name : vertex_group_names)
			{
				const auto membership = geometry->vertexGroupMembership(name);
				if (!membership || membership->size() != static_cast<size_t>(vertexCount))
					throw std::runtime_error("HouGeoIO::exportGeometry invalid vertex group " + name);
				exportGroup(context, name, *membership);
			}
			writer.jsonEndArray();
		}

		const std::vector<std::string> primitive_group_names = geometry->primitiveGroupNames();
		if (!primitive_group_names.empty())
		{
			writer.jsonString("primitivegroups");
			writer.jsonBeginArray();
			for (const std::string& name : primitive_group_names)
			{
				const auto membership = geometry->primitiveGroupMembership(name);
				if (!membership || membership->size() != static_cast<size_t>(primitiveCount))
					throw std::runtime_error("HouGeoIO::exportGeometry invalid primitive group " + name);
				exportGroup(context, name, *membership);
			}
			writer.jsonEndArray();
		}

		writer.jsonEndArray();
		return output.good();
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
		if (sourceTupleSize <= 0 || elementCount < 0)
			throw std::runtime_error("HouGeoIO::exportAttribute: invalid element metadata for attribute " + name);
		const size_t element_count = static_cast<size_t>(elementCount);
		const size_t tuple_size = static_cast<size_t>(sourceTupleSize);
		if (element_count > std::numeric_limits<size_t>::max() / tuple_size)
			throw std::length_error("HouGeoIO::exportAttribute: scalar count overflow for attribute " + name);
		const size_t scalar_count = element_count * tuple_size;

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

			const HouGeoAdapter::RawDataView raw_data =
				requireRawAttributeData(attribute, "attributes." + name);

			if( promotePosition )
			{
				if (element_count > std::numeric_limits<size_t>::max() / 4u)
					throw std::length_error("HouGeoIO::exportAttribute: promoted P size overflow");
				if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16 )
				{
					const std::vector<uword> source = copyRawValues<uword>(
						raw_data, scalar_count, "HouGeoIO::exportAttribute P");
					std::vector<uword> promotedData(element_count * 4u);
					for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
					{
						const size_t sourceOffset = static_cast<size_t>(elementIndex) * 3u;
						const size_t destinationOffset = static_cast<size_t>(elementIndex) * 4u;
						promotedData[destinationOffset] = source[sourceOffset];
						promotedData[destinationOffset + 1u] = source[sourceOffset + 1u];
						promotedData[destinationOffset + 2u] = source[sourceOffset + 2u];
						promotedData[destinationOffset + 3u] = floatToHalfBits(1.0f);
					}
					writer.jsonUniformArrayReal16(std::span<const uword>(promotedData));
				}
				else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				{
					const std::vector<real32> source = copyRawValues<real32>(
						raw_data, scalar_count, "HouGeoIO::exportAttribute P");
					std::vector<real32> promotedData(element_count * 4u);
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
			{
				const std::vector<uword> values = copyRawValues<uword>(raw_data, scalar_count, name);
				writer.jsonUniformArrayReal16(std::span<const uword>(values));
			}
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				writer.jsonUniformArray(copyRawValues<real32>(raw_data, scalar_count, name));
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
				writer.jsonUniformArray(copyRawValues<real64>(raw_data, scalar_count, name));
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
				writer.jsonUniformArray(copyRawValues<sint32>(raw_data, scalar_count, name));
			else if( attribute->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64 )
				writer.jsonUniformArray(copyRawValues<sint64>(raw_data, scalar_count, name));

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
		writer.jsonUniformArray(std::span<const real32>(transform.ma));

		writer.jsonString("res");
		const std::array<sint32, 3> resolution_values = {
			resolution.x,
			resolution.y,
			resolution.z,
		};
		writer.jsonUniformArray(std::span<const sint32>(resolution_values));

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










