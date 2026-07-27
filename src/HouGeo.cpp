#include <houio/HouGeo.h>

#include <cstring>
#include <limits>
#include <numeric>
#include <type_traits>




namespace houio
{
	namespace
	{
		int checkedArrayCount( const json::ArrayPtr &array, const std::string &description )
		{
			if( !array )
				throw std::runtime_error(description + " must be an array");
			const sint64 count = array->size();
			if( count < 0 )
				throw std::runtime_error(description + " has a negative element count");
			if( count > static_cast<sint64>(std::numeric_limits<int>::max()) )
				throw std::length_error(description + " exceeds supported indexing");
			return static_cast<int>(count);
		}

		std::vector<int> expandPagedIntValues(
			json::ObjectPtr values,
			sint64 elementCount,
			int tupleSize,
			const std::string& attributeName)
		{
			if( !values )
				throw std::runtime_error( "HouGeo::loadAttribute missing integer value object for attribute " + attributeName );
			if( elementCount < 0 || elementCount > static_cast<sint64>(std::numeric_limits<int>::max()) )
				throw std::length_error( "HouGeo::loadAttribute integer element count exceeds supported indexing for attribute "
					+ attributeName );
			if( tupleSize <= 0 || values->get<int>("size", 1) != tupleSize )
				throw std::runtime_error( "HouGeo::loadAttribute integer tuple size mismatch for attribute " + attributeName );

			const size_t elementCountSize = static_cast<size_t>(elementCount);
			const size_t tupleSizeValue = static_cast<size_t>(tupleSize);
			if( elementCountSize != 0 && tupleSizeValue > std::numeric_limits<size_t>::max() / elementCountSize )
				throw std::length_error( "HouGeo::loadAttribute integer tuple count overflow for attribute " + attributeName );
			const size_t scalarCount = elementCountSize * tupleSizeValue;
			if( scalarCount > static_cast<size_t>(std::numeric_limits<int>::max()) )
				throw std::length_error( "HouGeo::loadAttribute integer scalar count exceeds supported indexing for attribute "
					+ attributeName );

			std::vector<int> result(scalarCount);
			if( values->contains("arrays") )
			{
				json::ArrayPtr arrays = values->array("arrays");
				if( !arrays || arrays->size() != tupleSize )
					throw std::runtime_error( "HouGeo::loadAttribute invalid integer component arrays for attribute " + attributeName );
				for( int componentIndex=0;componentIndex<tupleSize;++componentIndex )
				{
					json::ArrayPtr componentValues = arrays->array(componentIndex);
					if( !componentValues || componentValues->size() != elementCount )
						throw std::runtime_error( "HouGeo::loadAttribute integer value count mismatch for attribute " + attributeName );
					for( int elementIndex=0;elementIndex<static_cast<int>(elementCount);++elementIndex )
					{
						const size_t destinationIndex = static_cast<size_t>(elementIndex) * tupleSizeValue
							+ static_cast<size_t>(componentIndex);
						result[destinationIndex] = componentValues->get<int>(elementIndex);
					}
				}
				return result;
			}

			if( !values->contains("rawpagedata") )
				throw std::runtime_error( "HouGeo::loadAttribute missing integer payload for attribute " + attributeName );
			json::ArrayPtr rawPageData = values->array("rawpagedata");
			if( !rawPageData )
				throw std::runtime_error( "HouGeo::loadAttribute invalid integer payload for attribute " + attributeName );

			const int elementsPerPage = values->get<int>("pagesize", 0);
			if( elementsPerPage <= 0 )
				throw std::runtime_error( "HouGeo::loadAttribute invalid page size for attribute " + attributeName );
			const size_t pageCount = elementCountSize == 0 ? 0
				: (elementCountSize + static_cast<size_t>(elementsPerPage) - 1u)
					/ static_cast<size_t>(elementsPerPage);

			std::vector<int> packing;
			if( values->contains("packing") )
			{
				json::ArrayPtr packingValues = values->array("packing");
				const int packingCount = checkedArrayCount(packingValues,
					"HouGeo::loadAttribute integer packing for attribute " + attributeName);
				if( packingCount == 0 )
					throw std::runtime_error( "HouGeo::loadAttribute integer packing cannot be empty for attribute " + attributeName );
				size_t packedTupleSize = 0;
				for( int packingIndex=0;packingIndex<packingCount;++packingIndex )
				{
					const int packSize = packingValues->get<int>(packingIndex);
					if( packSize <= 0 )
						throw std::runtime_error( "HouGeo::loadAttribute integer packing must be positive for attribute " + attributeName );
					const size_t packSizeValue = static_cast<size_t>(packSize);
					if( packedTupleSize > tupleSizeValue || packSizeValue > tupleSizeValue - packedTupleSize )
						throw std::runtime_error( "HouGeo::loadAttribute integer packing exceeds tuple size for attribute "
							+ attributeName );
					packedTupleSize += packSizeValue;
					packing.push_back(packSize);
				}
				if( packedTupleSize != tupleSizeValue )
					throw std::runtime_error( "HouGeo::loadAttribute integer packing does not cover tuple size for attribute "
						+ attributeName );
			}
			else
			{
				packing.push_back(tupleSize);
			}
			if( std::accumulate(packing.begin(), packing.end(), size_t{0}) != tupleSizeValue )
				throw std::runtime_error( "HouGeo::loadAttribute integer packing does not cover tuple size for attribute "
					+ attributeName );

			std::vector<std::vector<bool>> constantFlags(
				packing.size(), std::vector<bool>(pageCount, false));
			if( values->contains("constantpageflags") )
			{
				json::ArrayPtr flagsPerPack = values->array("constantpageflags");
				if( !flagsPerPack || flagsPerPack->size() != static_cast<sint64>(packing.size()) )
					throw std::runtime_error( "HouGeo::loadAttribute invalid constant page flags for attribute " + attributeName );
				for( size_t packIndex=0;packIndex<packing.size();++packIndex )
				{
					json::ArrayPtr pageFlags = flagsPerPack->array(static_cast<int>(packIndex));
					if( !pageFlags || pageFlags->size() != static_cast<sint64>(pageCount) )
						throw std::runtime_error( "HouGeo::loadAttribute constant page flag count mismatch for attribute "
							+ attributeName );
					for( size_t pageIndex=0;pageIndex<pageCount;++pageIndex )
						constantFlags[packIndex][pageIndex] = pageFlags->get<bool>(static_cast<int>(pageIndex));
				}
			}

			size_t expectedRawCount = 0;
			for( size_t pageIndex=0;pageIndex<pageCount;++pageIndex )
			{
				const size_t pageStart = pageIndex * static_cast<size_t>(elementsPerPage);
				const size_t pageElementCount = std::min(
					elementCountSize - pageStart, static_cast<size_t>(elementsPerPage));
				for( size_t packIndex=0;packIndex<packing.size();++packIndex )
				{
					const size_t repeatedElements = constantFlags[packIndex][pageIndex] ? 1u : pageElementCount;
					const size_t packSize = static_cast<size_t>(packing[packIndex]);
					if( repeatedElements != 0 && packSize > std::numeric_limits<size_t>::max() / repeatedElements )
						throw std::length_error( "HouGeo::loadAttribute integer page payload overflow for attribute "
							+ attributeName );
					const size_t packValueCount = repeatedElements * packSize;
					if( packValueCount > std::numeric_limits<size_t>::max() - expectedRawCount )
						throw std::length_error( "HouGeo::loadAttribute integer page payload overflow for attribute "
							+ attributeName );
					expectedRawCount += packValueCount;
				}
			}
			if( rawPageData->size() != static_cast<sint64>(expectedRawCount) )
				throw std::runtime_error( "HouGeo::loadAttribute integer page payload size mismatch for attribute "
					+ attributeName );

			size_t rawIndex = 0;
			for( size_t pageIndex=0;pageIndex<pageCount;++pageIndex )
			{
				const size_t pageStart = pageIndex * static_cast<size_t>(elementsPerPage);
				const size_t pageElementCount = std::min(
					elementCountSize - pageStart, static_cast<size_t>(elementsPerPage));
				size_t componentStart = 0;
				for( size_t packIndex=0;packIndex<packing.size();++packIndex )
				{
					const size_t packSize = static_cast<size_t>(packing[packIndex]);
					const bool constantPage = constantFlags[packIndex][pageIndex];
					for( size_t pageElement=0;pageElement<pageElementCount;++pageElement )
					{
						const size_t sourceStart = rawIndex + (constantPage ? 0u : pageElement * packSize);
						const size_t destinationStart = (pageStart + pageElement) * tupleSizeValue + componentStart;
						for( size_t component=0;component<packSize;++component )
							result[destinationStart + component] = rawPageData->get<int>(
								static_cast<int>(sourceStart + component));
					}
					rawIndex += (constantPage ? 1u : pageElementCount) * packSize;
					componentStart += packSize;
				}
			}
			if( rawIndex != expectedRawCount )
				throw std::runtime_error( "HouGeo::loadAttribute integer page expansion mismatch for attribute "
					+ attributeName );
			return result;
		}

		template<typename Function>
		void withSchemaPath( const std::string &path, Function &&function )
		{
			try
			{
				function();
			}
			catch( const DiagnosticException &exception )
			{
				throw DiagnosticException(withDiagnosticPath(exception.diagnostic(), path));
			}
			catch( const std::exception &exception )
			{
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					exception.what(), -1, path});
			}
		}

		size_t checkedProduct( size_t left, size_t right, const std::string &description )
		{
			if( left != 0 && right > std::numeric_limits<size_t>::max() / left )
				throw std::length_error(description + " exceeds addressable storage");
			return left * right;
		}

		size_t volumeVoxelCount( const math::V3i &resolution )
		{
			if( resolution.x <= 0 || resolution.y <= 0 || resolution.z <= 0 )
				throw std::invalid_argument( "Volume resolution dimensions must be positive" );
			const size_t xy = checkedProduct(static_cast<size_t>(resolution.x), static_cast<size_t>(resolution.y), "Volume resolution");
			return checkedProduct(xy, static_cast<size_t>(resolution.z), "Volume resolution");
		}

		size_t volumeIndex( int x, int y, int z, const math::V3i &resolution )
		{
			const size_t planeSize = checkedProduct(static_cast<size_t>(resolution.x), static_cast<size_t>(resolution.y), "Volume plane");
			return static_cast<size_t>(z) * planeSize + static_cast<size_t>(y) * static_cast<size_t>(resolution.x)
				+ static_cast<size_t>(x);
		}

		template<typename T>
		void storeNumericValue(std::span<std::byte> data, size_t destination_index, const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			if (destination_index > std::numeric_limits<size_t>::max() / sizeof(T))
				throw std::out_of_range("Numeric attribute destination index overflow");
			const size_t byte_offset = destination_index * sizeof(T);
			if (byte_offset > data.size() || sizeof(T) > data.size() - byte_offset)
				throw std::out_of_range("Numeric attribute destination index is outside storage");
			std::memcpy(data.data() + byte_offset, &value, sizeof(T));
		}

		Attribute::ComponentType componentTypeForStorage(
			HouGeoAdapter::AttributeAdapter::Storage storage) noexcept
		{
			using Storage = HouGeoAdapter::AttributeAdapter::Storage;
			switch (storage)
			{
			case Storage::float16:
				return Attribute::ComponentType::float16;
			case Storage::float32:
				return Attribute::ComponentType::float32;
			case Storage::float64:
				return Attribute::ComponentType::float64;
			case Storage::int32:
				return Attribute::ComponentType::int32;
			case Storage::int64:
				return Attribute::ComponentType::int64;
			case Storage::invalid:
				return Attribute::ComponentType::invalid;
			}
			return Attribute::ComponentType::invalid;
		}

		void storeNumericComponent(
			std::span<std::byte> data,
			size_t destination_index,
			HouGeoAdapter::AttributeAdapter::Storage storage,
			const json::Value& value)
		{
			switch( storage )
			{
			case HouGeoAdapter::AttributeAdapter::Storage::float16:
				storeNumericValue(data, destination_index, floatToHalfBits(value.as<real32>()));
				break;
			case HouGeoAdapter::AttributeAdapter::Storage::float32:
				storeNumericValue(data, destination_index, value.as<real32>());
				break;
			case HouGeoAdapter::AttributeAdapter::Storage::float64:
				storeNumericValue(data, destination_index, value.as<real64>());
				break;
			case HouGeoAdapter::AttributeAdapter::Storage::int32:
				storeNumericValue(data, destination_index, value.as<sint32>());
				break;
			case HouGeoAdapter::AttributeAdapter::Storage::int64:
				storeNumericValue(data, destination_index, value.as<sint64>());
				break;
			default:
				throw std::runtime_error( "Unsupported numeric attribute storage" );
			}
		}
	}

	HouGeo::HouGeo() :
		HouGeoAdapter()
	{
	}

	sint64 HouGeo::pointCount() const
	{
		if (m_pointCount >= 0)
			return m_pointCount;
		for (const auto& [name, attribute] : m_pointAttributes)
		{
			static_cast<void>(name);
			if (attribute)
				return attribute->elementCount();
		}
		return 0;
	}

	sint64 HouGeo::vertexCount() const
	{
		return m_topology ? m_topology->indexCount() : 0;
	}

	sint64 HouGeo::primitiveCount() const
	{
		sint64 primitive_count = 0;
		for (const auto& primitive : m_primitives)
		{
			if (!primitive)
				throw std::runtime_error("HouGeo contains a null primitive");
			const sint64 stored_count = primitive->primitiveCount();
			if (stored_count < 0
				|| stored_count > std::numeric_limits<sint64>::max() - primitive_count)
				throw std::overflow_error("HouGeo primitive count exceeds sint64 range");
			primitive_count += stored_count;
		}
		return primitive_count;
	}

	std::vector<std::string> HouGeo::pointAttributeNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_pointAttributes.size());
		for (const auto& [name, attribute] : m_pointAttributes)
			names.push_back(attribute ? attribute->name() : name);
		return names;
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::pointAttribute(const std::string& name)
	{
		const auto iterator = m_pointAttributes.find(name);
		return iterator != m_pointAttributes.end() ? iterator->second : nullptr;
	}

	HouGeoAdapter::AttributeAdapter::ConstPtr HouGeo::pointAttribute(
		const std::string& name) const
	{
		const auto iterator = m_pointAttributes.find(name);
		return iterator != m_pointAttributes.end() ? iterator->second : nullptr;
	}

	std::vector<std::string> HouGeo::vertexAttributeNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_vertexAttributes.size());
		for (const auto& [name, attribute] : m_vertexAttributes)
			names.push_back(attribute ? attribute->name() : name);
		return names;
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::vertexAttribute(const std::string& name)
	{
		const auto iterator = m_vertexAttributes.find(name);
		return iterator != m_vertexAttributes.end() ? iterator->second : nullptr;
	}

	HouGeoAdapter::AttributeAdapter::ConstPtr HouGeo::vertexAttribute(
		const std::string& name) const
	{
		const auto iterator = m_vertexAttributes.find(name);
		return iterator != m_vertexAttributes.end() ? iterator->second : nullptr;
	}

	std::vector<std::string> HouGeo::globalAttributeNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_globalAttributes.size());
		for (const auto& [name, attribute] : m_globalAttributes)
			names.push_back(attribute ? attribute->name() : name);
		return names;
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::globalAttribute(const std::string& name)
	{
		const auto iterator = m_globalAttributes.find(name);
		return iterator != m_globalAttributes.end() ? iterator->second : nullptr;
	}

	HouGeoAdapter::AttributeAdapter::ConstPtr HouGeo::globalAttribute(
		const std::string& name) const
	{
		const auto iterator = m_globalAttributes.find(name);
		return iterator != m_globalAttributes.end() ? iterator->second : nullptr;
	}

	std::vector<std::string> HouGeo::pointGroupNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_pointGroups.size());
		for (const auto& [name, membership] : m_pointGroups)
		{
			static_cast<void>(membership);
			names.push_back(name);
		}
		return names;
	}

	std::optional<std::vector<bool>> HouGeo::pointGroupMembership(const std::string& name) const
	{
		const auto iterator = m_pointGroups.find(name);
		if (iterator == m_pointGroups.end())
			return std::nullopt;
		return iterator->second;
	}

	std::vector<std::string> HouGeo::vertexGroupNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_vertexGroups.size());
		for (const auto& [name, membership] : m_vertexGroups)
		{
			static_cast<void>(membership);
			names.push_back(name);
		}
		return names;
	}

	std::optional<std::vector<bool>> HouGeo::vertexGroupMembership(const std::string& name) const
	{
		const auto iterator = m_vertexGroups.find(name);
		if (iterator == m_vertexGroups.end())
			return std::nullopt;
		return iterator->second;
	}

	std::vector<std::string> HouGeo::primitiveGroupNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_primitiveGroups.size());
		for (const auto& [name, membership] : m_primitiveGroups)
		{
			static_cast<void>(membership);
			names.push_back(name);
		}
		return names;
	}

	std::optional<std::vector<bool>> HouGeo::primitiveGroupMembership(const std::string& name) const
	{
		const auto iterator = m_primitiveGroups.find(name);
		if (iterator == m_primitiveGroups.end())
			return std::nullopt;
		return iterator->second;
	}

	bool HouGeo::hasPrimitiveAttribute(const std::string& name) const
	{
		return m_primitiveAttributes.contains(name);
	}

	std::vector<std::string> HouGeo::primitiveAttributeNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_primitiveAttributes.size());
		for (const auto& [name, attribute] : m_primitiveAttributes)
			names.push_back(attribute ? attribute->name() : name);
		return names;
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::primitiveAttribute(const std::string& name)
	{
		const auto iterator = m_primitiveAttributes.find(name);
		return iterator != m_primitiveAttributes.end() ? iterator->second : nullptr;
	}

	HouGeoAdapter::AttributeAdapter::ConstPtr HouGeo::primitiveAttribute(
		const std::string& name) const
	{
		const auto iterator = m_primitiveAttributes.find(name);
		return iterator != m_primitiveAttributes.end() ? iterator->second : nullptr;
	}

	std::vector<HouGeoAdapter::Primitive::Ptr> HouGeo::primitives()
	{
		return m_primitives;
	}

	std::vector<HouGeoAdapter::Primitive::ConstPtr> HouGeo::primitives() const
	{
		return {m_primitives.begin(), m_primitives.end()};
	}

	std::span<const HouGeoAdapter::Primitive::Ptr> HouGeo::primitiveView() const noexcept
	{
		return m_primitives;
	}

	HouGeo::Topology::Ptr HouGeo::topology()
	{
		return m_topology;
	}

	HouGeo::Topology::ConstPtr HouGeo::topology() const
	{
		return m_topology;
	}


	HouGeo::Ptr HouGeo::create()
	{
		return std::make_shared<HouGeo>();
	}

	void HouGeo::addPrimitive( ScalarField::Ptr field )
	{
		if( !field )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null field" );

		// Houdini encodes the volume translation through a topology vertex referencing P.
		const auto position_iterator = m_pointAttributes.find("P");
		HouAttribute::Ptr positionAttribute = position_iterator != m_pointAttributes.end()
			? position_iterator->second
			: nullptr;
		if( !positionAttribute )
		{
			positionAttribute = std::make_shared<HouAttribute>();
			positionAttribute->name_ = "P";
			positionAttribute->tuple_size_ = HouAttribute::TupleSize(4);
			positionAttribute->storage_ = HouAttribute::Storage::float32;
			positionAttribute->type_ = HouAttribute::Type::numeric;
			positionAttribute->numeric_attribute_ = Attribute::createV4f();
			setPointAttribute( positionAttribute );
		}

		const math::V3f center = field->localToWorld( math::V3f(0.5f) );
		const int pointIndex = positionAttribute->numeric_attribute_->appendElement<math::V4f>(
			math::V4f(center.x, center.y, center.z, 1.0f));
		if( m_pointCount >= 0 )
			++m_pointCount;

		if( !m_topology )
			m_topology = std::make_shared<HouTopology>();

		HouVolume::Ptr volumePrimitive = std::make_shared<HouVolume>();
		volumePrimitive->scalar_field_ = field;

		std::vector<int> pointIndices{pointIndex};
		const sint64 topologyVertex = m_topology->indexCount();
		if( topologyVertex > static_cast<sint64>(std::numeric_limits<int>::max()) )
			throw std::overflow_error( "HouGeo volume topology index exceeds int range" );
		volumePrimitive->topology_vertex_ = static_cast<int>(topologyVertex);
		m_topology->appendIndices(pointIndices);

		m_primitives.push_back( volumePrimitive );
	}

	void HouGeo::addPrimitive( VolumePrimitive::Ptr volume )
	{
		if( !volume )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null volume" );
		m_primitives.push_back(std::move(volume));
	}

	void HouGeo::addPrimitive( PackedGeometryPrimitive::Ptr packedGeometry )
	{
		if( !packedGeometry )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null packed geometry" );
		m_primitives.push_back(std::move(packedGeometry));
	}

	void HouGeo::addPrimitive( PackedFragmentPrimitive::Ptr packedFragment )
	{
		if( !packedFragment )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null packed fragment" );
		m_primitives.push_back(std::move(packedFragment));
	}

	void HouGeo::addPrimitive( PackedDiskPrimitive::Ptr packedDisk )
	{
		if( !packedDisk )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null packed disk" );
		m_primitives.push_back(std::move(packedDisk));
	}

	void HouGeo::addPrimitive( PackedDiskSequencePrimitive::Ptr packedDiskSequence )
	{
		if( !packedDiskSequence )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null packed disk sequence" );
		m_primitives.push_back(std::move(packedDiskSequence));
	}

	void HouGeo::addPrimitive( SparseVdbPrimitive::Ptr sparseVdb )
	{
		if( !sparseVdb )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null sparse VDB" );
		m_primitives.push_back(std::move(sparseVdb));
	}

	void HouGeo::addPrimitive( NativeVdbPrimitive::Ptr nativeVdb )
	{
		if( !nativeVdb )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null native VDB" );
		m_primitives.push_back(std::move(nativeVdb));
	}

	void HouGeo::addPrimitive( PolyPrimitive::Ptr poly )
	{
		if( !poly )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null polygon" );
		m_primitives.push_back(std::move(poly));
	}

	void HouGeo::setTopology( HouTopology::Ptr topo )
	{
		if( !topo )
			throw std::invalid_argument( "HouGeo::setTopology received null topology" );
		m_topology = topo;
	}


	void HouGeo::setPointAttribute( HouAttribute::Ptr attr )
	{
		if( !attr )
			throw std::invalid_argument( "HouGeo::setPointAttribute received a null attribute" );
		if( attr->name().empty() )
			throw std::invalid_argument( "HouGeo::setPointAttribute requires a non-empty name" );
		if( m_pointCount >= 0 && attr->elementCount() != m_pointCount )
			throw std::invalid_argument( "HouGeo::setPointAttribute element count does not match pointcount" );
		m_pointAttributes[attr->name()] = attr;
	}

	void HouGeo::setVertexAttribute( HouAttribute::Ptr attr )
	{
		if( !attr )
			throw std::invalid_argument( "HouGeo::setVertexAttribute received a null attribute" );
		if( attr->name().empty() )
			throw std::invalid_argument( "HouGeo::setVertexAttribute requires a non-empty name" );
		if( attr->elementCount() != vertexCount() )
			throw std::invalid_argument( "HouGeo::setVertexAttribute element count does not match vertexcount" );
		m_vertexAttributes[attr->name()] = std::move(attr);
	}

	void HouGeo::setPrimitiveAttribute( const std::string &name, HouAttribute::Ptr attr )
	{
		if( !attr )
			throw std::invalid_argument( "HouGeo::setPrimitiveAttribute received a null attribute" );
		if( name.empty() )
			throw std::invalid_argument( "HouGeo::setPrimitiveAttribute requires a non-empty name" );
		if( attr->elementCount() != primitiveCount() )
			throw std::invalid_argument( "HouGeo::setPrimitiveAttribute element count does not match primitivecount" );
		attr->name_ = name;
		m_primitiveAttributes[name] = std::move(attr);
	}

	void HouGeo::setGlobalAttribute( HouAttribute::Ptr attr )
	{
		if( !attr )
			throw std::invalid_argument( "HouGeo::setGlobalAttribute received a null attribute" );
		if( attr->name().empty() )
			throw std::invalid_argument( "HouGeo::setGlobalAttribute requires a non-empty name" );
		m_globalAttributes[attr->name()] = std::move(attr);
	}

	void HouGeo::setPointGroup( const std::string &name, const std::vector<bool> &membership )
	{
		if( name.empty() )
			throw std::invalid_argument( "HouGeo::setPointGroup requires a non-empty name" );
		const sint64 elementCount = pointCount();
		if( elementCount < 0 || static_cast<uint64>(elementCount) != membership.size() )
			throw std::invalid_argument( "HouGeo::setPointGroup membership count does not match pointcount" );
		m_pointGroups[name] = membership;
	}

	void HouGeo::setVertexGroup( const std::string &name, const std::vector<bool> &membership )
	{
		if( name.empty() )
			throw std::invalid_argument( "HouGeo::setVertexGroup requires a non-empty name" );
		const sint64 elementCount = vertexCount();
		if( elementCount < 0 || static_cast<uint64>(elementCount) != membership.size() )
			throw std::invalid_argument( "HouGeo::setVertexGroup membership count does not match vertexcount" );
		m_vertexGroups[name] = membership;
	}

	void HouGeo::setPrimitiveGroup( const std::string &name, const std::vector<bool> &membership )
	{
		if( name.empty() )
			throw std::invalid_argument( "HouGeo::setPrimitiveGroup requires a non-empty name" );
		const sint64 elementCount = primitiveCount();
		if( elementCount < 0 || static_cast<uint64>(elementCount) != membership.size() )
			throw std::invalid_argument( "HouGeo::setPrimitiveGroup membership count does not match primitivecount" );
		m_primitiveGroups[name] = membership;
	}

	// Attribute ==============================

	HouGeo::HouAttribute::HouAttribute()
		: name_("unnamed"),
		  options_(json::Object::create())
	{
	}

	HouGeo::HouAttribute::HouAttribute(const std::string& name, Attribute::Ptr attribute)
		: name_(name),
		  options_(json::Object::create()),
		  tuple_size_(TupleSize(attribute ? attribute->numComponents() : 1)),
		  storage_(Storage::float32),
		  type_(Type::numeric),
		  element_count_(attribute ? attribute->numElements() : 0),
		  numeric_attribute_(std::move(attribute))
	{
		if (!numeric_attribute_)
			throw std::invalid_argument("HouAttribute numeric storage cannot be null");
		switch (numeric_attribute_->elementComponentType())
		{
		case Attribute::ComponentType::float32:
			storage_ = Storage::float32;
			break;
		case Attribute::ComponentType::float64:
			storage_ = Storage::float64;
			break;
		case Attribute::ComponentType::float16:
			storage_ = Storage::float16;
			break;
		case Attribute::ComponentType::int32:
			storage_ = Storage::int32;
			break;
		case Attribute::ComponentType::int64:
			storage_ = Storage::int64;
			break;
		default:
			throw std::runtime_error("HouAttribute received unsupported numeric storage");
		}
	}

	std::string HouGeo::HouAttribute::name() const
	{
		return name_;
	}

	std::string HouGeo::HouAttribute::scope() const
	{
		return scope_;
	}

	json::ObjectPtr HouGeo::HouAttribute::options() const
	{
		return options_;
	}

	HouGeoAdapter::AttributeAdapter::Type HouGeo::HouAttribute::type() const
	{
		return type_;
	}

	HouGeoAdapter::AttributeAdapter::TupleSize HouGeo::HouAttribute::tupleSize() const
	{
		return numeric_attribute_ ? TupleSize(numeric_attribute_->numComponents()) : tuple_size_;
	}

	HouGeoAdapter::AttributeAdapter::Storage HouGeo::HouAttribute::storage() const
	{
		return storage_;
	}

	std::vector<int> HouGeo::HouAttribute::packing() const
	{
		return {};
	}

	HouGeoAdapter::RawDataView HouGeo::HouAttribute::rawData() const
	{
		return numeric_attribute_
			? HouGeoAdapter::RawDataView(numeric_attribute_->bytes())
			: HouGeoAdapter::RawDataView{};
	}

	int HouGeo::HouAttribute::elementCount() const
	{
		return numeric_attribute_ ? numeric_attribute_->numElements() : element_count_;
	}

	int HouGeo::HouAttribute::addString(const std::string& value)
	{
		string_values_.push_back(value);
		type_ = Type::string;
		storage_ = Storage::int32;
		tuple_size_ = TupleSize(1);
		return element_count_++;
	}

	std::string HouGeo::HouAttribute::stringValue(int index) const
	{
		return stringValue(index, 0);
	}

	std::string HouGeo::HouAttribute::stringValue(int element_index, int component_index) const
	{
		if (element_index < 0 || element_index >= element_count_)
			throw std::out_of_range("HouAttribute string element index is out of range");
		if (component_index < 0 || component_index >= tuple_size_.value())
			throw std::out_of_range("HouAttribute string component index is out of range");
		const size_t flattened_index = static_cast<size_t>(element_index) * tuple_size_.asSize()
			+ static_cast<size_t>(component_index);
		if (flattened_index >= string_values_.size())
			throw std::out_of_range("HouAttribute string tuple storage is inconsistent");
		return string_values_[flattened_index];
	}

	std::shared_ptr<json::Object> HouGeo::HouAttribute::dictionaryValue(int index) const
	{
		if (index < 0 || static_cast<size_t>(index) >= dictionary_values_.size())
			throw std::out_of_range("HouAttribute dictionary index is out of range");
		return dictionary_values_[static_cast<size_t>(index)];
	}

	// Topology ==============================

	HouGeo::HouTopology::HouTopology() = default;

	std::vector<int> HouGeo::HouTopology::indexValues() const
	{
		return indexBuffer;
	}

	std::span<const int> HouGeo::HouTopology::indexView() const noexcept
	{
		return indexBuffer;
	}

	void HouGeo::HouTopology::appendIndices(std::span<const int> indices)
	{
		indexBuffer.insert(indexBuffer.end(), indices.begin(), indices.end());
	}

	sint64 HouGeo::HouTopology::indexCount() const
	{
		if (indexBuffer.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()))
			throw std::overflow_error("HouTopology index count exceeds sint64 range");
		return static_cast<sint64>(indexBuffer.size());
	}




	void HouGeo::load( json::ObjectPtr rootObject )
	{
		if( !rootObject )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::load received a null root object", -1, "root"});

		SharedPrimitiveData sharedPrimitiveData;

		sint64 vertexCount = 0;
		sint64 pointCount = 0;
		sint64 primitiveCount = 0;
		if( rootObject->contains("pointcount") )
			pointCount = rootObject->get<int>("pointcount", 0);
		if( rootObject->contains("vertexcount") )
			vertexCount = rootObject->get<int>("vertexcount", 0);
		if( rootObject->contains("primitivecount") )
			primitiveCount = rootObject->get<int>("primitivecount", 0);
		if( pointCount < 0 || vertexCount < 0 || primitiveCount < 0 )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::load received negative geometry counts", -1, "counts"});
		m_pointCount = pointCount;
		if( rootObject->contains("attributes") )
		{
			json::ArrayPtr attributeValues = rootObject->array("attributes");
			if( !attributeValues )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeo::load attributes must be a flattened array", -1, "attributes"});
			json::ObjectPtr attributes = toObject(attributeValues);
			if( attributes->contains("pointattributes") )
			{
				json::ArrayPtr pointAttributes = attributes->array("pointattributes");
				if( !pointAttributes )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
						"HouGeo::load pointattributes must be an array", -1, "attributes.pointattributes"});
				const int pointAttributeCount = checkedArrayCount(pointAttributes,
					"HouGeo::load pointattributes");
				for( int attributeIndex=0;attributeIndex<pointAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.pointattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(pointAttributes->array(attributeIndex), pointCount);
					});
					m_pointAttributes.emplace(attribute->name(), attribute);
				}
			}
			if( attributes->contains("vertexattributes") )
			{
				json::ArrayPtr vertexAttributes = attributes->array("vertexattributes");
				if( !vertexAttributes )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
						"HouGeo::load vertexattributes must be an array", -1, "attributes.vertexattributes"});
				const int vertexAttributeCount = checkedArrayCount(vertexAttributes,
					"HouGeo::load vertexattributes");
				for( int attributeIndex=0;attributeIndex<vertexAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.vertexattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(vertexAttributes->array(attributeIndex), vertexCount);
					});
					m_vertexAttributes.emplace(attribute->name(), attribute);
				}
			}
			if( attributes->contains("primitiveattributes") )
			{
				json::ArrayPtr primitiveAttributes = attributes->array("primitiveattributes");
				if( !primitiveAttributes )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
						"HouGeo::load primitiveattributes must be an array", -1, "attributes.primitiveattributes"});
				const int primitiveAttributeCount = checkedArrayCount(primitiveAttributes,
					"HouGeo::load primitiveattributes");
				for( int attributeIndex=0;attributeIndex<primitiveAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.primitiveattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(primitiveAttributes->array(attributeIndex), primitiveCount);
					});
					m_primitiveAttributes.emplace(attribute->name(), attribute);
				}
			}
			if( attributes->contains("globalattributes") )
			{
				json::ArrayPtr globalAttributes = attributes->array("globalattributes");
				if( !globalAttributes )
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
						"HouGeo::load globalattributes must be an array", -1, "attributes.globalattributes"});
				const int globalAttributeCount = checkedArrayCount(globalAttributes,
					"HouGeo::load globalattributes");
				for( int attributeIndex=0;attributeIndex<globalAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.globalattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(globalAttributes->array(attributeIndex), 1);
					});
					m_globalAttributes.emplace(attribute->name(), attribute);
				}
			}
		}
		if( rootObject->contains("topology") )
		{
			withSchemaPath("topology", [&]()
			{
				loadTopology(toObject(rootObject->array("topology")), pointCount);
				if( m_topology->indexCount() != vertexCount )
					throw std::runtime_error( "HouGeo::load topology count does not match vertexcount" );
			});
		}
		else if( vertexCount != 0 )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::load missing topology for non-empty vertex domain", -1, "topology"});
		}
		if( rootObject->contains("sharedprimitivedata") )
		{
			json::ArrayPtr entries = rootObject->array("sharedprimitivedata");
			if( !entries )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeo::load sharedprimitivedata must be an array", -1, "sharedprimitivedata"});
			const int entryValueCount = checkedArrayCount(entries,
				"HouGeo::load sharedprimitivedata");
			if( (entryValueCount % 2) != 0 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeo::load sharedprimitivedata requires type/value pairs", -1, "sharedprimitivedata"});

			const int entryCount = entryValueCount / 2;
			for( int entryIndex=0;entryIndex<entryCount;++entryIndex )
			{
				withSchemaPath("sharedprimitivedata[" + std::to_string(entryIndex) + "]", [&]()
				{
					const int recordOffset = entryIndex * 2;
					const std::string entryType = entries->get<std::string>(recordOffset);
					json::ArrayPtr entry = entries->array(recordOffset + 1);
					if( !entry || entry->size() < 3 )
						throw std::runtime_error( "HouGeo::load shared primitive entry requires type, id, and data" );

					const std::string dataType = entry->get<std::string>(0);
					const std::string dataId = entry->get<std::string>(1);
					json::ArrayPtr sharedData = entry->array(2);
					if( !sharedData )
						throw std::runtime_error( "HouGeo::load shared primitive data must be an array" );

					if( (entryType == "PackedGeometry" || entryType == "PackedFragment")
						&& dataType == "gu:embeddedgeo" )
					{
						HouGeo::Ptr embeddedGeometry = HouGeo::create();
						embeddedGeometry->load(toObject(sharedData));
						sharedPrimitiveData.sharedEmbeddedGeometry[dataId] = std::move(embeddedGeometry);
					}
					else
					{
						sharedPrimitiveData.sharedVoxelData[dataId] = toObject(sharedData);
					}
				});
			}
		}
		if( rootObject->contains("primitives") )
		{
			json::ArrayPtr primitives = rootObject->array("primitives");
			if( !primitives )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeo::load primitives must be an array", -1, "primitives"});
			const int primitiveRecordCount = checkedArrayCount(primitives,
				"HouGeo::load primitives");
			for( int primitiveIndex=0;primitiveIndex<primitiveRecordCount;++primitiveIndex )
			{
				withSchemaPath("primitives[" + std::to_string(primitiveIndex) + "]", [&]()
				{
					loadPrimitive(primitives->array(primitiveIndex), sharedPrimitiveData);
				});
			}
		}
		if( this->primitiveCount() != primitiveCount )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::load primitive records do not match primitivecount", -1, "primitives"});
		if( rootObject->contains("pointgroups") )
			withSchemaPath("pointgroups", [&]() { loadGroups(rootObject->array("pointgroups"), pointCount, m_pointGroups); });
		if( rootObject->contains("vertexgroups") )
			withSchemaPath("vertexgroups", [&]() { loadGroups(rootObject->array("vertexgroups"), vertexCount, m_vertexGroups); });
		if( rootObject->contains("primitivegroups") )
			withSchemaPath("primitivegroups", [&]() { loadGroups(rootObject->array("primitivegroups"), primitiveCount, m_primitiveGroups); });
	}


	void HouGeo::loadGroups( json::ArrayPtr groups, sint64 elementCount,
		std::map<std::string, std::vector<bool>> &destination )
	{
		if( !groups || elementCount < 0 )
			throw std::runtime_error( "HouGeo::loadGroups received invalid group data" );
		if( elementCount > static_cast<sint64>(std::numeric_limits<int>::max()) )
			throw std::length_error( "HouGeo::loadGroups element count exceeds supported indexing" );

		const int groupCount = checkedArrayCount(groups, "HouGeo::loadGroups group list");
		for( int groupIndex=0;groupIndex<groupCount;++groupIndex )
		{
			json::ArrayPtr group = groups->array(groupIndex);
			if( !group || group->size() != 2 )
				throw std::runtime_error( "HouGeo::loadGroups expected definition and data arrays" );

			json::ObjectPtr definition = toObject(group->array(0));
			json::ObjectPtr data = toObject(group->array(1));
			const std::string name = definition->get<std::string>("name", "");
			if( name.empty() )
				throw std::runtime_error( "HouGeo::loadGroups encountered a group without a name" );
			if( destination.find(name) != destination.end() )
				throw std::runtime_error( "HouGeo::loadGroups encountered duplicate group " + name );
			if( !data->contains("selection") )
				throw std::runtime_error( "HouGeo::loadGroups missing selection for group " + name );

			json::ArrayPtr selection = data->array("selection");
			if( !selection || selection->size() != 2 || !selection->value(0).isString() )
				throw std::runtime_error( "HouGeo::loadGroups received malformed selection data for group " + name );
			if( selection->get<std::string>(0) != "unordered" )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeo::loadGroups supports only unordered selections for group " + name, -1, "selection"});

			json::ArrayPtr encodedMembership = selection->array(1);
			if( !encodedMembership || encodedMembership->size() != 2
				|| !encodedMembership->value(0).isString() )
				throw std::runtime_error( "HouGeo::loadGroups received malformed membership data for group " + name );
			if( encodedMembership->get<std::string>(0) != "i8" )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeo::loadGroups requires i8 membership encoding for group " + name, -1, "selection.encoding"});

			json::ArrayPtr membershipValues = encodedMembership->array(1);
			if( !membershipValues || membershipValues->size() != elementCount )
				throw std::runtime_error( "HouGeo::loadGroups membership count mismatch for group " + name );

			std::vector<bool> membership(static_cast<size_t>(elementCount), false);
			const int membershipCount = static_cast<int>(elementCount);
			for( int elementIndex=0;elementIndex<membershipCount;++elementIndex )
			{
				const int value = membershipValues->get<int>(elementIndex);
				if( value != 0 && value != 1 )
					throw std::runtime_error( "HouGeo::loadGroups membership must contain only zero or one for group " + name );
				membership[static_cast<size_t>(elementIndex)] = value != 0;
			}
			destination.emplace(name, std::move(membership));
		}
	}

	HouGeo::HouAttribute::Ptr HouGeo::loadAttribute( json::ArrayPtr attribute, sint64 elementCount )
	{
		if( !attribute || attribute->size() != 2 )
			throw std::runtime_error( "HouGeo::loadAttribute expected definition and data arrays" );
		if( elementCount < 0 || elementCount > static_cast<sint64>(std::numeric_limits<int>::max()) )
			throw std::length_error( "HouGeo::loadAttribute element count exceeds supported indexing" );

		json::ObjectPtr attrDef = toObject(attribute->array(0));
		json::ObjectPtr attrData = toObject(attribute->array(1));

		HouGeo::HouAttribute::Ptr attr = std::make_shared<HouGeo::HouAttribute>();

		std::string attrName = attrDef->get<std::string>("name");
		attr->name_ = attrName;
		attr->scope_ = attrDef->get<std::string>("scope", "public");
		if( attr->scope_.empty() )
			throw std::runtime_error( "HouGeo::loadAttribute scope cannot be empty for attribute " + attrName );
		if( attrDef->contains("options") )
		{
			attr->options_ = attrDef->object("options");
			if( !attr->options_ )
				throw std::runtime_error( "HouGeo::loadAttribute options must be an object for attribute " + attrName );
		}
		else
		{
			attr->options_ = json::Object::create();
		}
		AttributeAdapter::Type attrType = AttributeAdapter::parseType(
			attrDef->get<std::string>("type"));

		if( attrType == AttributeAdapter::Type::numeric )
		{
			const std::string storageName = attrData->get<std::string>("storage");
			const AttributeAdapter::Storage attrStorage = AttributeAdapter::parseStorage(storageName);
			const std::optional<size_t> componentByteWidth = AttributeAdapter::storageByteWidth(attrStorage);
			const Attribute::ComponentType attrComponentType = componentTypeForStorage(attrStorage);
			if( !componentByteWidth || attrComponentType == Attribute::ComponentType::invalid )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeo::loadAttribute does not support storage " + storageName, -1, "storage"});

			const AttributeAdapter::TupleSize tupleSize(attrData->get<int>("size"));
			const int attrTupleSize = tupleSize.value();
			attr->numeric_attribute_ = std::make_shared<Attribute>(attrTupleSize, attrComponentType);
			attr->numeric_attribute_->resize(elementCount);
			std::span<std::byte> data = attr->numeric_attribute_->mutableBytes();

			const int dstTupleSize = attrTupleSize;
			attr->name_ = attrName;
			attr->type_ = attrType;
			attr->storage_ = attrStorage;
			attr->tuple_size_ = tupleSize;
			attr->element_count_ = static_cast<int>(elementCount);

			if( attrData->contains("values") )
			{
				json::ObjectPtr values = toObject( attrData->array("values") );
				if( values->contains("tuples") )
				{
					json::ArrayPtr tuples = values->array("tuples");
					if( !tuples || tuples->size() != elementCount )
						throw std::runtime_error( "HouGeo::loadAttribute tuple count mismatch for attribute " + attrName );

					for( sint64 elementIndex=0;elementIndex<elementCount;++elementIndex )
					{
						json::ArrayPtr tuple = tuples->array(static_cast<int>(elementIndex));
						if( !tuple || tuple->size() != attrTupleSize )
							throw std::runtime_error( "HouGeo::loadAttribute tuple size mismatch for attribute " + attrName );

						for( int componentIndex=0;componentIndex<attrTupleSize;++componentIndex )
						{
							const size_t destinationIndex = static_cast<size_t>(elementIndex)
								* static_cast<size_t>(attrTupleSize) + static_cast<size_t>(componentIndex);
							storeNumericComponent(data, destinationIndex, attrStorage, tuple->value(componentIndex));
						}
					}
				}
				else if( values->contains("arrays") )
				{
					json::ArrayPtr componentArrays = values->array("arrays");
					if( !componentArrays || componentArrays->size() != attrTupleSize )
						throw std::runtime_error( "HouGeo::loadAttribute component array count mismatch for attribute " + attrName );

					for( int componentIndex=0;componentIndex<attrTupleSize;++componentIndex )
					{
						json::ArrayPtr componentValues = componentArrays->array(componentIndex);
						if( !componentValues || componentValues->size() != elementCount )
							throw std::runtime_error( "HouGeo::loadAttribute component value count mismatch for attribute " + attrName );

						for( sint64 elementIndex=0;elementIndex<elementCount;++elementIndex )
						{
							const size_t destinationIndex = static_cast<size_t>(elementIndex)
								* static_cast<size_t>(attrTupleSize) + static_cast<size_t>(componentIndex);
							storeNumericComponent(data, destinationIndex, attrStorage,
								componentValues->value(static_cast<int>(elementIndex)));
						}
					}
				}
				else if( values->contains("rawpagedata") )
				{
					const int elementsPerPage = values->get<int>("pagesize");
					if( elementsPerPage <= 0 )
						throw std::runtime_error( "HouGeo::loadAttribute pagesize must be positive for attribute " + attrName );
					const size_t pageCount = elementCount == 0 ? 0 :
						(static_cast<size_t>(elementCount) + static_cast<size_t>(elementsPerPage) - 1u)
						/ static_cast<size_t>(elementsPerPage);

					// one pack is a sequence of components
					// packing is used to describe in which sequence components are written to the file
					// packing allows to store vectors as list of structs or struct of lists.
					std::vector<ubyte> attrPacking;
					if( values->contains("packing") )
					{
						json::ArrayPtr packingArray = values->array("packing");
						if( !packingArray || packingArray->size() <= 0
							|| packingArray->size() > std::numeric_limits<int>::max() )
							throw std::runtime_error( "HouGeo::loadAttribute packing must be a non-empty array for attribute " + attrName );
						const int packingCount = static_cast<int>(packingArray->size());
						for( int packingIndex=0;packingIndex<packingCount;++packingIndex )
						{
							const ubyte packSize = packingArray->get<ubyte>(packingIndex);
							if( packSize == 0 )
								throw std::runtime_error( "HouGeo::loadAttribute packing cannot contain zero for attribute " + attrName );
							attrPacking.push_back(packSize);
						}
					}else
					{
						if( attrTupleSize < 0 || attrTupleSize > std::numeric_limits<ubyte>::max() )
							throw std::runtime_error( "HouGeo::loadAttribute tuple size exceeds packing range for attribute " + attrName );
						attrPacking.push_back(static_cast<ubyte>(attrTupleSize));
					}
					size_t packedComponentCount = 0;
					for( ubyte packSize : attrPacking )
						packedComponentCount += static_cast<size_t>(packSize);
					if( packedComponentCount != static_cast<size_t>(attrTupleSize) )
						throw std::runtime_error( "HouGeo::loadAttribute packing does not cover tuple size for attribute " + attrName );

					// constantpageflags is an array which
					// contains an array for each pack
					// each of those per pack arrays contains flags for each page
					// which tell us wether the pack is constant for this page
					std::vector<std::vector<bool>> constantPageFlagsPerPack;

					// to make things even more fun, some packs can be constant
					// and this may be different per page - oh boy
					if( values->contains("constantpageflags") )
					{
						json::ArrayPtr constantPageFlags = values->array("constantpageflags");
						if( !constantPageFlags || constantPageFlags->size() != static_cast<sint64>(attrPacking.size()) )
							throw std::runtime_error( "HouGeo::loadAttribute constantpageflags pack count mismatch for attribute " + attrName );

						for( size_t packIndex=0;packIndex<attrPacking.size();++packIndex )
						{
							json::ArrayPtr packConstantFlags = constantPageFlags->array(static_cast<int>(packIndex));
							if( !packConstantFlags || packConstantFlags->size() != static_cast<sint64>(pageCount) )
								throw std::runtime_error( "HouGeo::loadAttribute constantpageflags page count mismatch for attribute " + attrName );
							constantPageFlagsPerPack.emplace_back();
							constantPageFlagsPerPack.back().reserve(pageCount);
							for( size_t pageIndex=0;pageIndex<pageCount;++pageIndex )
								constantPageFlagsPerPack.back().push_back(
									packConstantFlags->get<bool>(static_cast<int>(pageIndex)) );
						}
					}else
					{
						constantPageFlagsPerPack.resize(attrPacking.size(), std::vector<bool>(pageCount, false));
					}

					json::ArrayPtr rawPageData = values->array("rawpagedata");
					if( !rawPageData )
						throw std::runtime_error( "HouGeo::loadAttribute rawpagedata must be an array for attribute " + attrName );

					size_t expectedRawValueCount = 0;
					for( size_t pageIndex=0;pageIndex<pageCount;++pageIndex )
					{
						const size_t pageStartElement = pageIndex * static_cast<size_t>(elementsPerPage);
						const size_t pageElementCount = std::min(static_cast<size_t>(elementCount) - pageStartElement,
							static_cast<size_t>(elementsPerPage));
						for( size_t packIndex=0;packIndex<attrPacking.size();++packIndex )
						{
							const size_t repeatedElementCount = constantPageFlagsPerPack[packIndex][pageIndex]
								? 1u : pageElementCount;
							const size_t packValueCount = checkedProduct(static_cast<size_t>(attrPacking[packIndex]),
								repeatedElementCount, "Attribute page payload");
							if( packValueCount > std::numeric_limits<size_t>::max() - expectedRawValueCount )
								throw std::length_error( "Attribute page payload exceeds addressable storage" );
							expectedRawValueCount += packValueCount;
						}
					}
					if( rawPageData->size() != static_cast<sint64>(expectedRawValueCount) )
						throw std::runtime_error( "HouGeo::loadAttribute rawpagedata size mismatch for attribute " + attrName );

					// Repack source tuples into the destination component layout.

					if( elementCount > static_cast<sint64>(std::numeric_limits<int>::max()) )
						throw std::overflow_error( "HouGeo::loadAttribute element count exceeds int range for attribute " + attrName );
					attr->element_count_ = static_cast<int>(elementCount);
					size_t elementsRemaining = static_cast<size_t>(attr->element_count_);

					// process each page
					size_t pageIndex = 0;
					size_t pageStartIndex = 0;
					while( elementsRemaining>0 )
					{
						const size_t pageStartElement = pageIndex * static_cast<size_t>(elementsPerPage);
						const size_t numElements = std::min(elementsRemaining, static_cast<size_t>(elementsPerPage));

						// process each pack
						size_t packIndex = 0;
						size_t startComponentIndex = 0;
						for( std::vector<ubyte>::iterator it = attrPacking.begin(); it != attrPacking.end();++it, ++packIndex )
						{
							ubyte pack = *it;
							const int remainingComponents = std::max(0, dstTupleSize - static_cast<int>(startComponentIndex));
							const size_t maxPack = std::min(static_cast<size_t>(pack), static_cast<size_t>(remainingComponents));

							if( maxPack == 0 )
								break;

							// is pack for current page constant?
							bool isConstant = constantPageFlagsPerPack[packIndex].empty() ? false : constantPageFlagsPerPack[packIndex][pageIndex];


							// if pack is constant only the first element is given, this is the reference
							// find element index where the new page starts
							size_t elementIndex = pageStartIndex;

							// now iterate over all elements of current page and get values from current pack
							for( size_t i=0;i<numElements;++i )
							{
								// we update elementIndex only if pack is varying within current page
								// otherwise we will just keep pointing to the reference element
								if( !isConstant )
									// get page element index into rawpagedata for current pack
									// we can do pageStartElement*attrTupleSize because packing doesnt matter for past pages
									elementIndex = pageStartIndex + i*pack;
								// get global element index for writing into our dense array
								const size_t destElementIndex = (pageStartElement + i) * static_cast<size_t>(dstTupleSize);

								// for each component of current pack
								for( size_t component=0;component<maxPack;++component )
								{
									// Copy the packed component into the dense destination tuple.
									const size_t rawIndex = elementIndex + component;
									if( rawIndex > static_cast<size_t>(std::numeric_limits<int>::max()) )
										throw std::overflow_error( "HouGeo::loadAttribute raw page index exceeds int range for attribute " + attrName );
									storeNumericComponent(data, destElementIndex + startComponentIndex + component,
										attrStorage, rawPageData->value(static_cast<int>(rawIndex)));
								}
							}


							startComponentIndex += pack;
							if( !isConstant )
								pageStartIndex += numElements*pack;
							else
								pageStartIndex += pack;
						}


						elementsRemaining -= numElements;

						// proceed next page
						++pageIndex;
					}
					if( pageStartIndex != expectedRawValueCount )
						throw std::runtime_error( "HouGeo::loadAttribute did not consume the complete paged payload for attribute " + attrName );

				}
			}
		}else
		if( attrType == AttributeAdapter::Type::string )
		{
			if( !attrData->contains("strings") )
				throw std::runtime_error( "HouGeo::loadAttribute missing string table for attribute " + attrName );

			json::ArrayPtr stringsArray = attrData->array("strings");
			const int stringCount = checkedArrayCount(stringsArray,
				"HouGeo::loadAttribute string table for attribute " + attrName);
			std::vector<std::string> stringTable;
			stringTable.reserve(static_cast<size_t>(stringCount));
			for( int stringIndex=0;stringIndex<stringCount;++stringIndex )
				stringTable.push_back(stringsArray->get<std::string>(stringIndex));

			const AttributeAdapter::TupleSize tupleSize(attrData->get<int>("size", 1));
			const size_t scalarCount = checkedProduct(
				static_cast<size_t>(elementCount), tupleSize.asSize(), "String attribute value count");
			attr->name_ = attrName;
			attr->type_ = attrType;
			attr->storage_ = AttributeAdapter::Storage::int32;
			attr->tuple_size_ = tupleSize;

			if( attrData->contains("indices") )
			{
				json::ObjectPtr indices = toObject(attrData->array("indices"));
				const std::vector<int> indexValues = expandPagedIntValues(
					indices, elementCount, tupleSize.value(), attrName);

				attr->string_values_.reserve(scalarCount);
				for( int stringIndex : indexValues )
				{
					if( stringIndex == -1 )
					{
						attr->string_values_.emplace_back();
						continue;
					}
					if( stringIndex < 0 || static_cast<size_t>(stringIndex) >= stringTable.size() )
						throw std::runtime_error( "HouGeo::loadAttribute string index out of range for attribute " + attrName );
					attr->string_values_.push_back(stringTable[static_cast<size_t>(stringIndex)]);
				}
			}
			else if( stringTable.size() == scalarCount )
				attr->string_values_ = stringTable;
			else if( stringTable.size() == 1 && scalarCount > 0 )
				attr->string_values_.assign(scalarCount, stringTable.front());
			else if( scalarCount == 0 )
				attr->string_values_.clear();
			else
				throw std::runtime_error( "HouGeo::loadAttribute cannot map string table to elements for attribute " + attrName );

			if( attr->string_values_.size() != scalarCount )
				throw std::runtime_error( "HouGeo::loadAttribute string tuple storage mismatch for attribute " + attrName );
			attr->element_count_ = static_cast<int>(elementCount);
		}else if( attrType == AttributeAdapter::Type::dictionary )
		{
			json::ArrayPtr dictionaries = attrData->array("dicts");
			const int dictionaryCount = checkedArrayCount(dictionaries,
				"HouGeo::loadAttribute dictionary table for attribute " + attrName);
			std::vector<json::ObjectPtr> dictionaryTable;
			dictionaryTable.reserve(static_cast<size_t>(dictionaryCount));
			for( int dictionaryIndex=0;dictionaryIndex<dictionaryCount;++dictionaryIndex )
			{
				json::ObjectPtr dictionary = dictionaries->object(dictionaryIndex);
				if( !dictionary )
					throw std::runtime_error( "HouGeo::loadAttribute expected a dictionary value for attribute " + attrName );
				dictionaryTable.push_back(dictionary);
			}

			attr->name_ = attrName;
			attr->type_ = attrType;
			attr->storage_ = AttributeAdapter::Storage::int32;
			attr->tuple_size_ = AttributeAdapter::TupleSize(1);
			attr->dictionary_values_.reserve(static_cast<size_t>(elementCount));

			if( attrData->contains("indices") )
			{
				json::ObjectPtr indices = toObject(attrData->array("indices"));
				const std::vector<int> indexValues = expandPagedIntValues(indices, elementCount, 1, attrName);
				for( int dictionaryIndex : indexValues )
				{
					if( dictionaryIndex == -1 )
					{
						attr->dictionary_values_.push_back(json::Object::create());
						continue;
					}
					if( dictionaryIndex < 0 || static_cast<size_t>(dictionaryIndex) >= dictionaryTable.size() )
						throw std::runtime_error( "HouGeo::loadAttribute dictionary index out of range for attribute " + attrName );
					attr->dictionary_values_.push_back(dictionaryTable[static_cast<size_t>(dictionaryIndex)]);
				}
			}else if( dictionaryTable.size() == static_cast<size_t>(elementCount) )
				attr->dictionary_values_ = dictionaryTable;
			else if( dictionaryTable.size() == 1 && elementCount > 0 )
				attr->dictionary_values_.assign(static_cast<size_t>(elementCount), dictionaryTable.front());
			else if( elementCount != 0 )
				throw std::runtime_error( "HouGeo::loadAttribute cannot map dictionary table to elements for attribute " + attrName );

			attr->element_count_ = static_cast<int>(attr->dictionary_values_.size());
		}else if( attrType == AttributeAdapter::Type::invalid )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
				"HouGeo::loadAttribute does not support attribute type " + attrDef->get<std::string>("type"),
				-1, "type"});
		}

		return attr;
	}


	void HouGeo::loadTopology( json::ObjectPtr topologyObject, sint64 pointCount )
	{
		if( !topologyObject || pointCount < 0 )
			throw std::runtime_error( "HouGeo::loadTopology received invalid topology metadata" );

		HouTopology::Ptr topology = std::make_shared<HouTopology>();
		if( topologyObject->contains("pointref") )
		{
			json::ObjectPtr pointReferences = toObject( topologyObject->array("pointref") );
			if( pointReferences->contains("indices") )
			{
				json::ArrayPtr indices = pointReferences->array("indices");
				if( !indices )
					throw std::runtime_error( "HouGeo::loadTopology missing index array" );
				const int indexCount = checkedArrayCount(indices,
					"HouGeo::loadTopology index array");
				for( int indexPosition=0;indexPosition<indexCount;++indexPosition )
				{
					const int pointIndex = indices->get<int>(indexPosition);
					if( pointIndex < 0 || static_cast<sint64>(pointIndex) >= pointCount )
						throw std::runtime_error( "HouGeo::loadTopology point index out of range" );
					topology->indexBuffer.push_back(pointIndex);
				}
			}
		}
		m_topology = topology;
	}

	void HouGeo::loadPrimitive( json::ArrayPtr primitive, SharedPrimitiveData& sharedPrimitiveData )
	{
		if( !primitive || primitive->size() != 2 )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadPrimitive expected definition and data arrays", -1, ""});

		json::ObjectPtr definition;
		withSchemaPath("definition", [&]()
		{
			json::ArrayPtr definitionValues = primitive->array(0);
			if( !definitionValues )
				throw std::runtime_error(
					"HouGeo::loadPrimitive definition must be a flattened object" );
			definition = toObject(definitionValues);
			if( !definition )
				throw std::runtime_error( "HouGeo::loadPrimitive definition is invalid" );
		});
		std::string primitiveType;
		if( definition->contains("type") )
			primitiveType = definition->get<std::string>("type", "");
		if( primitiveType.empty() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadPrimitive missing primitive type", -1, "definition.type"});

		// primitive
		if( primitiveType=="Volume" )
			withSchemaPath("data", [&]() { loadVolumePrimitive(toObject(primitive->array(1)), sharedPrimitiveData); });
		else
		if( primitiveType=="PackedGeometry" )
			withSchemaPath("data", [&]() { loadPackedGeometryPrimitive(toObject(primitive->array(1)), sharedPrimitiveData); });
		else
		if( primitiveType=="PackedFragment" )
			withSchemaPath("data", [&]() { loadPackedFragmentPrimitive(toObject(primitive->array(1)), sharedPrimitiveData); });
		else
		if( primitiveType=="PackedDisk" )
			withSchemaPath("data", [&]() { loadPackedDiskPrimitive(toObject(primitive->array(1))); });
		else
		if( primitiveType=="PackedDiskSequence" )
			withSchemaPath("data", [&]() { loadPackedDiskSequencePrimitive(toObject(primitive->array(1))); });
		else
		if( primitiveType=="VDB" )
			withSchemaPath("data", [&]() { loadNativeVdbPrimitive(toObject(primitive->array(1))); });
		else
		if( primitiveType=="Poly" )
			withSchemaPath("data", [&]() { loadPolyPrimitive(toObject(primitive->array(1))); });
		else
		if( primitiveType=="Polygon_run" || primitiveType=="p_r" )
			withSchemaPath("data", [&]() { loadPolygonRun(toObject(primitive->array(1)), true); });
		else
		if( primitiveType=="PolygonCurve_run" || primitiveType=="c_r" )
			withSchemaPath("data", [&]() { loadPolygonRun(toObject(primitive->array(1)), false); });
		else
		if( primitiveType=="run" )
		{
			if( !definition->contains("runtype") )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeo::loadPrimitive run record is missing runtype", -1, "definition.runtype"});
			if( definition->get<std::string>( "runtype" ) == "Poly" )
			{
				withSchemaPath("data", [&]()
				{
					loadPolyPrimitiveRun(definition, primitive->array(1));
				});
				return;
			}
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
				"HouGeo::loadPrimitive does not support run type " + definition->get<std::string>("runtype"), -1, "definition.runtype"});
		}
		else
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
				"HouGeo::loadPrimitive does not support primitive type " + primitiveType, -1, "definition.type"});
		}

	}

	void HouGeo::loadPackedGeometryPrimitive(
		json::ObjectPtr packedGeometry,
		SharedPrimitiveData& sharedPrimitiveData )
	{
		if( !packedGeometry )
			throw std::invalid_argument(
				"HouGeo::loadPackedGeometryPrimitive received null data" );
		json::ObjectPtr parameters = packedGeometry->object("parameters");
		if( !parameters )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedGeometry primitive is missing parameters",
				-1,
				"parameters"});
		const std::string embeddedId = parameters->get<std::string>("embedded", "");
		if( embeddedId.empty() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedGeometry primitive is missing embedded geometry id",
				-1,
				"parameters.embedded"});
		const auto embedded = sharedPrimitiveData.sharedEmbeddedGeometry.find(embeddedId);
		if( embedded == sharedPrimitiveData.sharedEmbeddedGeometry.end() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedGeometry embedded geometry was not found",
				-1,
				"parameters.embedded"});

		const int topologyVertex = packedGeometry->get<int>("vertex", -1);
		if( topologyVertex < 0 || !m_topology
			|| static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedGeometry topology vertex is outside vertexcount",
				-1,
				"vertex"});
		}

		math::V3f pivot(0.0f);
		json::ArrayPtr pivotValues = packedGeometry->array("pivot");
		if( pivotValues )
		{
			if( pivotValues->size() != 3 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedGeometry pivot requires three values",
					-1,
					"pivot"});
			pivot = math::V3f(
				pivotValues->get<real32>(0),
				pivotValues->get<real32>(1),
				pivotValues->get<real32>(2));
		}

		math::M33f transform = math::M33f::identity();
		json::ArrayPtr transformValues = packedGeometry->array("transform");
		if( transformValues )
		{
			if( transformValues->size() != 9 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedGeometry transform requires nine values",
					-1,
					"transform"});
			transform = math::M33f(
				transformValues->get<real32>(0),
				transformValues->get<real32>(1),
				transformValues->get<real32>(2),
				transformValues->get<real32>(3),
				transformValues->get<real32>(4),
				transformValues->get<real32>(5),
				transformValues->get<real32>(6),
				transformValues->get<real32>(7),
				transformValues->get<real32>(8));
		}

		auto result = std::make_shared<HouPackedGeometry>();
		result->embedded_geometry_ = embedded->second;
		result->topology_vertex_ = topologyVertex;
		result->pivot_ = pivot;
		result->transform_ = transform;
		result->viewport_lod_ = packedGeometry->get<std::string>("viewportlod", "full");
		result->point_instance_transform_ =
			parameters->get<int>("pointinstancetransform", 0) != 0;
		result->treat_as_folder_ = parameters->get<int>("treatasfolder", 0) != 0;
		m_primitives.push_back(std::move(result));
	}

	void HouGeo::loadPackedFragmentPrimitive(
		json::ObjectPtr packedFragment,
		SharedPrimitiveData& sharedPrimitiveData )
	{
		if( !packedFragment )
			throw std::invalid_argument(
				"HouGeo::loadPackedFragmentPrimitive received null data" );
		json::ObjectPtr parameters = packedFragment->object("parameters");
		if( !parameters )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedFragment primitive is missing parameters",
				-1,
				"parameters"});

		const std::string embeddedId = parameters->get<std::string>("embedded", "");
		if( embeddedId.empty() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedFragment primitive is missing embedded geometry id",
				-1,
				"parameters.embedded"});
		const auto embedded = sharedPrimitiveData.sharedEmbeddedGeometry.find(embeddedId);
		if( embedded == sharedPrimitiveData.sharedEmbeddedGeometry.end() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedFragment embedded geometry was not found",
				-1,
				"parameters.embedded"});

		const std::string fragmentAttribute = parameters->get<std::string>("attribute", "");
		if( fragmentAttribute.empty() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedFragment primitive is missing its fragment attribute",
				-1,
				"parameters.attribute"});
		const std::string fragmentName = parameters->get<std::string>("name", "");
		if( fragmentName.empty() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedFragment primitive is missing its fragment name",
				-1,
				"parameters.name"});

		const int topologyVertex = packedFragment->get<int>("vertex", -1);
		if( topologyVertex < 0 || !m_topology
			|| static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedFragment topology vertex is outside vertexcount",
				-1,
				"vertex"});
		}

		const auto parseBounds = [&](const std::string& key,
			const HouGeoAdapter::PackedFragmentPrimitive::Bounds* fallback = nullptr)
		{
			json::ArrayPtr values = parameters->array(key);
			if( !values )
			{
				if( fallback )
					return *fallback;
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedFragment " + key + " requires six values",
					-1,
					"parameters." + key});
			}
			if( values->size() != 6 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedFragment " + key + " requires six values",
					-1,
					"parameters." + key});
			HouGeoAdapter::PackedFragmentPrimitive::Bounds result{};
			for( int index = 0; index < 6; ++index )
				result[static_cast<std::size_t>(index)] = values->get<real32>(index);
			return result;
		};

		const HouGeoAdapter::PackedFragmentPrimitive::Bounds bounds = parseBounds("bounds");
		const HouGeoAdapter::PackedFragmentPrimitive::Bounds cachedBounds =
			parseBounds("cachedbounds", &bounds);

		math::V3f pivot(0.0f);
		json::ArrayPtr pivotValues = packedFragment->array("pivot");
		if( pivotValues )
		{
			if( pivotValues->size() != 3 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedFragment pivot requires three values",
					-1,
					"pivot"});
			pivot = math::V3f(
				pivotValues->get<real32>(0),
				pivotValues->get<real32>(1),
				pivotValues->get<real32>(2));
		}

		math::M33f transform = math::M33f::identity();
		json::ArrayPtr transformValues = packedFragment->array("transform");
		if( transformValues )
		{
			if( transformValues->size() != 9 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedFragment transform requires nine values",
					-1,
					"transform"});
			transform = math::M33f(
				transformValues->get<real32>(0),
				transformValues->get<real32>(1),
				transformValues->get<real32>(2),
				transformValues->get<real32>(3),
				transformValues->get<real32>(4),
				transformValues->get<real32>(5),
				transformValues->get<real32>(6),
				transformValues->get<real32>(7),
				transformValues->get<real32>(8));
		}

		auto result = std::make_shared<HouPackedFragment>();
		result->embedded_geometry_ = embedded->second;
		result->topology_vertex_ = topologyVertex;
		result->pivot_ = pivot;
		result->transform_ = transform;
		result->viewport_lod_ = packedFragment->get<std::string>("viewportlod", "full");
		result->point_instance_transform_ =
			parameters->get<int>("pointinstancetransform", 0) != 0;
		result->fragment_attribute_ = fragmentAttribute;
		result->fragment_name_ = fragmentName;
		result->bounds_ = bounds;
		result->cached_bounds_ = cachedBounds;
		m_primitives.push_back(std::move(result));
	}

	void HouGeo::loadPackedDiskPrimitive( json::ObjectPtr packedDisk )
	{
		if( !packedDisk )
			throw std::invalid_argument( "HouGeo::loadPackedDiskPrimitive received null data" );
		json::ObjectPtr parameters = packedDisk->object("parameters");
		if( !parameters )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema, "PackedDisk primitive is missing parameters",
				-1, "parameters"});

		const std::string filename = parameters->get<std::string>("filename", "");
		if( filename.empty() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema, "PackedDisk primitive is missing its filename",
				-1, "parameters.filename"});

		const int topologyVertex = packedDisk->get<int>("vertex", -1);
		if( topologyVertex < 0 || !m_topology
			|| static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedDisk topology vertex is outside vertexcount",
				-1, "vertex"});
		}

		math::V3f pivot(0.0f);
		json::ArrayPtr pivotValues = packedDisk->array("pivot");
		if( pivotValues )
		{
			if( pivotValues->size() != 3 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedDisk pivot requires three values", -1, "pivot"});
			pivot = math::V3f(
				pivotValues->get<real32>(0),
				pivotValues->get<real32>(1),
				pivotValues->get<real32>(2));
		}

		math::M33f transform = math::M33f::identity();
		json::ArrayPtr transformValues = packedDisk->array("transform");
		if( transformValues )
		{
			if( transformValues->size() != 9 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedDisk transform requires nine values", -1, "transform"});
			transform = math::M33f(
				transformValues->get<real32>(0),
				transformValues->get<real32>(1),
				transformValues->get<real32>(2),
				transformValues->get<real32>(3),
				transformValues->get<real32>(4),
				transformValues->get<real32>(5),
				transformValues->get<real32>(6),
				transformValues->get<real32>(7),
				transformValues->get<real32>(8));
		}

		auto result = std::make_shared<HouPackedDisk>();
		result->topology_vertex_ = topologyVertex;
		result->filename_ = filename;
		result->expand_frame_ = parameters->get<real32>("expandframe", 1.0f);
		result->expand_filename_ = parameters->get<int>("expandfilename", 0) != 0;
		result->pivot_ = pivot;
		result->transform_ = transform;
		result->viewport_lod_ = packedDisk->get<std::string>("viewportlod", "full");
		result->point_instance_transform_ =
			parameters->get<int>("pointinstancetransform", 0) != 0;
		result->treat_as_folder_ = parameters->get<int>("treatasfolder", 0) != 0;
		m_primitives.push_back(std::move(result));
	}

	void HouGeo::loadPackedDiskSequencePrimitive( json::ObjectPtr packedDiskSequence )
	{
		if( !packedDiskSequence )
			throw std::invalid_argument(
				"HouGeo::loadPackedDiskSequencePrimitive received null data" );
		json::ObjectPtr parameters = packedDiskSequence->object("parameters");
		if( !parameters )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedDiskSequence primitive is missing parameters",
				-1, "parameters"});

		json::ArrayPtr filenameValues = parameters->array("filenames");
		if( !filenameValues || filenameValues->size() <= 0 )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedDiskSequence requires at least one filename",
				-1, "parameters.filenames"});
		const int filenameCount = checkedArrayCount(
			filenameValues, "PackedDiskSequence filenames");
		std::vector<std::string> filenames;
		filenames.reserve(static_cast<std::size_t>(filenameCount));
		for( int index = 0; index < filenameCount; ++index )
		{
			const std::string filename = filenameValues->get<std::string>(index);
			if( filename.empty() )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedDiskSequence filename cannot be empty",
					-1, "parameters.filenames[" + std::to_string(index) + "]"});
			filenames.push_back(filename);
		}

		const int topologyVertex = packedDiskSequence->get<int>("vertex", -1);
		if( topologyVertex < 0 || !m_topology
			|| static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedDiskSequence topology vertex is outside vertexcount",
				-1, "vertex"});
		}

		math::V3f pivot(0.0f);
		json::ArrayPtr pivotValues = packedDiskSequence->array("pivot");
		if( pivotValues )
		{
			if( pivotValues->size() != 3 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedDiskSequence pivot requires three values", -1, "pivot"});
			pivot = math::V3f(
				pivotValues->get<real32>(0),
				pivotValues->get<real32>(1),
				pivotValues->get<real32>(2));
		}

		math::M33f transform = math::M33f::identity();
		json::ArrayPtr transformValues = packedDiskSequence->array("transform");
		if( transformValues )
		{
			if( transformValues->size() != 9 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
					DiagnosticCategory::schema,
					"PackedDiskSequence transform requires nine values", -1, "transform"});
			transform = math::M33f(
				transformValues->get<real32>(0),
				transformValues->get<real32>(1),
				transformValues->get<real32>(2),
				transformValues->get<real32>(3),
				transformValues->get<real32>(4),
				transformValues->get<real32>(5),
				transformValues->get<real32>(6),
				transformValues->get<real32>(7),
				transformValues->get<real32>(8));
		}

		const std::string wrap = parameters->get<std::string>("wrap", "cycle");
		PackedDiskSequencePrimitive::WrapMode wrapMode;
		if( wrap == "cycle" )
			wrapMode = PackedDiskSequencePrimitive::WrapMode::cycle;
		else if( wrap == "clamp" )
			wrapMode = PackedDiskSequencePrimitive::WrapMode::clamp;
		else if( wrap == "strict" )
			wrapMode = PackedDiskSequencePrimitive::WrapMode::strict;
		else if( wrap == "mirror" )
			wrapMode = PackedDiskSequencePrimitive::WrapMode::mirror;
		else
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"PackedDiskSequence wrap mode is invalid",
				-1, "parameters.wrap"});

		auto result = std::make_shared<HouPackedDiskSequence>();
		result->topology_vertex_ = topologyVertex;
		result->filenames_ = std::move(filenames);
		result->index_ = parameters->get<real32>("index", 0.0f);
		result->wrap_mode_ = wrapMode;
		result->pivot_ = pivot;
		result->transform_ = transform;
		result->viewport_lod_ =
			packedDiskSequence->get<std::string>("viewportlod", "full");
		result->point_instance_transform_ =
			parameters->get<int>("pointinstancetransform", 0) != 0;
		m_primitives.push_back(std::move(result));
	}

	void HouGeo::loadNativeVdbPrimitive( json::ObjectPtr nativeVdb )
	{
		if( !nativeVdb )
			throw std::invalid_argument( "HouGeo::loadNativeVdbPrimitive received null data" );
		const int topologyVertex = nativeVdb->get<int>("vertex", -1);
		if( topologyVertex < 0 || !m_topology
			|| static_cast<sint64>(topologyVertex) >= m_topology->indexCount() )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"VDB topology vertex is outside vertexcount",
				-1,
				"vertex"});
		}
		json::ArrayPtr payload = nativeVdb->array("vdb");
		if( !payload || payload->size() < 2 )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
				DiagnosticCategory::schema,
				"VDB primitive is missing its serialized sparse payload",
				-1,
				"vdb"});

		auto result = std::make_shared<HouVdb>();
		result->topology_vertex_ = topologyVertex;
		result->serialized_payload_ = std::move(payload);
		m_primitives.push_back(std::move(result));
	}

	// HouGeo::HouPackedDisk =============================================

	int HouGeo::HouPackedDisk::topologyVertex() const
	{
		return topology_vertex_;
	}

	std::string HouGeo::HouPackedDisk::filename() const
	{
		return filename_;
	}

	real32 HouGeo::HouPackedDisk::expandFrame() const
	{
		return expand_frame_;
	}

	bool HouGeo::HouPackedDisk::expandFilename() const
	{
		return expand_filename_;
	}

	math::V3f HouGeo::HouPackedDisk::pivot() const
	{
		return pivot_;
	}

	math::M33f HouGeo::HouPackedDisk::transform() const
	{
		return transform_;
	}

	std::string HouGeo::HouPackedDisk::viewportLod() const
	{
		return viewport_lod_;
	}

	bool HouGeo::HouPackedDisk::pointInstanceTransform() const
	{
		return point_instance_transform_;
	}

	bool HouGeo::HouPackedDisk::treatAsFolder() const
	{
		return treat_as_folder_;
	}

	// HouGeo::HouPackedDiskSequence =====================================

	int HouGeo::HouPackedDiskSequence::topologyVertex() const
	{
		return topology_vertex_;
	}

	std::vector<std::string> HouGeo::HouPackedDiskSequence::filenames() const
	{
		return filenames_;
	}

	real32 HouGeo::HouPackedDiskSequence::index() const
	{
		return index_;
	}

	HouGeoAdapter::PackedDiskSequencePrimitive::WrapMode
	HouGeo::HouPackedDiskSequence::wrapMode() const
	{
		return wrap_mode_;
	}

	math::V3f HouGeo::HouPackedDiskSequence::pivot() const
	{
		return pivot_;
	}

	math::M33f HouGeo::HouPackedDiskSequence::transform() const
	{
		return transform_;
	}

	std::string HouGeo::HouPackedDiskSequence::viewportLod() const
	{
		return viewport_lod_;
	}

	bool HouGeo::HouPackedDiskSequence::pointInstanceTransform() const
	{
		return point_instance_transform_;
	}

	// HouGeo::HouSparseVdb ==============================================

	int HouGeo::HouSparseVdb::topologyVertex() const
	{
		return topology_vertex_;
	}

	const SparseFloatGrid& HouGeo::HouSparseVdb::sparseGrid() const
	{
		return sparse_grid_;
	}

	int HouGeo::HouVdb::topologyVertex() const
	{
		return topology_vertex_;
	}

	json::ArrayPtr HouGeo::HouVdb::serializedPayload() const
	{
		return serialized_payload_;
	}

	// HouGeo::HouPackedFragment =========================================

	HouGeoAdapter::ConstPtr HouGeo::HouPackedFragment::embeddedGeometry() const
	{
		return embedded_geometry_;
	}

	int HouGeo::HouPackedFragment::topologyVertex() const
	{
		return topology_vertex_;
	}

	math::V3f HouGeo::HouPackedFragment::pivot() const
	{
		return pivot_;
	}

	math::M33f HouGeo::HouPackedFragment::transform() const
	{
		return transform_;
	}

	std::string HouGeo::HouPackedFragment::viewportLod() const
	{
		return viewport_lod_;
	}

	bool HouGeo::HouPackedFragment::pointInstanceTransform() const
	{
		return point_instance_transform_;
	}

	bool HouGeo::HouPackedFragment::treatAsFolder() const
	{
		return false;
	}

	std::string HouGeo::HouPackedFragment::fragmentAttribute() const
	{
		return fragment_attribute_;
	}

	std::string HouGeo::HouPackedFragment::fragmentName() const
	{
		return fragment_name_;
	}

	HouGeoAdapter::PackedFragmentPrimitive::Bounds HouGeo::HouPackedFragment::bounds() const
	{
		return bounds_;
	}

	HouGeoAdapter::PackedFragmentPrimitive::Bounds HouGeo::HouPackedFragment::cachedBounds() const
	{
		return cached_bounds_;
	}

	// HouGeo::HouPackedGeometry =========================================

	HouGeoAdapter::ConstPtr HouGeo::HouPackedGeometry::embeddedGeometry() const
	{
		return embedded_geometry_;
	}

	int HouGeo::HouPackedGeometry::topologyVertex() const
	{
		return topology_vertex_;
	}

	math::V3f HouGeo::HouPackedGeometry::pivot() const
	{
		return pivot_;
	}

	math::M33f HouGeo::HouPackedGeometry::transform() const
	{
		return transform_;
	}

	std::string HouGeo::HouPackedGeometry::viewportLod() const
	{
		return viewport_lod_;
	}

	bool HouGeo::HouPackedGeometry::pointInstanceTransform() const
	{
		return point_instance_transform_;
	}

	bool HouGeo::HouPackedGeometry::treatAsFolder() const
	{
		return treat_as_folder_;
	}

	// HouGeo::HouVolume ==================================================

	void HouGeo::loadVolumePrimitive( json::ObjectPtr volume, SharedPrimitiveData& sharedPrimitiveData )
	{
		if( !volume )
			throw std::invalid_argument( "HouGeo::loadVolumePrimitive received null volume data" );
		if( !volume->contains("res") )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadVolumePrimitive missing resolution", -1, "res"});

		HouVolume::Ptr volumePrimitive = std::make_shared<HouVolume>();
		volumePrimitive->scalar_field_ = std::make_shared<ScalarField>();

		math::V3i resolution;
		withSchemaPath("res", [&]()
		{
			json::ArrayPtr resolutionValues = volume->array("res");
			if( !resolutionValues || resolutionValues->size() != 3 )
				throw std::runtime_error( "HouGeo::loadVolumePrimitive resolution must contain three values" );
			resolution = math::V3i(resolutionValues->get<int>(0), resolutionValues->get<int>(1),
				resolutionValues->get<int>(2));
			volumeVoxelCount(resolution);
		});

		const bool hasVertex = volume->contains("vertex");
		const bool hasTransform = volume->contains("transform");
		if( hasVertex != hasTransform )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadVolumePrimitive requires vertex and transform together", -1, hasVertex ? "transform" : "vertex"});

		if( hasVertex )
		{
			math::Matrix44d rotationScale;
			withSchemaPath("transform", [&]()
			{
				json::ArrayPtr transformValues = volume->array("transform");
				if( !transformValues || transformValues->size() != 9 )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive transform must contain nine values" );
				rotationScale = math::Matrix44d(
					transformValues->get<float>(0), transformValues->get<float>(1), transformValues->get<float>(2), 0.0,
					transformValues->get<float>(3), transformValues->get<float>(4), transformValues->get<float>(5), 0.0,
					transformValues->get<float>(6), transformValues->get<float>(7), transformValues->get<float>(8), 0.0,
					0.0, 0.0, 0.0, 1.0);
			});

			math::V3f position;
			withSchemaPath("vertex", [&]()
			{
				if( !m_topology )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive requires topology for transformed volumes" );
				const int topologyVertex = volume->get<int>("vertex");
				if( topologyVertex < 0 || static_cast<size_t>(topologyVertex) >= m_topology->indexBuffer.size() )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive vertex index is outside topology" );
				volumePrimitive->topology_vertex_ = topologyVertex;

				const int pointIndex = m_topology->indexBuffer[static_cast<size_t>(topologyVertex)];
				const auto position_iterator = m_pointAttributes.find("P");
				HouAttribute::Ptr positionAttribute = position_iterator != m_pointAttributes.end()
					? position_iterator->second
					: nullptr;
				if( !positionAttribute || !positionAttribute->numeric_attribute_ )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive requires a point P attribute" );
				if( positionAttribute->tupleSize().value() < 3 )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive P requires at least three components" );
				if( pointIndex < 0 || static_cast<sint64>(pointIndex) >= positionAttribute->elementCount() )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive point index is outside P" );

				const HouGeoAdapter::RawDataView position_data = positionAttribute->rawData();
				if (!position_data.available())
					throw std::runtime_error("HouGeo::loadVolumePrimitive P has no data");

				const size_t tuple_offset = static_cast<size_t>(pointIndex)
					* positionAttribute->tupleSize().asSize();
				if( positionAttribute->storage_ == AttributeAdapter::Storage::float16 )
				{
					position = math::V3f(
						halfBitsToFloat(position_data.read<uword>(tuple_offset)),
						halfBitsToFloat(position_data.read<uword>(tuple_offset + 1)),
						halfBitsToFloat(position_data.read<uword>(tuple_offset + 2)));
				}
				else if( positionAttribute->storage_ == AttributeAdapter::Storage::float32 )
				{
					position = math::V3f(
						position_data.read<real32>(tuple_offset),
						position_data.read<real32>(tuple_offset + 1),
						position_data.read<real32>(tuple_offset + 2));
				}
				else if( positionAttribute->storage_ == AttributeAdapter::Storage::float64 )
				{
					position = math::V3f(
						static_cast<real32>(position_data.read<real64>(tuple_offset)),
						static_cast<real32>(position_data.read<real64>(tuple_offset + 1)),
						static_cast<real32>(position_data.read<real64>(tuple_offset + 2)));
				}
				else
				{
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
						"HouGeo::loadVolumePrimitive supports only floating-point P storage", -1, "P.storage"});
				}
			});

			const math::Matrix44d translation = math::Matrix44d::translationMatrix(position);
			const math::Matrix44d localToWorld = math::Matrix44d::scaleMatrix(2.0)
				* math::Matrix44d::translationMatrix(-1.0, -1.0, -1.0) * rotationScale * translation;
			volumePrimitive->scalar_field_->setLocalToWorld(localToWorld);
		}

		const bool hasSharedVoxels = volume->contains("sharedvoxels");
		const bool hasInlineVoxels = volume->contains("voxels");
		if( hasSharedVoxels == hasInlineVoxels )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadVolumePrimitive requires exactly one voxel payload", -1, "voxels"});

		std::vector<float> voxelValues;
		if( hasSharedVoxels )
		{
			withSchemaPath("sharedvoxels", [&]()
			{
				const std::string dataId = volume->get<std::string>("sharedvoxels");
				const auto sharedData = sharedPrimitiveData.sharedVoxelData.find(dataId);
				if( sharedData == sharedPrimitiveData.sharedVoxelData.end() )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive shared voxel data was not found" );
				voxelValues = loadVoxelData(sharedData->second, resolution);
			});
		}
		else
		{
			withSchemaPath("voxels", [&]()
			{
				voxelValues = loadVoxelData(toObject(volume->array("voxels")), resolution);
			});
		}

		volumePrimitive->scalar_field_->resize(resolution);
		std::copy(voxelValues.begin(), voxelValues.end(), volumePrimitive->scalar_field_->values().begin());

		if( volume->contains("visualization") )
		{
			withSchemaPath("visualization", [&]()
			{
				json::ObjectPtr visualization = volume->object("visualization");
				if( !visualization )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive visualization must be an object" );
				volumePrimitive->visualization_mode_ = visualization->get<std::string>("mode", "smoke");
				if( volumePrimitive->visualization_mode_.empty() )
					volumePrimitive->visualization_mode_ = "smoke";
				volumePrimitive->visualization_iso_ = visualization->get<real32>("iso", 0.0f);
				volumePrimitive->visualization_density_ = visualization->get<real32>("density", 1.0f);
			});
		}

		m_primitives.push_back(volumePrimitive);
	}

	std::vector<float> HouGeo::loadVoxelData(
		json::ObjectPtr voxelObject,
		const math::V3i& resolution)
	{
		if( !voxelObject )
			throw std::invalid_argument( "HouGeo::loadVoxelData received null voxel data" );
		const size_t voxelCount = volumeVoxelCount(resolution);

		const bool hasTiledArray = voxelObject->contains("tiledarray");
		const bool hasConstantArray = voxelObject->contains("constantarray");
		if( hasTiledArray == hasConstantArray )
			throw std::runtime_error( "HouGeo::loadVoxelData requires exactly one supported voxel representation" );

		if( hasConstantArray )
		{
			const float constantValue = voxelObject->get<float>("constantarray");
			return std::vector<float>(voxelCount, constantValue);
		}

		json::ObjectPtr tiledArray = toObject(voxelObject->array("tiledarray"));
		if( !tiledArray || !tiledArray->contains("tiles") )
			throw std::runtime_error( "HouGeo::loadVoxelData tiled array is missing tiles" );
		json::ArrayPtr tiles = tiledArray->array("tiles");
		if( !tiles )
			throw std::runtime_error( "HouGeo::loadVoxelData tiles must be an array" );

		const size_t tilesX = (static_cast<size_t>(resolution.x) + 15) / 16;
		const size_t tilesY = (static_cast<size_t>(resolution.y) + 15) / 16;
		const size_t tilesZ = (static_cast<size_t>(resolution.z) + 15) / 16;
		const size_t expectedTileCount = checkedProduct(checkedProduct(tilesX, tilesY, "Volume tile count"),
			tilesZ, "Volume tile count");
		if( expectedTileCount > static_cast<size_t>(std::numeric_limits<int>::max()) )
			throw std::length_error( "Volume tile count exceeds supported indexing" );
		if( tiles->size() != static_cast<sint64>(expectedTileCount) )
			throw std::runtime_error( "HouGeo::loadVoxelData tile count does not match resolution" );

		struct TileInfo
		{
			json::ObjectPtr tile;
			int compression = 0;
			int voxelOffsetX = 0;
			int voxelOffsetY = 0;
			int voxelOffsetZ = 0;
			int tileSizeX = 0;
			int tileSizeY = 0;
			int tileSizeZ = 0;
			size_t voxelCount = 0;
		};

		std::vector<TileInfo> tileInfos;
		tileInfos.reserve(expectedTileCount);
		size_t currentTileIndex = 0;
		for( size_t tileZ=0;tileZ<tilesZ;++tileZ )
		{
			const int voxelOffsetZ = static_cast<int>(tileZ * 16);
			const int tileSizeZ = std::min(16, resolution.z - voxelOffsetZ);
			for( size_t tileY=0;tileY<tilesY;++tileY )
			{
				const int voxelOffsetY = static_cast<int>(tileY * 16);
				const int tileSizeY = std::min(16, resolution.y - voxelOffsetY);
				for( size_t tileX=0;tileX<tilesX;++tileX, ++currentTileIndex )
				{
					const int voxelOffsetX = static_cast<int>(tileX * 16);
					const int tileSizeX = std::min(16, resolution.x - voxelOffsetX);
					const size_t tileVoxelCount = checkedProduct(
						checkedProduct(static_cast<size_t>(tileSizeX), static_cast<size_t>(tileSizeY), "Volume tile"),
						static_cast<size_t>(tileSizeZ), "Volume tile");
					const std::string tilePath = "tiledarray.tiles[" + std::to_string(currentTileIndex) + "]";
					withSchemaPath(tilePath, [&]()
					{
						json::ArrayPtr tileValues = tiles->array(static_cast<int>(currentTileIndex));
						if( !tileValues )
							throw std::runtime_error( "HouGeo::loadVoxelData tile must be a flattened object" );
						json::ObjectPtr tile = toObject(tileValues);
						if( !tile->contains("data") )
							throw std::runtime_error( "HouGeo::loadVoxelData tile is missing data" );

						const int compression = tile->get<int>("compression", 1);
						if( compression == 0 || compression == 1 )
						{
							json::ArrayPtr data = tile->array("data");
							if( !data || data->size() != static_cast<sint64>(tileVoxelCount) )
								throw std::runtime_error( "HouGeo::loadVoxelData raw tile payload size mismatch" );
						}
						else if( compression == 2 )
						{
							static_cast<void>(tile->get<float>("data"));
						}
						else
						{
							throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
								DiagnosticCategory::unsupported_input,
								"HouGeo::loadVoxelData does not support tile compression " + std::to_string(compression),
								-1, "compression"});
						}

						tileInfos.push_back(TileInfo{tile, compression, voxelOffsetX, voxelOffsetY, voxelOffsetZ,
							tileSizeX, tileSizeY, tileSizeZ, tileVoxelCount});
					});
				}
			}
		}

		std::vector<float> voxelData(voxelCount);
		for( const TileInfo &tileInfo : tileInfos )
		{
			if( tileInfo.compression == 0 || tileInfo.compression == 1 )
			{
				json::ArrayPtr data = tileInfo.tile->array("data");
				size_t sourceIndex = 0;
				for( int localZ=0;localZ<tileInfo.tileSizeZ;++localZ )
					for( int localY=0;localY<tileInfo.tileSizeY;++localY )
						for( int localX=0;localX<tileInfo.tileSizeX;++localX, ++sourceIndex )
						{
							const size_t destinationIndex = volumeIndex(tileInfo.voxelOffsetX + localX,
								tileInfo.voxelOffsetY + localY, tileInfo.voxelOffsetZ + localZ, resolution);
							if( destinationIndex >= voxelCount || sourceIndex >= tileInfo.voxelCount )
								throw std::out_of_range( "HouGeo::loadVoxelData tile index exceeds validated storage" );
							voxelData[destinationIndex] = data->get<float>(static_cast<int>(sourceIndex));
						}
			}
			else
			{
				const float constantValue = tileInfo.tile->get<float>("data");
				for( int localZ=0;localZ<tileInfo.tileSizeZ;++localZ )
					for( int localY=0;localY<tileInfo.tileSizeY;++localY )
						for( int localX=0;localX<tileInfo.tileSizeX;++localX )
						{
							const size_t destinationIndex = volumeIndex(tileInfo.voxelOffsetX + localX,
								tileInfo.voxelOffsetY + localY, tileInfo.voxelOffsetZ + localZ, resolution);
							if( destinationIndex >= voxelCount )
								throw std::out_of_range( "HouGeo::loadVoxelData tile index exceeds validated storage" );
							voxelData[destinationIndex] = constantValue;
						}
			}
		}
		return voxelData;
	}

	int HouGeo::HouVolume::topologyVertex() const
	{
		return topology_vertex_;
	}

	real32 HouGeo::HouVolume::voxelValue(int x, int y, int z) const
	{
		return scalar_field_->voxel(x, y, z);
	}

	std::string HouGeo::HouVolume::visualizationMode() const
	{
		return visualization_mode_;
	}

	real32 HouGeo::HouVolume::visualizationIso() const
	{
		return visualization_iso_;
	}

	real32 HouGeo::HouVolume::visualizationDensity() const
	{
		return visualization_density_;
	}

	math::Vec3i HouGeo::HouVolume::resolution() const
	{
		return scalar_field_->resolution();
	}

	math::M44f HouGeo::HouVolume::transform() const
	{
		return scalar_field_->localToWorldMatrix();
	}

	HouGeoAdapter::RawDataView HouGeo::HouVolume::rawData() const
	{
		if (!scalar_field_)
			return {};
		return HouGeoAdapter::RawDataView::from<real32>(scalar_field_->values());
	}


	// HouGeo::HouPoly ==================================================
	void HouGeo::loadPolyPrimitive( json::ObjectPtr polygonObject )
	{
		if( !polygonObject )
			throw std::invalid_argument( "HouGeo::loadPolyPrimitive received invalid polygon data" );
		if( !m_topology )
			throw std::runtime_error( "HouGeo::loadPolyPrimitive expects topology to be loaded already" );
		if( !polygonObject->contains("vertex") )
			throw std::runtime_error( "HouGeo::loadPolyPrimitive is missing the vertex array" );

		json::ArrayPtr topologyIndices = polygonObject->array("vertex");
		if( !topologyIndices )
			throw std::runtime_error( "HouGeo::loadPolyPrimitive vertex must be an array" );
		// These values index the topology array rather than the point domain directly.
		const int vertexCount = checkedArrayCount(topologyIndices,
			"HouGeo::loadPolyPrimitive vertex array");
		if( vertexCount <= 0 )
			throw std::runtime_error( "HouGeo::loadPolyPrimitive polygon must contain vertices" );

		HouPoly::Ptr polygonPrimitive = std::make_shared<HouPoly>();
		polygonPrimitive->m_closed = polygonObject->get<bool>("closed", true);
		polygonPrimitive->m_numPolys = 1;
		polygonPrimitive->m_perPolyVertexCount.push_back(vertexCount);
		polygonPrimitive->m_perPolyVertexListOffset.push_back(0);
		for( int vertexIndex=0;vertexIndex<vertexCount;++vertexIndex )
		{
			const int topologyIndex = topologyIndices->get<sint32>(vertexIndex);
			if( topologyIndex < 0 || static_cast<size_t>(topologyIndex) >= m_topology->indexBuffer.size() )
				throw std::runtime_error( "HouGeo::loadPolyPrimitive topology index out of range" );
			polygonPrimitive->m_vertices.push_back(m_topology->indexBuffer[static_cast<size_t>(topologyIndex)]);
		}

		m_primitives.push_back( polygonPrimitive );
	}

	void HouGeo::loadPolyPrimitiveRun( json::ObjectPtr definition, json::ArrayPtr runEntries )
	{
		if( !m_topology )
			throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun expects topology to be loaded already" );
		if( !definition || !runEntries )
			throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun received invalid run data" );

		HouPoly::Ptr polygonRun = std::make_shared<HouPoly>();
		polygonRun->m_numPolys = checkedArrayCount(runEntries,
			"HouGeo::loadPolyPrimitiveRun entries");
		if( polygonRun->m_numPolys <= 0 )
			throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun requires at least one entry" );
		polygonRun->m_closed = true;
		size_t vertexOffset = 0;
		for( int primitiveIndex=0;primitiveIndex<polygonRun->m_numPolys;++primitiveIndex )
		{
			json::ArrayPtr runEntry = runEntries->array(primitiveIndex);
			if( !runEntry || runEntry->size() == 0 )
				throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun invalid polygon entry" );
			json::ArrayPtr topologyIndices = runEntry->array(0);
			if( !topologyIndices )
				throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun missing polygon vertices" );
			const int vertexCount = checkedArrayCount(topologyIndices,
				"HouGeo::loadPolyPrimitiveRun polygon vertices");
			if( vertexCount <= 0 )
				throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun polygon must contain vertices" );
			if( vertexOffset > static_cast<size_t>(std::numeric_limits<int>::max()) )
				throw std::overflow_error( "HouGeo::loadPolyPrimitiveRun vertex offset exceeds int range" );
			polygonRun->m_perPolyVertexCount.push_back(vertexCount);
			polygonRun->m_perPolyVertexListOffset.push_back(static_cast<int>(vertexOffset));
			for( int vertexIndex=0;vertexIndex<vertexCount; ++vertexIndex, ++vertexOffset )
			{
				const int topologyIndex = topologyIndices->get<sint32>(vertexIndex);
				if( topologyIndex < 0 || static_cast<size_t>(topologyIndex) >= m_topology->indexBuffer.size() )
					throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun topology index out of range" );
				polygonRun->m_vertices.push_back(m_topology->indexBuffer[static_cast<size_t>(topologyIndex)]);
			}
		}
		m_primitives.push_back( polygonRun );
	}

	void HouGeo::loadPolygonRun( json::ObjectPtr polygonRun, bool closed )
	{
		if( !polygonRun )
			throw std::invalid_argument( "HouGeo::loadPolygonRun received invalid polygon-run data" );
		if( !m_topology )
			throw std::runtime_error( "HouGeo::loadPolygonRun expects topology to be loaded already" );

		const std::string startVertexKey = polygonRun->contains("startvertex") ? "startvertex" : "s_v";
		const std::string primitiveCountKey = polygonRun->contains("nprimitives") ? "nprimitives" : "n_p";
		const std::string runLengthKey = polygonRun->contains("nvertices_rle") ? "nvertices_rle" : "r_v";
		const std::string vertexCountsKey = polygonRun->contains("nvertices") ? "nvertices" : "n_v";
		const bool hasRunLengthData = polygonRun->contains(runLengthKey);
		const bool hasVertexCounts = polygonRun->contains(vertexCountsKey);
		if( !polygonRun->contains(startVertexKey) || !polygonRun->contains(primitiveCountKey)
			|| (!hasRunLengthData && !hasVertexCounts) )
			throw std::runtime_error( "HouGeo::loadPolygonRun missing required fields" );

		const int startVertex = polygonRun->get<int>(startVertexKey, -1);
		const int expectedPrimitiveCount = polygonRun->get<int>(primitiveCountKey, -1);
		json::ArrayPtr vertexCountData = polygonRun->array(hasRunLengthData ? runLengthKey : vertexCountsKey);
		if( startVertex < 0 || expectedPrimitiveCount <= 0 || !vertexCountData )
			throw std::runtime_error( "HouGeo::loadPolygonRun invalid run metadata" );
		if( static_cast<size_t>(startVertex) > m_topology->indexBuffer.size() )
			throw std::runtime_error( "HouGeo::loadPolygonRun start vertex exceeds topology" );
		if( hasRunLengthData && (vertexCountData->size() % 2) != 0 )
			throw std::runtime_error( "HouGeo::loadPolygonRun invalid run-length data" );
		if( !hasRunLengthData && hasVertexCounts && vertexCountData->size() != expectedPrimitiveCount )
			throw std::runtime_error( "HouGeo::loadPolygonRun vertex-count array length mismatch" );

		HouPoly::Ptr polygonRunPrimitive = std::make_shared<HouPoly>();
		polygonRunPrimitive->m_closed = polygonRun->get<bool>("closed", closed);

		size_t topologyIndex = static_cast<size_t>(startVertex);
		int primitiveCount = 0;
		auto appendPolygons = [&]( int verticesPerPrimitive, int repetitionCount )
		{
			if( verticesPerPrimitive <= 0 || repetitionCount <= 0 )
				throw std::runtime_error( "HouGeo::loadPolygonRun invalid vertex-count data" );
			if( primitiveCount > expectedPrimitiveCount
				|| repetitionCount > expectedPrimitiveCount - primitiveCount )
				throw std::runtime_error( "HouGeo::loadPolygonRun primitive count exceeds nprimitives" );

			for( int repetition=0;repetition<repetitionCount;++repetition )
			{
				const size_t verticesToConsume = static_cast<size_t>(verticesPerPrimitive);
				if( topologyIndex > m_topology->indexBuffer.size()
					|| verticesToConsume > m_topology->indexBuffer.size() - topologyIndex )
					throw std::runtime_error( "HouGeo::loadPolygonRun topology range exceeds index buffer" );
				const size_t nextTopologyIndex = topologyIndex + verticesToConsume;
				if( polygonRunPrimitive->m_vertices.size()
					> static_cast<size_t>(std::numeric_limits<int>::max()) )
					throw std::overflow_error( "HouGeo::loadPolygonRun vertex offset exceeds int range" );

				polygonRunPrimitive->m_perPolyVertexListOffset.push_back(
					static_cast<int>(polygonRunPrimitive->m_vertices.size()));
				polygonRunPrimitive->m_perPolyVertexCount.push_back(verticesPerPrimitive);
				for( ;topologyIndex<nextTopologyIndex;++topologyIndex )
					polygonRunPrimitive->m_vertices.push_back(m_topology->indexBuffer[topologyIndex]);
				++primitiveCount;
			}
		};

		if( hasRunLengthData )
		{
			for( sint64 i=0;i<vertexCountData->size();i+=2 )
				appendPolygons(
					vertexCountData->get<int>(static_cast<int>(i)),
					vertexCountData->get<int>(static_cast<int>(i + 1)));
		}
		else
		{
			for( sint64 i=0;i<vertexCountData->size();++i )
				appendPolygons(vertexCountData->get<int>(static_cast<int>(i)), 1);
		}

		if( primitiveCount != expectedPrimitiveCount )
			throw std::runtime_error( "HouGeo::loadPolygonRun primitive count mismatch" );

		polygonRunPrimitive->m_numPolys = primitiveCount;
		m_primitives.push_back( polygonRunPrimitive );
	}

	int HouGeo::HouPoly::polygonCount() const
	{
		return m_numPolys;
	}

	int HouGeo::HouPoly::polygonVertexCount(int polygon_index) const
	{
		if (polygon_index < 0 || polygon_index >= m_numPolys
			|| static_cast<size_t>(polygon_index) >= m_perPolyVertexCount.size())
			throw std::out_of_range("HouPoly polygon index is out of range");
		const int vertex_count = m_perPolyVertexCount[static_cast<size_t>(polygon_index)];
		if (vertex_count < 0)
			throw std::runtime_error("HouPoly polygon has a negative vertex count");
		return vertex_count;
	}

	std::span<const int> HouGeo::HouPoly::polygonVertexIndices(int polygon_index) const
	{
		const int vertex_count = polygonVertexCount(polygon_index);
		if (static_cast<size_t>(polygon_index) >= m_perPolyVertexListOffset.size())
			throw std::runtime_error("HouPoly polygon offset table is incomplete");
		const int offset = m_perPolyVertexListOffset[static_cast<size_t>(polygon_index)];
		if (offset < 0)
			throw std::runtime_error("HouPoly polygon has a negative vertex offset");
		const size_t offset_value = static_cast<size_t>(offset);
		const size_t count_value = static_cast<size_t>(vertex_count);
		if (offset_value > m_vertices.size() || count_value > m_vertices.size() - offset_value)
			throw std::runtime_error("HouPoly polygon range exceeds stored vertices");
		return std::span<const int>(m_vertices).subspan(offset_value, count_value);
	}

	bool HouGeo::HouPoly::isClosed() const
	{
		return m_closed;
	}



	// MISC =======================================================

	json::ObjectPtr HouGeo::toObject( json::ArrayPtr flattenedArray )
	{
		if( !flattenedArray )
			throw std::runtime_error( "HouGeo::toObject received a null array" );
		if( (flattenedArray->size() % 2) != 0 )
			throw std::runtime_error( "HouGeo::toObject requires an even-length key/value array" );

		json::ObjectPtr object = json::Object::create();
		const int elementCount = checkedArrayCount(flattenedArray,
			"HouGeo::toObject flattened array");
		for( int keyIndex=0;keyIndex<elementCount;keyIndex+=2 )
		{
			if( !flattenedArray->value(keyIndex).isString() )
				throw std::runtime_error( "HouGeo::toObject requires string keys" );

			const std::string key = flattenedArray->get<std::string>(keyIndex);
			if( object->contains(key) )
				throw std::runtime_error( "HouGeo::toObject encountered duplicate key " + key );
			object->append(key, flattenedArray->value(keyIndex + 1));
		}

		return object;
	}








	




}  // namespace houio
