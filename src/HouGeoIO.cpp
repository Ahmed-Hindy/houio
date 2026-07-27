#include <houio/HouGeoIO.h>
#include <houio/GeometryIO.h>
#include <houio/NativeVdbPayload.h>
#include <houio/OpenVdbBackend.h>

#include <algorithm>
#include <array>
#include <cmath>
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

		void validateDomainAttribute(
			const HouGeoAdapter::AttributeAdapter::ConstPtr& attribute,
			sint64 expectedCount,
			const std::string &path )
		{
			if( !attribute )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry encountered a null attribute", -1, path});
			if (attribute->elementCount() != expectedCount)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry attribute element count does not match its domain", -1, path});
		}

		size_t expectedAttributeBytes(
			const HouGeoAdapter::AttributeAdapter::ConstPtr& attribute,
			const std::string& path)
		{
			const std::optional<size_t> component_size =
				HouGeoAdapter::AttributeAdapter::storageByteWidth(attribute->storage());
			if (!component_size || attribute->elementCount() < 0)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute has invalid numeric storage metadata", -1, path});
			const size_t component_count = attribute->tupleSize().asSize();
			const size_t element_count = static_cast<size_t>(attribute->elementCount());
			if (component_count > std::numeric_limits<size_t>::max() / *component_size)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute tuple byte count overflow", -1, path});
			const size_t element_bytes = component_count * *component_size;
			if (element_count > std::numeric_limits<size_t>::max() / element_bytes)
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO attribute byte count overflow", -1, path});
			return element_count * element_bytes;
		}

		HouGeoAdapter::RawDataView requireRawAttributeData(
			const HouGeoAdapter::AttributeAdapter::ConstPtr& attribute,
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
				writer.jsonNull();
			else
			{
				JsonScalarWriter scalarWriter{writer};
				std::visit(scalarWriter, value.variant());
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
			json::ArrayPtr rootArray = reader.root().asArray();
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

	Geometry::Ptr HouGeoIO::convertToGeometry(
		HouGeo::ConstPtr houGeo,
		HouGeoAdapter::Primitive::ConstPtr houPrim )
	{
		return convertToGeometry(houGeo, houPrim, nullptr, nullptr);
	}

	Geometry::Ptr HouGeoIO::convertToGeometry(
		HouGeo::ConstPtr houGeo,
		HouGeoAdapter::Primitive::ConstPtr houPrim,
		DiagnosticList *diagnostics )
	{
		return convertToGeometry(houGeo, houPrim, diagnostics, nullptr);
	}

	GeometryConversionResult HouGeoIO::convertToGeometryResult(
		HouGeo::ConstPtr houGeo,
		HouGeoAdapter::Primitive::ConstPtr houPrim )
	{
		GeometryConversionResult result;
		result.value = convertToGeometry(houGeo, houPrim, &result.diagnostics, &result.report);
		return result;
	}

	Geometry::Ptr HouGeoIO::convertToGeometry(
		HouGeo::ConstPtr houGeo,
		HouGeoAdapter::Primitive::ConstPtr houPrim,
		DiagnosticList *diagnostics,
		GeometryConversionReport *report )
	{
		if( report )
			*report = {};
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
		if( report )
		{
			report->sourcePointCount = pointCount;
			report->skippedPrimitiveAttributes = houGeo->primitiveAttributeNames();
			report->skippedGlobalAttributes = houGeo->globalAttributeNames();
			report->droppedPointGroups = houGeo->pointGroupNames();
			report->droppedVertexGroups = houGeo->vertexGroupNames();
			report->droppedPrimitiveGroups = houGeo->primitiveGroupNames();
		}

		// The simplified model supports fixed-size primitive sets and one arbitrary n-gon.
		int numPolys = 0;
		int numVerticesPerPoly = 0;
		if( houPrim )
		{
			HouGeo::HouPoly::ConstPtr poly =
				std::dynamic_pointer_cast<const HouGeo::HouPoly>(houPrim);
			if( !poly )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeoIO::convertToGeometry supports only polygon primitives", -1, "conversion.primitive"});
			numPolys = poly->polygonCount();
			if( numPolys < 0 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry polygon count cannot be negative", -1, "conversion.primitive"});

			size_t observedVertexCount = 0;
			numVerticesPerPoly = numPolys > 0 ? poly->polygonVertexCount(0) : 0;
			for( int polygonIndex=0;polygonIndex<numPolys;++polygonIndex )
			{
				const int polygonVertexCount = poly->polygonVertexCount(polygonIndex);
				if( polygonVertexCount != numVerticesPerPoly )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
						"HouGeoIO::convertToGeometry requires a constant polygon vertex count", -1,
						"conversion.primitive"});
				if( polygonVertexCount < 0 || static_cast<size_t>(polygonVertexCount) > vertexCount - observedVertexCount )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
						"HouGeoIO::convertToGeometry polygon vertices exceed the vertex domain", -1,
						"conversion.primitive"});
				static_cast<void>(poly->polygonVertexIndices(polygonIndex));
				observedVertexCount += static_cast<size_t>(polygonVertexCount);
			}
			if( observedVertexCount != vertexCount )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeoIO::convertToGeometry polygon vertex total does not match vertexcount", -1,
					"conversion.primitive"});
			if( numVerticesPerPoly >= 3 && !poly->isClosed() )
			{
				if( report )
					report->polygonClosureLost = true;
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry closes open polygons in the simplified mesh model", -1,
					"conversion.primitive.closed"});
			}
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
		else
		if( numPolys == 1 && numVerticesPerPoly > 4 )
		{
			result = Geometry::createPolyGeometry();
		}
		if( !result )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
				"HouGeoIO::convertToGeometry supports lines, triangles, quads, or one arbitrary n-gon", -1,
				"conversion.primitive"});

		// attributes ---
		std::vector<Attribute::Ptr> geoAttrs;

		const std::vector<std::string> pointAttributesNames = houGeo->pointAttributeNames();
		const std::vector<std::string> vertexAttributesNames = houGeo->vertexAttributeNames();

		// convert point attributes ---
		for (const std::string& attribute_name : pointAttributesNames)
		{
			std::string attrName = attribute_name;
			HouGeoAdapter::AttributeAdapter::ConstPtr houAttr =
				houGeo->pointAttribute(attrName);
			const std::string attributePath = "attributes.pointattributes." + attrName;
			validateDomainAttribute(houAttr, numPoints, attributePath);
			if( houAttr->type() != HouGeoAdapter::AttributeAdapter::Type::numeric )
			{
				if( report )
					report->skippedPointAttributes.push_back(attrName);
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry skips non-numeric point attribute " + attrName,
					-1, attributePath});
				continue;
			}
			const int numComponents = houAttr->tupleSize().value();
			const HouGeoAdapter::AttributeAdapter::Storage storage = houAttr->storage();

			Attribute::Ptr attr;
			if( attrName == "P" )
			{
				if( numComponents < 3 )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: P requires at least three components" );
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);

				attr = result->attribute("P");
				if( !attr )
					throw std::runtime_error( "HouGeoIO::convertToGeometry target geometry has no P attribute" );
				for( size_t pointIndex=0;pointIndex<pointCount;++pointIndex )
				{
					math::Vec3f position;
					const size_t tupleOffset = pointIndex * static_cast<size_t>(numComponents);
					if( storage == HouGeoAdapter::AttributeAdapter::Storage::float16 )
					{
						position = math::Vec3f(
							halfBitsToFloat(raw_data.read<uword>(tupleOffset)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 1)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 2)));
					}
					else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float32 )
					{
						position = math::Vec3f(
							raw_data.read<real32>(tupleOffset),
							raw_data.read<real32>(tupleOffset + 1),
							raw_data.read<real32>(tupleOffset + 2));
					}
					else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float64 )
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
					if( storage == HouGeoAdapter::AttributeAdapter::Storage::float16 )
					{
						uv = math::Vec2f(
							halfBitsToFloat(raw_data.read<uword>(tupleOffset)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 1)));
					}
					else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float32 )
					{
						uv = math::Vec2f(
							raw_data.read<real32>(tupleOffset),
							raw_data.read<real32>(tupleOffset + 1));
					}
					else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float64 )
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
			if( storage == HouGeoAdapter::AttributeAdapter::Storage::float16 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::float16,
					raw_data.bytes(), houAttr->elementCount());
			}
			else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float32 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::float32,
					raw_data.bytes(), houAttr->elementCount());
			}
			else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float64 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::float64,
					raw_data.bytes(), houAttr->elementCount());
			}
			else if( storage == HouGeoAdapter::AttributeAdapter::Storage::int32 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::int32,
					raw_data.bytes(), houAttr->elementCount());
			}
			else if( storage == HouGeoAdapter::AttributeAdapter::Storage::int64 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::int64,
					raw_data.bytes(), houAttr->elementCount());
			}
			else
			{
				if( report )
					report->skippedPointAttributes.push_back(attrName);
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry cannot convert point attribute " + attrName,
					-1, "attributes.pointattributes." + attrName});
			}

			if( attr )
				result->setAttribute(attrName, attr);
		}

		Attribute::Ptr convertedPositions = result->attribute("P");
		if( !convertedPositions || convertedPositions->numElements() != static_cast<int>(pointCount) )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeoIO::convertToGeometry requires one converted P value per point", -1,
				"attributes.pointattributes.P"});

		// convert vertex attributes ---
		std::vector<std::pair<Attribute::Ptr, Attribute::Ptr>> vertex2pointAttr;
		for (const std::string& attribute_name : vertexAttributesNames)
		{
			std::string attrName = attribute_name;
			HouGeoAdapter::AttributeAdapter::ConstPtr houAttr =
				houGeo->vertexAttribute(attrName);
			const std::string attributePath = "attributes.vertexattributes." + attrName;
			validateDomainAttribute(houAttr, numVertices, attributePath);
			if( houAttr->type() != HouGeoAdapter::AttributeAdapter::Type::numeric )
			{
				if( report )
					report->skippedVertexAttributes.push_back(attrName);
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry skips non-numeric vertex attribute " + attrName,
					-1, attributePath});
				continue;
			}
			const int numComponents = houAttr->tupleSize().value();
			const HouGeoAdapter::AttributeAdapter::Storage storage = houAttr->storage();

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
					if( storage == HouGeoAdapter::AttributeAdapter::Storage::float16 )
					{
						uv = math::Vec2f(
							halfBitsToFloat(raw_data.read<uword>(tupleOffset)),
							halfBitsToFloat(raw_data.read<uword>(tupleOffset + 1)));
					}
					else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float32 )
					{
						uv = math::Vec2f(
							raw_data.read<real32>(tupleOffset),
							raw_data.read<real32>(tupleOffset + 1));
					}
					else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float64 )
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
			if( storage == HouGeoAdapter::AttributeAdapter::Storage::float16 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::float16,
					raw_data.bytes(), houAttr->elementCount());
			}
			else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float32 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::float32,
					raw_data.bytes(), houAttr->elementCount());
			}
			else if( storage == HouGeoAdapter::AttributeAdapter::Storage::float64 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::float64,
					raw_data.bytes(), houAttr->elementCount());
			}
			else if( storage == HouGeoAdapter::AttributeAdapter::Storage::int64 )
			{
				const HouGeoAdapter::RawDataView raw_data = requireRawAttributeData(houAttr, attributePath);
				attr = Attribute::create(numComponents, Attribute::ComponentType::int64,
					raw_data.bytes(), houAttr->elementCount());
			}

			if( !attr )
			{
				if( report )
					report->skippedVertexAttributes.push_back(attrName);
				appendDiagnostic(diagnostics, Diagnostic{DiagnosticSeverity::warning, DiagnosticCategory::conversion,
					"HouGeoIO::convertToGeometry cannot convert vertex attribute " + attrName,
					-1, "attributes.vertexattributes." + attrName});
				continue;
			}

			// create point attribute which we will derive from this vertex attribute
			Attribute::Ptr pointAttr = attr->copy();
			pointAttr->resize(pointCount);
			pointAttr->fillZero();

			result->setAttribute(attrName, pointAttr);
			vertex2pointAttr.push_back( std::make_pair( attr, pointAttr ) );
		}


		// only done when we have primitives...
		if( houPrim )
		{
			HouGeo::HouPoly::ConstPtr poly =
				std::dynamic_pointer_cast<const HouGeo::HouPoly>(houPrim);
			if( !poly )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeoIO::convertToGeometry expected a polygon primitive", -1, "conversion.primitive"});

			// Geometry has no face-varying domain, so points are split when vertex values differ.
			std::vector<bool> pointsToSplit(pointCount, false);
			std::vector<bool> pointsInitialized(pointCount, false);
			std::vector<int> firstVertex(pointCount, -1);

			size_t globalVertexIndex = 0;
			for( int polygonIndex=0;polygonIndex<numPolys;++polygonIndex )
			{
				const int polygonVertexCount = poly->polygonVertexCount(polygonIndex);
				const std::span<const int> polygonVertices = poly->polygonVertexIndices(polygonIndex);
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
			if( report )
			{
				report->splitSourcePointCount = static_cast<size_t>(
					std::count(pointsToSplit.begin(), pointsToSplit.end(), true));
			}

			globalVertexIndex = 0;
			for( int polygonIndex=0;polygonIndex<numPolys;++polygonIndex )
			{
				const int polygonVertexCount = poly->polygonVertexCount(polygonIndex);
				const std::span<const int> polygonVertices = poly->polygonVertexIndices(polygonIndex);
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
					{
						finalPointIndex = result->duplicatePoint(finalPointIndex);
						if( report )
							++report->duplicatedPointCount;
					}
					else if( !pointsInitialized[pointIndex] )
						pointsInitialized[pointIndex] = true;

					for( const auto &attributePair : vertex2pointAttr )
					{
						const size_t elementBytes = attributeElementBytes(attributePair.first);
						std::span<std::byte> destination_value =
							attributePair.second->mutableElementBytes(static_cast<size_t>(finalPointIndex));
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
				else if( numPolys == 1 && polygonVertexCount > 4 )
				{
					for( const unsigned int vertex : vertices )
						result->addPolygonVertex(vertex);
				}
			}

			// Houdini polygons are clockwise; the convenience geometry expects counter-clockwise order.
			result->reverse();
			if( report )
				report->windingReversed = true;
		}
		if( report )
		{
			const Attribute::CPtr outputPositions = result->attribute("P");
			report->outputPointCount = outputPositions
				? static_cast<size_t>(outputPositions->numElements())
				: 0u;
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

		const std::vector<std::string> pointAttributeNames = geometry->attributeNames();
		for( const std::string &name : pointAttributeNames )
		{
			Attribute::Ptr sourceAttribute = geometry->attribute(name);

			// Houdini stores P as a four-component point position in this representation.
			if( name == "P" && sourceAttribute->numComponents() == 3 )
			{
				Attribute::Ptr promotedAttribute = std::make_shared<Attribute>(
					4, Attribute::ComponentType::float32);
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

		if( geometry->primitiveCount() > 0 )
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
			const int primitive_count = static_cast<int>(geometry->primitiveCount());
			const int vertices_per_primitive = static_cast<int>(geometry->verticesPerPrimitive());
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
			std::span<std::byte> destination_bytes = attribute->mutableBytes();
			if (source_bytes.size() != destination_bytes.size())
				throw std::runtime_error("HouGeoIO::exportPointAttributes byte size mismatch");
			std::memcpy(destination_bytes.data(), source_bytes.data(), source_bytes.size());
			geometry->setAttribute(name, attribute);
		}

		return exportGeometry(filename, geometry);
	}




	bool HouGeoIO::exportGeometry(
		std::ostream& output,
		HouGeoAdapter::ConstPtr geometry,
		bool binary)
	{
		if( !geometry || !output.good() || !binary )
			return false;
		json::BinaryWriter writer(output);
		ExportContext context(writer);
		std::vector<const HouGeoAdapter*> activeGeometries;
		return exportGeometryValue(context, std::move(geometry), activeGeometries)
			&& output.good();
	}

	bool HouGeoIO::exportGeometryValue(
		ExportContext& context,
		HouGeoAdapter::ConstPtr geometry,
		std::vector<const HouGeoAdapter*> &activeGeometries)
	{
		if( !geometry )
			return false;
		if( std::find(activeGeometries.begin(), activeGeometries.end(), geometry.get())
			!= activeGeometries.end() )
		{
			throw DiagnosticException(Diagnostic{
				DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"HouGeoIO cannot export a cyclic packed-geometry payload graph",
				-1,
				"primitives.packed.embedded"});
		}
		activeGeometries.push_back(geometry.get());
		struct ActiveGeometryGuard final
		{
			std::vector<const HouGeoAdapter*> &stack;
			~ActiveGeometryGuard() { stack.pop_back(); }
		} activeGeometryGuard{activeGeometries};

		json::BinaryWriter &writer = context.writer;

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
		if (const HouGeoAdapter::Topology::ConstPtr geometry_topology = geometry->topology())
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

		struct EmbeddedGeometryRecord
		{
			std::string primitiveType;
			std::string embeddedId;
			HouGeoAdapter::ConstPtr geometry;
		};
		std::vector<EmbeddedGeometryRecord> embeddedGeometryRecords;
		if( primitiveCount > 0 )
		{
			writer.jsonString( "primitives" );
			writer.jsonBeginArray();

			sint64 observedPrimitiveCount = 0;
			sint64 topologyVertexOffset = 0;
			const auto exportPrimitiveRange = [&](const auto& primitiveRange)
			{
				for( const auto &primitiveEntry : primitiveRange )
				{
					const HouGeoAdapter::Primitive::ConstPtr primitive = primitiveEntry;
					if( !primitive )
						throw std::runtime_error( "HouGeoIO::exportGeometry received a null primitive adapter" );

					const int storedPrimitiveCount = primitive->primitiveCount();
					if( storedPrimitiveCount < 0
						|| static_cast<sint64>(storedPrimitiveCount)
							> std::numeric_limits<sint64>::max() - observedPrimitiveCount )
					{
						throw std::overflow_error( "HouGeoIO::exportGeometry primitive count exceeds sint64 range" );
					}
					observedPrimitiveCount += static_cast<sint64>(storedPrimitiveCount);

					if( auto volume = std::dynamic_pointer_cast<const HouGeoAdapter::VolumePrimitive>(primitive) )
					{
						exportPrimitive(context, volume);
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto packedFragment = std::dynamic_pointer_cast<const HouGeoAdapter::PackedFragmentPrimitive>(primitive) )
					{
						if( !packedFragment->embeddedGeometry() )
							throw std::runtime_error( "HouGeoIO::exportGeometry packed fragment has no embedded payload" );
						const std::string embeddedId = "embed:houio:"
							+ std::to_string(embeddedGeometryRecords.size());
						if( !exportPrimitive(context, packedFragment, embeddedId) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a packed fragment primitive" );
						embeddedGeometryRecords.push_back(
							{"PackedFragment", embeddedId, packedFragment->embeddedGeometry()});
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto packedGeometry = std::dynamic_pointer_cast<const HouGeoAdapter::PackedGeometryPrimitive>(primitive) )
					{
						if( !packedGeometry->embeddedGeometry() )
							throw std::runtime_error( "HouGeoIO::exportGeometry packed geometry has no embedded payload" );
						const std::string embeddedId = "embed:houio:"
							+ std::to_string(embeddedGeometryRecords.size());
						if( !exportPrimitive(context, packedGeometry, embeddedId) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a packed geometry primitive" );
						embeddedGeometryRecords.push_back(
							{"PackedGeometry", embeddedId, packedGeometry->embeddedGeometry()});
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto packedDiskSequence = std::dynamic_pointer_cast<const HouGeoAdapter::PackedDiskSequencePrimitive>(primitive) )
					{
						if( !exportPrimitive(context, packedDiskSequence) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a packed disk sequence primitive" );
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto packedDisk = std::dynamic_pointer_cast<const HouGeoAdapter::PackedDiskPrimitive>(primitive) )
					{
						if( !exportPrimitive(context, packedDisk) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a packed disk primitive" );
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto sparseVdb = std::dynamic_pointer_cast<const HouGeoAdapter::SparseVdbPrimitive>(primitive) )
					{
						if( !exportPrimitive(context, sparseVdb) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a sparse Float VDB primitive" );
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto sparseInt32Vdb = std::dynamic_pointer_cast<const HouGeoAdapter::SparseInt32VdbPrimitive>(primitive) )
					{
						if( !exportPrimitive(context, sparseInt32Vdb) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a sparse Int32 VDB primitive" );
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto sparseVec3fVdb = std::dynamic_pointer_cast<const HouGeoAdapter::SparseVec3fVdbPrimitive>(primitive) )
					{
						if( !exportPrimitive(context, sparseVec3fVdb) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a sparse Vec3f VDB primitive" );
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto nativeVdb = std::dynamic_pointer_cast<const HouGeoAdapter::NativeVdbPrimitive>(primitive) )
					{
						if( !exportPrimitive(context, nativeVdb) )
							throw std::runtime_error( "HouGeoIO::exportGeometry could not serialize a native VDB primitive" );
						if( topologyVertexOffset == std::numeric_limits<sint64>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry topology offset exceeds sint64 range" );
						++topologyVertexOffset;
					}
					else if( auto polygonRun = std::dynamic_pointer_cast<const HouGeoAdapter::PolyPrimitive>(primitive) )
					{
						if( topologyVertexOffset > std::numeric_limits<int>::max() )
							throw std::overflow_error( "HouGeoIO::exportGeometry polygon topology offset exceeds int range" );
						exportPrimitive(context, polygonRun, static_cast<int>(topologyVertexOffset));
						const int polygonCount = polygonRun->polygonCount();
						if( polygonCount < 0 )
							throw std::runtime_error( "HouGeoIO::exportGeometry polygon count cannot be negative" );
						for( int polygonIndex=0;polygonIndex<polygonCount;++polygonIndex )
						{
							const int polygonVertexCount = polygonRun->polygonVertexCount(polygonIndex);
							if( polygonVertexCount < 0
								|| static_cast<sint64>(polygonVertexCount)
									> std::numeric_limits<sint64>::max() - topologyVertexOffset )
							{
								throw std::overflow_error( "HouGeoIO::exportGeometry polygon vertex total exceeds sint64 range" );
							}
							topologyVertexOffset += static_cast<sint64>(polygonVertexCount);
						}
					}
					else
					{
						throw std::runtime_error( "HouGeoIO::exportGeometry encountered an unsupported primitive adapter" );
					}
				}
			};

			const std::span<const HouGeoAdapter::Primitive::Ptr> primitiveView = geometry->primitiveView();
			if( !primitiveView.empty() )
				exportPrimitiveRange(primitiveView);
			else
			{
				const std::vector<HouGeoAdapter::Primitive::ConstPtr> copiedPrimitives = geometry->primitives();
				exportPrimitiveRange(copiedPrimitives);
			}

			if( observedPrimitiveCount != primitiveCount )
				throw std::runtime_error( "HouGeoIO::exportGeometry primitive adapters do not match primitiveCount" );
			if( topologyVertexOffset != vertexCount )
				throw std::runtime_error( "HouGeoIO::exportGeometry primitive topology does not match vertexCount" );
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

		if( !embeddedGeometryRecords.empty() )
		{
			writer.jsonString("sharedprimitivedata");
			writer.jsonBeginArray();
			for( const EmbeddedGeometryRecord &record : embeddedGeometryRecords )
			{
				writer.jsonString(record.primitiveType);
				writer.jsonBeginArray();
				writer.jsonString("gu:embeddedgeo");
				writer.jsonString(record.embeddedId);
				if( !exportGeometryValue(context, record.geometry, activeGeometries) )
					throw std::runtime_error( "HouGeoIO failed to serialize embedded packed geometry" );
				writer.jsonEndArray();
			}
			writer.jsonEndArray();
		}

		writer.jsonEndArray();
		return true;
	}






	bool HouGeoIO::exportAttribute(
		ExportContext &context,
		HouGeoAdapter::AttributeAdapter::ConstPtr attribute )
	{
		if( !attribute )
			return false;
		json::BinaryWriter &writer = context.writer;

		const HouGeoAdapter::AttributeAdapter::Type attribute_type = attribute->type();
		const HouGeoAdapter::AttributeAdapter::Storage attribute_storage = attribute->storage();
		const HouGeoAdapter::AttributeAdapter::TupleSize sourceTupleSize = attribute->tupleSize();
		const std::string name = attribute->name();
		const std::string attributeScope = attribute->scope();
		if( attributeScope.empty() )
			throw std::runtime_error( "HouGeoIO::exportAttribute: attribute scope cannot be empty for " + name );
		const std::shared_ptr<json::Object> attributeOptions = attribute->options();
		const bool promotePosition = attribute_type == HouGeoAdapter::AttributeAdapter::Type::numeric
			&& name == "P" && sourceTupleSize.value() == 3;
		const int exportTupleSize = promotePosition ? 4 : sourceTupleSize.value();
		const int elementCount = attribute->elementCount();
		if (elementCount < 0)
			throw std::runtime_error("HouGeoIO::exportAttribute: invalid element metadata for attribute " + name);
		const size_t element_count = static_cast<size_t>(elementCount);
		const size_t tuple_size = sourceTupleSize.asSize();
		if (element_count > std::numeric_limits<size_t>::max() / tuple_size)
			throw std::length_error("HouGeoIO::exportAttribute: scalar count overflow for attribute " + name);
		const size_t scalar_count = element_count * tuple_size;

		const std::optional<std::string_view> attributeTypeName =
			HouGeoAdapter::AttributeAdapter::typeName(attribute_type);
		if( !attributeTypeName )
			throw std::runtime_error( "HouGeoIO::exportAttribute: unsupported type for attribute " + name );

		const std::optional<std::string_view> storageName =
			HouGeoAdapter::AttributeAdapter::storageName(attribute_storage);
		if( attribute_type == HouGeoAdapter::AttributeAdapter::Type::numeric && !storageName )
			throw std::runtime_error( "HouGeoIO::exportAttribute: unsupported storage for attribute " + name );

		if( attribute_type == HouGeoAdapter::AttributeAdapter::Type::numeric
			&& name == "P" && exportTupleSize != 4 )
			throw std::runtime_error( "HouGeoIO::exportAttribute: P must contain either three or four components" );

		writer.jsonBeginArray();

		writer.jsonBeginArray();
		writer.jsonString( "scope" );
		writer.jsonString( attributeScope );
		writer.jsonString( "type" );
		writer.jsonString(std::string(*attributeTypeName));
		writer.jsonString( "name" );
		writer.jsonString( name );
		writer.jsonString( "options" );
		if( attributeOptions )
			writeJsonObject(writer, attributeOptions);
		else
		{
			writer.jsonBeginMap();
			writer.jsonEndMap();
		}
		writer.jsonEndArray();

		writer.jsonBeginArray();
		if( attribute_type == HouGeoAdapter::AttributeAdapter::Type::numeric )
		{
			writer.jsonString( "size" );
			writer.jsonInt( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString(std::string(*storageName));

			writer.jsonString( "values" );
			writer.jsonBeginArray();
			writer.jsonString( "size" );
			writer.jsonInt( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString(std::string(*storageName));
			writer.jsonString( "pagesize" );
			writer.jsonInt( 1024 );
			writer.jsonString( "rawpagedata" );

			const HouGeoAdapter::RawDataView raw_data =
				requireRawAttributeData(attribute, "attributes." + name);

			if( promotePosition )
			{
				if (element_count > std::numeric_limits<size_t>::max() / 4u)
					throw std::length_error("HouGeoIO::exportAttribute: promoted P size overflow");
				if( attribute_storage == HouGeoAdapter::AttributeAdapter::Storage::float16 )
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
				else if( attribute_storage == HouGeoAdapter::AttributeAdapter::Storage::float32 )
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
			else if( attribute_storage == HouGeoAdapter::AttributeAdapter::Storage::float16 )
			{
				const std::vector<uword> values = copyRawValues<uword>(raw_data, scalar_count, name);
				writer.jsonUniformArrayReal16(std::span<const uword>(values));
			}
			else if( attribute_storage == HouGeoAdapter::AttributeAdapter::Storage::float32 )
				writer.jsonUniformArray(copyRawValues<real32>(raw_data, scalar_count, name));
			else if( attribute_storage == HouGeoAdapter::AttributeAdapter::Storage::float64 )
				writer.jsonUniformArray(copyRawValues<real64>(raw_data, scalar_count, name));
			else if( attribute_storage == HouGeoAdapter::AttributeAdapter::Storage::int32 )
				writer.jsonUniformArray(copyRawValues<sint32>(raw_data, scalar_count, name));
			else if( attribute_storage == HouGeoAdapter::AttributeAdapter::Storage::int64 )
				writer.jsonUniformArray(copyRawValues<sint64>(raw_data, scalar_count, name));

			writer.jsonEndArray();
		}
		else if( attribute_type == HouGeoAdapter::AttributeAdapter::Type::string )
		{
			writer.jsonString( "size" );
			writer.jsonInt( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString( "int32" );

			std::map<std::string, sint32> stringLookup;
			std::vector<std::string> stringTable;
			std::vector<sint32> stringIndices;
			stringIndices.reserve(scalar_count);
			for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
			{
				for( int componentIndex=0;componentIndex<sourceTupleSize.value();++componentIndex )
				{
					const std::string value = attribute->stringValue(elementIndex, componentIndex);
					auto existing = stringLookup.find(value);
					if( existing == stringLookup.end() )
					{
						if( stringTable.size() > static_cast<size_t>(std::numeric_limits<sint32>::max()) )
							throw std::length_error( "HouGeoIO::exportAttribute string table exceeds int32 range for attribute "
								+ name );
						const sint32 stringIndex = static_cast<sint32>(stringTable.size());
						stringTable.push_back(value);
						existing = stringLookup.emplace(value, stringIndex).first;
					}
					stringIndices.push_back(existing->second);
				}
			}

			writer.jsonString( "strings" );
			writer.jsonBeginArray();
			for( const std::string &value : stringTable )
				writer.jsonString(value);
			writer.jsonEndArray();

			writer.jsonString( "indices" );
			writer.jsonBeginArray();
			writer.jsonString( "size" );
			writer.jsonInt32( exportTupleSize );
			writer.jsonString( "storage" );
			writer.jsonString( "int32" );
			writer.jsonString( "pagesize" );
			writer.jsonInt32( 1024 );
			writer.jsonString( "rawpagedata" );
			writer.jsonUniformArray(stringIndices);
			writer.jsonEndArray();
		}else if( attribute_type == HouGeoAdapter::AttributeAdapter::Type::dictionary )
		{
			std::vector<std::shared_ptr<json::Object>> dictionaries;
			dictionaries.reserve(static_cast<size_t>(elementCount));
			for( int elementIndex=0;elementIndex<elementCount;++elementIndex )
			{
				std::shared_ptr<json::Object> dictionary = attribute->dictionaryValue(elementIndex);
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

	bool HouGeoIO::exportTopology(
		ExportContext &context,
		HouGeoAdapter::Topology::ConstPtr topology )
	{
		if( !topology )
			return false;
		json::BinaryWriter &writer = context.writer;

		const sint64 declaredIndexCount = topology->indexCount();
		if( declaredIndexCount < 0
			|| static_cast<uint64>(declaredIndexCount) > std::numeric_limits<size_t>::max() )
		{
			throw std::length_error( "HouGeoIO::exportTopology index count exceeds addressable storage" );
		}
		const size_t expectedIndexCount = static_cast<size_t>(declaredIndexCount);
		std::vector<int> copiedIndices;
		std::span<const int> topologyIndices = topology->indexView();
		if( topologyIndices.size() != expectedIndexCount )
		{
			copiedIndices = topology->indexValues();
			if( copiedIndices.size() != expectedIndexCount )
				throw std::runtime_error( "HouGeoIO::exportTopology index data does not match indexCount" );
			topologyIndices = copiedIndices;
		}

		bool requires32BitIndices = false;
		for( const int index : topologyIndices )
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
			static_assert(std::is_same_v<int, sint32>);
			writer.jsonUniformArray<sint32>(
				std::span<const sint32>(topologyIndices.data(), topologyIndices.size()));
		}
		else
		{
			std::vector<sint16> compactIndices;
			compactIndices.reserve(topologyIndices.size());
			for( const int index : topologyIndices )
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

	bool HouGeoIO::exportPrimitive(
		ExportContext &context,
		HouGeoAdapter::VolumePrimitive::ConstPtr volume )
	{
		if( !volume )
			return false;
		json::BinaryWriter &writer = context.writer;
		const math::V3i resolution = volume->resolution();

		writer.jsonBeginArray();

		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString("Volume");
		writer.jsonEndArray();

		writer.jsonBeginArray();
		writer.jsonString("vertex");
		writer.jsonInt32(volume->topologyVertex());

		writer.jsonString("transform");
		const math::M44f volume_transform = volume->transform();
		const math::M44f translation =
			math::M44f::translationMatrix(volume_transform.translation());
		const math::M44f rotationScale = math::M44f::scaleMatrix(0.5f)
			* math::M44f::translationMatrix(1.0, 1.0, 1.0)
			* volume_transform * translation.inverted();
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
								tileValues.push_back(volume->voxelValue(voxelX, voxelY, voxelZ));
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
		writer.jsonString(volume->visualizationMode());
		writer.jsonKey("iso");
		writer.jsonReal32(volume->visualizationIso());
		writer.jsonKey("density");
		writer.jsonReal32(volume->visualizationDensity());
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
		HouGeoAdapter::PackedGeometryPrimitive::ConstPtr packedGeometry,
		const std::string &embeddedId )
	{
		if( !packedGeometry || !packedGeometry->embeddedGeometry()
			|| packedGeometry->topologyVertex() < 0 || embeddedId.empty() )
		{
			return false;
		}
		json::BinaryWriter &writer = context.writer;
		const math::V3f pivot = packedGeometry->pivot();
		const math::M33f transform = packedGeometry->transform();

		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString("PackedGeometry");
		writer.jsonEndArray();

		writer.jsonBeginArray();
		writer.jsonString("parameters");
		writer.jsonBeginMap();
		writer.jsonKey("embedded");
		writer.jsonString(embeddedId);
		writer.jsonKey("pointinstancetransform");
		writer.jsonInt32(packedGeometry->pointInstanceTransform() ? 1 : 0);
		writer.jsonKey("treatasfolder");
		writer.jsonInt32(packedGeometry->treatAsFolder() ? 1 : 0);
		writer.jsonEndMap();

		writer.jsonString("pivot");
		writer.jsonBeginArray();
		writer.jsonReal32(pivot.x);
		writer.jsonReal32(pivot.y);
		writer.jsonReal32(pivot.z);
		writer.jsonEndArray();

		writer.jsonString("transform");
		writer.jsonBeginArray();
		for( const real32 value : transform.ma )
			writer.jsonReal32(value);
		writer.jsonEndArray();

		writer.jsonString("vertex");
		writer.jsonInt32(packedGeometry->topologyVertex());
		writer.jsonString("viewportlod");
		writer.jsonString(packedGeometry->viewportLod());
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::PackedFragmentPrimitive::ConstPtr packedFragment,
		const std::string &embeddedId )
	{
		if( !packedFragment || !packedFragment->embeddedGeometry()
			|| packedFragment->topologyVertex() < 0 || embeddedId.empty()
			|| packedFragment->fragmentAttribute().empty()
			|| packedFragment->fragmentName().empty() )
		{
			return false;
		}
		json::BinaryWriter &writer = context.writer;
		const math::V3f pivot = packedFragment->pivot();
		const math::M33f transform = packedFragment->transform();
		const auto bounds = packedFragment->bounds();
		const auto cachedBounds = packedFragment->cachedBounds();

		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString("PackedFragment");
		writer.jsonEndArray();
		writer.jsonBeginArray();
		writer.jsonString("parameters");
		writer.jsonBeginMap();
		writer.jsonKey("attribute");
		writer.jsonString(packedFragment->fragmentAttribute());
		writer.jsonKey("bounds");
		writer.jsonBeginArray();
		for( const real32 value : bounds )
			writer.jsonReal32(value);
		writer.jsonEndArray();
		writer.jsonKey("cachedbounds");
		writer.jsonBeginArray();
		for( const real32 value : cachedBounds )
			writer.jsonReal32(value);
		writer.jsonEndArray();
		writer.jsonKey("embedded");
		writer.jsonString(embeddedId);
		writer.jsonKey("name");
		writer.jsonString(packedFragment->fragmentName());
		writer.jsonKey("pointinstancetransform");
		writer.jsonInt32(packedFragment->pointInstanceTransform() ? 1 : 0);
		writer.jsonEndMap();
		writer.jsonString("pivot");
		writer.jsonBeginArray();
		writer.jsonReal32(pivot.x);
		writer.jsonReal32(pivot.y);
		writer.jsonReal32(pivot.z);
		writer.jsonEndArray();
		writer.jsonString("transform");
		writer.jsonBeginArray();
		for( const real32 value : transform.ma )
			writer.jsonReal32(value);
		writer.jsonEndArray();
		writer.jsonString("vertex");
		writer.jsonInt32(packedFragment->topologyVertex());
		writer.jsonString("viewportlod");
		writer.jsonString(packedFragment->viewportLod());
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::PackedDiskPrimitive::ConstPtr packedDisk )
	{
		if( !packedDisk || packedDisk->topologyVertex() < 0
			|| packedDisk->filename().empty() )
		{
			return false;
		}
		json::BinaryWriter &writer = context.writer;
		const math::V3f pivot = packedDisk->pivot();
		const math::M33f transform = packedDisk->transform();

		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString("PackedDisk");
		writer.jsonEndArray();
		writer.jsonBeginArray();
		writer.jsonString("parameters");
		writer.jsonBeginMap();
		writer.jsonKey("expandfilename");
		writer.jsonInt32(packedDisk->expandFilename() ? 1 : 0);
		writer.jsonKey("expandframe");
		writer.jsonReal32(packedDisk->expandFrame());
		writer.jsonKey("filename");
		writer.jsonString(packedDisk->filename());
		writer.jsonKey("pointinstancetransform");
		writer.jsonInt32(packedDisk->pointInstanceTransform() ? 1 : 0);
		writer.jsonKey("treatasfolder");
		writer.jsonInt32(packedDisk->treatAsFolder() ? 1 : 0);
		writer.jsonEndMap();
		writer.jsonString("pivot");
		writer.jsonBeginArray();
		writer.jsonReal32(pivot.x);
		writer.jsonReal32(pivot.y);
		writer.jsonReal32(pivot.z);
		writer.jsonEndArray();
		writer.jsonString("transform");
		writer.jsonBeginArray();
		for( const real32 value : transform.ma )
			writer.jsonReal32(value);
		writer.jsonEndArray();
		writer.jsonString("vertex");
		writer.jsonInt32(packedDisk->topologyVertex());
		writer.jsonString("viewportlod");
		writer.jsonString(packedDisk->viewportLod());
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::PackedDiskSequencePrimitive::ConstPtr packedDiskSequence )
	{
		if( !packedDiskSequence || packedDiskSequence->topologyVertex() < 0
			|| !std::isfinite(packedDiskSequence->index()) )
		{
			return false;
		}
		const std::vector<std::string> filenames = packedDiskSequence->filenames();
		if( filenames.empty()
			|| std::any_of(filenames.begin(), filenames.end(),
				[](const std::string &filename) { return filename.empty(); }) )
		{
			return false;
		}

		const char *wrap = nullptr;
		switch( packedDiskSequence->wrapMode() )
		{
		case HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::cycle:
			wrap = "cycle";
			break;
		case HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::clamp:
			wrap = "clamp";
			break;
		case HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::strict:
			wrap = "strict";
			break;
		case HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode::mirror:
			wrap = "mirror";
			break;
		}
		if( !wrap )
			return false;

		json::BinaryWriter &writer = context.writer;
		const math::V3f pivot = packedDiskSequence->pivot();
		const math::M33f transform = packedDiskSequence->transform();

		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString("PackedDiskSequence");
		writer.jsonEndArray();
		writer.jsonBeginArray();
		writer.jsonString("parameters");
		writer.jsonBeginMap();
		writer.jsonKey("filenames");
		writer.jsonBeginArray();
		for( const std::string &filename : filenames )
			writer.jsonString(filename);
		writer.jsonEndArray();
		writer.jsonKey("index");
		writer.jsonReal32(packedDiskSequence->index());
		writer.jsonKey("pointinstancetransform");
		writer.jsonInt32(packedDiskSequence->pointInstanceTransform() ? 1 : 0);
		writer.jsonKey("wrap");
		writer.jsonString(wrap);
		writer.jsonEndMap();
		writer.jsonString("pivot");
		writer.jsonBeginArray();
		writer.jsonReal32(pivot.x);
		writer.jsonReal32(pivot.y);
		writer.jsonReal32(pivot.z);
		writer.jsonEndArray();
		writer.jsonString("transform");
		writer.jsonBeginArray();
		for( const real32 value : transform.ma )
			writer.jsonReal32(value);
		writer.jsonEndArray();
		writer.jsonString("vertex");
		writer.jsonInt32(packedDiskSequence->topologyVertex());
		writer.jsonString("viewportlod");
		writer.jsonString(packedDiskSequence->viewportLod());
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::SparseVdbPrimitive::ConstPtr sparseVdb )
	{
		if( !sparseVdb || sparseVdb->topologyVertex() < 0 )
			return false;

		const auto payload = NativeVdbPayload::encodeStream(
			[&](std::ostream& output)
			{
				return OpenVdbBackend::encodeFloatGrid(
					output, sparseVdb->sparseGrid());
			});
		if( !payload )
			throwReadFailure(payload.diagnostics,
				"HouGeoIO could not stream SparseFloatGrid into a Houdini VDB payload");

		auto nativeVdb = std::make_shared<HouGeo::HouVdb>();
		nativeVdb->setTopologyVertex(sparseVdb->topologyVertex());
		nativeVdb->setSerializedPayload(payload.value);
		return exportPrimitive(
			context,
			std::static_pointer_cast<const HouGeoAdapter::NativeVdbPrimitive>(nativeVdb));
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::SparseInt32VdbPrimitive::ConstPtr sparseVdb )
	{
		if( !sparseVdb || sparseVdb->topologyVertex() < 0 )
			return false;

		const auto payload = NativeVdbPayload::encodeStream(
			[&](std::ostream& output)
			{
				return OpenVdbBackend::encodeInt32Grid(
					output, sparseVdb->sparseGrid());
			});
		if( !payload )
			throwReadFailure(payload.diagnostics,
				"HouGeoIO could not stream SparseInt32Grid into a Houdini VDB payload");

		auto nativeVdb = std::make_shared<HouGeo::HouVdb>();
		nativeVdb->setTopologyVertex(sparseVdb->topologyVertex());
		nativeVdb->setSerializedPayload(payload.value);
		return exportPrimitive(
			context,
			std::static_pointer_cast<const HouGeoAdapter::NativeVdbPrimitive>(nativeVdb));
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::SparseVec3fVdbPrimitive::ConstPtr sparseVdb )
	{
		if( !sparseVdb || sparseVdb->topologyVertex() < 0 )
			return false;

		const auto payload = NativeVdbPayload::encodeStream(
			[&](std::ostream& output)
			{
				return OpenVdbBackend::encodeVec3fGrid(
					output, sparseVdb->sparseGrid());
			});
		if( !payload )
			throwReadFailure(payload.diagnostics,
				"HouGeoIO could not stream SparseVec3fGrid into a Houdini VDB payload");

		auto nativeVdb = std::make_shared<HouGeo::HouVdb>();
		nativeVdb->setTopologyVertex(sparseVdb->topologyVertex());
		nativeVdb->setSerializedPayload(payload.value);
		return exportPrimitive(
			context,
			std::static_pointer_cast<const HouGeoAdapter::NativeVdbPrimitive>(nativeVdb));
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::NativeVdbPrimitive::ConstPtr nativeVdb )
	{
		if( !nativeVdb || nativeVdb->topologyVertex() < 0
			|| !nativeVdb->serializedPayload() )
		{
			return false;
		}
		json::BinaryWriter &writer = context.writer;
		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString("VDB");
		writer.jsonEndArray();
		writer.jsonBeginArray();
		writer.jsonString("vertex");
		writer.jsonInt32(nativeVdb->topologyVertex());
		writer.jsonString("vdb");
		writeJsonValue(writer, json::Value::createArray(nativeVdb->serializedPayload()));
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( ExportContext &context,
		HouGeoAdapter::PolyPrimitive::ConstPtr polygonRun, int startVertex )
	{
		if( !polygonRun || startVertex < 0 || polygonRun->polygonCount() <= 0 )
			return false;
		json::BinaryWriter &writer = context.writer;

		std::vector<sint32> vertexCounts;
		vertexCounts.reserve(static_cast<size_t>(polygonRun->polygonCount()));
		for( int polygonIndex=0;polygonIndex<polygonRun->polygonCount();++polygonIndex )
		{
			const int vertexCount = polygonRun->polygonVertexCount(polygonIndex);
			if( vertexCount <= 0 )
				throw std::runtime_error( "HouGeoIO::exportPrimitive: polygon has no vertices" );
			vertexCounts.push_back(vertexCount);
		}

		writer.jsonBeginArray();
		writer.jsonBeginArray();
		writer.jsonString("type");
		writer.jsonString(polygonRun->isClosed() ? "Polygon_run" : "PolygonCurve_run");
		writer.jsonEndArray();

		writer.jsonBeginArray();
		writer.jsonString("startvertex");
		writer.jsonInt32(startVertex);
		writer.jsonString("nprimitives");
		writer.jsonInt32(polygonRun->polygonCount());
		writer.jsonString("nvertices");
		writer.jsonUniformArray(vertexCounts);
		writer.jsonEndArray();
		writer.jsonEndArray();
		return true;
	}

}










