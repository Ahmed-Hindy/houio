#include <houio/HouGeo.h>

#include <cstring>
#include <limits>




namespace houio
{
	namespace
	{
		std::vector<int> expandPagedIntValues( json::ObjectPtr values, sint64 elementCount, const std::string& attributeName )
		{
			if( !values )
				throw std::runtime_error( "HouGeo::loadAttribute missing integer value object for attribute " + attributeName );
			if( values->get<int>("size", 1) != 1 )
				throw std::runtime_error( "HouGeo::loadAttribute expected scalar integer values for attribute " + attributeName );

			std::vector<int> result;
			result.reserve(static_cast<size_t>(elementCount));

			if( values->hasKey("arrays") )
			{
				json::ArrayPtr arrays = values->getArray("arrays");
				if( !arrays || arrays->size() != 1 )
					throw std::runtime_error( "HouGeo::loadAttribute invalid integer component arrays for attribute " + attributeName );

				json::ArrayPtr data = arrays->getArray(0);
				if( !data || data->size() != elementCount )
					throw std::runtime_error( "HouGeo::loadAttribute integer value count mismatch for attribute " + attributeName );
				for( sint64 index=0;index<elementCount;++index )
					result.push_back(data->get<int>(static_cast<int>(index)));
				return result;
			}

			if( !values->hasKey("rawpagedata") )
				throw std::runtime_error( "HouGeo::loadAttribute missing integer payload for attribute " + attributeName );

			json::ArrayPtr rawPageData = values->getArray("rawpagedata");
			if( !rawPageData )
				throw std::runtime_error( "HouGeo::loadAttribute invalid integer payload for attribute " + attributeName );

			if( !values->hasKey("constantpageflags") )
			{
				if( rawPageData->size() != elementCount )
					throw std::runtime_error( "HouGeo::loadAttribute integer value count mismatch for attribute " + attributeName );
				for( sint64 index=0;index<elementCount;++index )
					result.push_back(rawPageData->get<int>(static_cast<int>(index)));
				return result;
			}

			const int elementsPerPage = values->get<int>("pagesize", 0);
			if( elementsPerPage <= 0 )
				throw std::runtime_error( "HouGeo::loadAttribute invalid page size for attribute " + attributeName );

			json::ArrayPtr flagsPerPack = values->getArray("constantpageflags");
			if( !flagsPerPack || flagsPerPack->size() != 1 )
				throw std::runtime_error( "HouGeo::loadAttribute invalid constant page flags for attribute " + attributeName );
			json::ArrayPtr pageFlags = flagsPerPack->getArray(0);
			const sint64 expectedPageCount = (elementCount + elementsPerPage - 1) / elementsPerPage;
			if( !pageFlags || pageFlags->size() != expectedPageCount )
				throw std::runtime_error( "HouGeo::loadAttribute constant page flag count mismatch for attribute " + attributeName );

			sint64 dataIndex = 0;
			sint64 elementsRemaining = elementCount;
			for( sint64 pageIndex=0;pageIndex<expectedPageCount;++pageIndex )
			{
				const sint64 pageElementCount = std::min<sint64>(elementsRemaining, elementsPerPage);
				const bool constantPage = pageFlags->get<bool>(static_cast<int>(pageIndex));
				if( constantPage )
				{
					if( dataIndex >= rawPageData->size() )
						throw std::runtime_error( "HouGeo::loadAttribute constant page payload underrun for attribute " + attributeName );
					const int value = rawPageData->get<int>(static_cast<int>(dataIndex++));
					result.insert(result.end(), static_cast<size_t>(pageElementCount), value);
				}
				else
				{
					if( dataIndex + pageElementCount > rawPageData->size() )
						throw std::runtime_error( "HouGeo::loadAttribute varying page payload underrun for attribute " + attributeName );
					for( sint64 pageOffset=0;pageOffset<pageElementCount;++pageOffset )
						result.push_back(rawPageData->get<int>(static_cast<int>(dataIndex++)));
				}
				elementsRemaining -= pageElementCount;
			}

			if( dataIndex != rawPageData->size() || result.size() != static_cast<size_t>(elementCount) )
				throw std::runtime_error( "HouGeo::loadAttribute integer page expansion mismatch for attribute " + attributeName );
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

		void storeNumericComponent( char *data, size_t destinationIndex,
			HouGeoAdapter::AttributeAdapter::Storage storage, const json::Value &value )
		{
			if( !data )
				throw std::invalid_argument( "Numeric attribute storage is null" );
			switch( storage )
			{
			case HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL16:
			{
				const uword converted = floatToHalfBits(value.as<real32>());
				std::memcpy(data + destinationIndex * sizeof(converted), &converted, sizeof(converted));
				break;
			}
			case HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32:
			{
				const real32 converted = value.as<real32>();
				std::memcpy(data + destinationIndex * sizeof(converted), &converted, sizeof(converted));
				break;
			}
			case HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64:
			{
				const real64 converted = value.as<real64>();
				std::memcpy(data + destinationIndex * sizeof(converted), &converted, sizeof(converted));
				break;
			}
			case HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32:
			{
				const sint32 converted = value.as<sint32>();
				std::memcpy(data + destinationIndex * sizeof(converted), &converted, sizeof(converted));
				break;
			}
			case HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT64:
			{
				const sint64 converted = value.as<sint64>();
				std::memcpy(data + destinationIndex * sizeof(converted), &converted, sizeof(converted));
				break;
			}
			default:
				throw std::runtime_error( "Unsupported numeric attribute storage" );
			}
		}
	}

	HouGeo::HouGeo() :
		HouGeoAdapter()
	{
	}

	sint64 HouGeo::pointcount()const
	{
		if( m_pointCount >= 0 )
			return m_pointCount;
		for( const auto &entry : m_pointAttributes )
		{
			if( entry.second )
				return entry.second->getNumElements();
		}
		return 0;
	}

	sint64 HouGeo::vertexcount()const
	{
		if( m_topology )
			return m_topology->getNumIndices();
		return 0;
	}

	sint64 HouGeo::primitivecount()const
	{
		// One stored primitive may represent multiple Houdini primitives, such as a polygon run.
		sint64 primitiveCount = 0;
		for( const auto &primitive : m_primitives )
			primitiveCount += primitive->numPrimitives();
		return primitiveCount;
	}


	void HouGeo::getPointAttributeNames( std::vector<std::string> &names )const
	{
		for( auto it = m_pointAttributes.cbegin(); it != m_pointAttributes.cend(); ++it )
			names.push_back( it->second->getName() );
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::getPointAttribute( const std::string &name )
	{
		auto it = m_pointAttributes.find(name);
		if(it != m_pointAttributes.end())
			return it->second;
		return HouGeoAdapter::AttributeAdapter::Ptr();
	}

	void HouGeo::getVertexAttributeNames( std::vector<std::string> &names )const
	{
		for( auto it = m_vertexAttributes.cbegin(); it != m_vertexAttributes.cend(); ++it )
			names.push_back( it->second->getName() );
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::getVertexAttribute( const std::string &name )
	{
		auto it = m_vertexAttributes.find(name);
		if(it != m_vertexAttributes.end())
			return it->second;
		return HouGeoAdapter::AttributeAdapter::Ptr();
	}


	void HouGeo::getGlobalAttributeNames( std::vector<std::string> &names )const
	{
		for( auto it = m_globalAttributes.cbegin(); it != m_globalAttributes.cend(); ++it )
			names.push_back( it->second->getName() );
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::getGlobalAttribute( const std::string &name )
	{
		auto it = m_globalAttributes.find(name);
		if(it != m_globalAttributes.end())
			return it->second;
		return HouGeoAdapter::AttributeAdapter::Ptr();
	}

	void HouGeo::getPointGroupNames( std::vector<std::string> &names )const
	{
		names.clear();
		for( const auto &entry : m_pointGroups )
			names.push_back(entry.first);
	}

	bool HouGeo::getPointGroupMembership( const std::string &name, std::vector<bool> &membership )const
	{
		auto it = m_pointGroups.find(name);
		if( it == m_pointGroups.end() )
		{
			membership.clear();
			return false;
		}
		membership = it->second;
		return true;
	}

	void HouGeo::getVertexGroupNames( std::vector<std::string> &names )const
	{
		names.clear();
		for( const auto &entry : m_vertexGroups )
			names.push_back(entry.first);
	}

	bool HouGeo::getVertexGroupMembership( const std::string &name, std::vector<bool> &membership )const
	{
		auto it = m_vertexGroups.find(name);
		if( it == m_vertexGroups.end() )
		{
			membership.clear();
			return false;
		}
		membership = it->second;
		return true;
	}

	void HouGeo::getPrimitiveGroupNames( std::vector<std::string> &names )const
	{
		names.clear();
		for( const auto &entry : m_primitiveGroups )
			names.push_back(entry.first);
	}

	bool HouGeo::getPrimitiveGroupMembership( const std::string &name, std::vector<bool> &membership )const
	{
		auto it = m_primitiveGroups.find(name);
		if( it == m_primitiveGroups.end() )
		{
			membership.clear();
			return false;
		}
		membership = it->second;
		return true;
	}


	bool HouGeo::hasPrimitiveAttribute( const std::string &name )const
	{
		auto it = m_primitiveAttributes.find( name );
		if( it != m_primitiveAttributes.end() )
			return true;
		return false;
	}

	void HouGeo::getPrimitiveAttributeNames( std::vector<std::string> &names )const
	{
		for( auto it = m_primitiveAttributes.cbegin(); it != m_primitiveAttributes.cend(); ++it )
			names.push_back( it->second->getName() );
	}

	HouGeoAdapter::AttributeAdapter::Ptr HouGeo::getPrimitiveAttribute( const std::string &name )
	{
		auto it = m_primitiveAttributes.find(name);
		if(it != m_primitiveAttributes.end())
			return it->second;
		return HouGeoAdapter::AttributeAdapter::Ptr();
	}

	void HouGeo::getPrimitives( std::vector<HouGeoAdapter::Primitive::Ptr>& primitives )
	{
		primitives = m_primitives;
	}

	HouGeo::Topology::Ptr HouGeo::getTopology()
	{
		return m_topology;
	}


	HouGeo::Ptr HouGeo::create()
	{
		return HouGeo::Ptr( new HouGeo() );
	}

	void HouGeo::addPrimitive( ScalarField::Ptr field )
	{
		if( !field )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null field" );

		// Houdini encodes the volume translation through a topology vertex referencing P.
		HouAttribute::Ptr positionAttribute = std::dynamic_pointer_cast<HouAttribute>(getPointAttribute( "P" ));
		if( !positionAttribute )
		{
			positionAttribute = std::make_shared<HouAttribute>();
			positionAttribute->m_name = "P";
			positionAttribute->tupleSize = 4;
			positionAttribute->m_storage = HouAttribute::ATTR_STORAGE_FPREAL32;
			positionAttribute->m_type = HouAttribute::ATTR_TYPE_NUMERIC;
			positionAttribute->m_attr = Attribute::createV4f();
			setPointAttribute( positionAttribute );
		}

		const math::V3f center = field->localToWorld( math::V3f(0.5f) );
		const int pointIndex = positionAttribute->m_attr->appendElement<math::V4f>(
			math::V4f(center.x, center.y, center.z, 1.0f));
		if( m_pointCount >= 0 )
			++m_pointCount;

		if( !m_topology )
			m_topology = std::make_shared<HouTopology>();

		HouVolume::Ptr volumePrimitive = std::make_shared<HouVolume>();
		volumePrimitive->field = field;

		std::vector<int> pointIndices{pointIndex};
		const sint64 topologyVertex = m_topology->getNumIndices();
		if( topologyVertex > static_cast<sint64>(std::numeric_limits<int>::max()) )
			throw std::overflow_error( "HouGeo volume topology index exceeds int range" );
		volumePrimitive->vertex = static_cast<int>(topologyVertex);
		m_topology->addIndices(pointIndices);

		m_primitives.push_back( volumePrimitive );
	}

	void HouGeo::addPrimitive( PolyPrimitive::Ptr poly )
	{
		if( !poly )
			throw std::invalid_argument( "HouGeo::addPrimitive received a null polygon" );
		m_primitives.push_back(poly);
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
		if( attr->getName().empty() )
			throw std::invalid_argument( "HouGeo::setPointAttribute requires a non-empty name" );
		if( m_pointCount >= 0 && attr->getNumElements() != m_pointCount )
			throw std::invalid_argument( "HouGeo::setPointAttribute element count does not match pointcount" );
		m_pointAttributes[attr->getName()] = attr;
	}

	void HouGeo::setPrimitiveAttribute( const std::string &name, HouAttribute::Ptr attr )
	{
		if( !attr )
			throw std::invalid_argument( "HouGeo::setPrimitiveAttribute received a null attribute" );
		if( name.empty() )
			throw std::invalid_argument( "HouGeo::setPrimitiveAttribute requires a non-empty name" );
		attr->m_name = name;
		m_primitiveAttributes[name] = attr;
	}

	void HouGeo::setPointGroup( const std::string &name, const std::vector<bool> &membership )
	{
		m_pointGroups[name] = membership;
	}

	void HouGeo::setVertexGroup( const std::string &name, const std::vector<bool> &membership )
	{
		m_vertexGroups[name] = membership;
	}

	void HouGeo::setPrimitiveGroup( const std::string &name, const std::vector<bool> &membership )
	{
		m_primitiveGroups[name] = membership;
	}

	// Attribute ==============================

	HouGeo::HouAttribute::HouAttribute()
		: AttributeAdapter(),
		  m_name("unnamed"),
		  tupleSize(1),
		  m_storage(HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INVALID),
		  m_type(HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC),
		  numElements(0)
	{
	}

	HouGeo::HouAttribute::HouAttribute( const std::string &name, Attribute::Ptr attr ) : AttributeAdapter(),
		m_name(name),
		tupleSize(attr->numComponents()),
		m_storage(ATTR_STORAGE_FPREAL32),
		m_type(HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC),
		numElements(attr->numElements()),
		m_attr(attr)
	{
		switch( m_attr->elementComponentType() )
		{
		case Attribute::FLOAT:
			m_storage = ATTR_STORAGE_FPREAL32;break;
		case Attribute::HALF:
			m_storage = ATTR_STORAGE_FPREAL16;break;
		//case ::Attribute::REAL64:
		//	m_storage = ATTR_STORAGE_FPREAL64;break;
		case Attribute::INT:
			m_storage = ATTR_STORAGE_INT32;break;
		case Attribute::INT64:
			m_storage = ATTR_STORAGE_INT64;break;
		default:
			throw std::runtime_error("HouGeo::HouAttribute::HouAttribute - unsupported attribute type");
		}
	}

	std::string HouGeo::HouAttribute::getName()const
	{
		return m_name;
	}

	HouGeoAdapter::AttributeAdapter::Type HouGeo::HouAttribute::getType()const
	{
		return m_type;
	}

	int HouGeo::HouAttribute::getTupleSize()const
	{
		if( m_attr )
			return m_attr->numComponents();
		return tupleSize;
	}

	HouGeoAdapter::AttributeAdapter::Storage HouGeo::HouAttribute::getStorage()const
	{
		return m_storage;
	}

	void HouGeo::HouAttribute::getPacking( std::vector<int> & )const
	{
	}

	HouGeoAdapter::RawPointer::Ptr HouGeo::HouAttribute::getRawPointer()
	{
		if( m_attr )
			return HouGeoAdapter::RawPointer::create( m_attr->getRawPointer() );
		//if( !data.empty() )
		//	return HouGeoAdapter::RawPointer::create( &data[0] );
		return HouGeoAdapter::RawPointer::create( 0 );
	}

	int HouGeo::HouAttribute::getNumElements()const
	{
		if( m_attr )
			return m_attr->numElements();
		return numElements;
	}


	/*
	int HouGeo::HouAttribute::addV4f( math::V4f value )
	{
		// TODO: check storage
		// TODO: check type

		if( tupleSize != 4 )
			qCritical() << "tupleSize does not match!";

		int elementSize = storageSize( m_storage )*tupleSize;
		data.resize( data.size() + elementSize );

		*((math::V4f *)(&data[ numElements * elementSize ])) = value;

		return numElements++;
	}
	*/

	int HouGeo::HouAttribute::addString(const std::string &value)
	{
		// TODO: check storage
		// TODO: check type
		strings.push_back(value);
		m_type = ATTR_TYPE_STRING;
		m_storage = ATTR_STORAGE_INT32;
		tupleSize = 1;
		return numElements++;
	}

	std::string HouGeo::HouAttribute::getString( int index )const
	{
		if( index < 0 || static_cast<size_t>(index) >= strings.size() )
			throw std::out_of_range( "HouAttribute string index is out of range" );
		return strings[static_cast<size_t>(index)];
	}




	// Topology ==============================

	HouGeo::HouTopology::HouTopology() :
		Topology(),
		indexBuffer()
	{
	}

	void HouGeo::HouTopology::getIndices( std::vector<int> &indices )const
	{
		indices.clear();
		indices.insert( indices.begin(), indexBuffer.begin(), indexBuffer.end() );
	}

	void HouGeo::HouTopology::addIndices( std::vector<int> &indices )
	{
		indexBuffer.insert( indexBuffer.end(), indices.begin(), indices.end() );
	}

	sint64 HouGeo::HouTopology::getNumIndices()const
	{
		if( indexBuffer.size() > static_cast<size_t>(std::numeric_limits<sint64>::max()) )
			throw std::overflow_error( "HouTopology index count exceeds sint64 range" );
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
		if( rootObject->hasKey("pointcount") )
			pointCount = rootObject->get<int>("pointcount", 0);
		if( rootObject->hasKey("vertexcount") )
			vertexCount = rootObject->get<int>("vertexcount", 0);
		if( rootObject->hasKey("primitivecount") )
			primitiveCount = rootObject->get<int>("primitivecount", 0);
		if( pointCount < 0 || vertexCount < 0 || primitiveCount < 0 )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::load received negative geometry counts", -1, "counts"});
		m_pointCount = pointCount;
		if( rootObject->hasKey("attributes") )
		{
			json::ObjectPtr attributes = toObject(rootObject->getArray("attributes"));
			if( attributes->hasKey("pointattributes") )
			{
				json::ArrayPtr pointAttributes = attributes->getArray("pointattributes");
				const sint64 pointAttributeCount = pointAttributes->size();
				for( sint64 attributeIndex=0;attributeIndex<pointAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.pointattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(pointAttributes->getArray(static_cast<int>(attributeIndex)), pointCount);
					});
					m_pointAttributes.emplace(attribute->getName(), attribute);
				}
			}
			if( attributes->hasKey("vertexattributes") )
			{
				json::ArrayPtr vertexAttributes = attributes->getArray("vertexattributes");
				const sint64 vertexAttributeCount = vertexAttributes->size();
				for( sint64 attributeIndex=0;attributeIndex<vertexAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.vertexattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(vertexAttributes->getArray(static_cast<int>(attributeIndex)), vertexCount);
					});
					m_vertexAttributes.emplace(attribute->getName(), attribute);
				}
			}
			if( attributes->hasKey("primitiveattributes") )
			{
				json::ArrayPtr primitiveAttributes = attributes->getArray("primitiveattributes");
				const sint64 primitiveAttributeCount = primitiveAttributes->size();
				for( sint64 attributeIndex=0;attributeIndex<primitiveAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.primitiveattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(primitiveAttributes->getArray(static_cast<int>(attributeIndex)), primitiveCount);
					});
					m_primitiveAttributes.emplace(attribute->getName(), attribute);
				}
			}
			if( attributes->hasKey("globalattributes") )
			{
				json::ArrayPtr globalAttributes = attributes->getArray("globalattributes");
				const sint64 globalAttributeCount = globalAttributes->size();
				for( sint64 attributeIndex=0;attributeIndex<globalAttributeCount;++attributeIndex )
				{
					HouAttribute::Ptr attribute;
					withSchemaPath("attributes.globalattributes[" + std::to_string(attributeIndex) + "]", [&]()
					{
						attribute = loadAttribute(globalAttributes->getArray(static_cast<int>(attributeIndex)), 1);
					});
					m_globalAttributes.emplace(attribute->getName(), attribute);
				}
			}
		}
		if( rootObject->hasKey("topology") )
		{
			withSchemaPath("topology", [&]()
			{
				loadTopology(toObject(rootObject->getArray("topology")), pointCount);
				if( m_topology->getNumIndices() != vertexCount )
					throw std::runtime_error( "HouGeo::load topology count does not match vertexcount" );
			});
		}
		else if( vertexCount != 0 )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::load missing topology for non-empty vertex domain", -1, "topology"});
		}
		if( rootObject->hasKey("sharedprimitivedata") )
		{
			json::ArrayPtr entries = rootObject->getArray("sharedprimitivedata");
			const int entryCount = static_cast<int>(entries->size() / 2);
			for( int entryIndex=0;entryIndex<entryCount;++entryIndex )
			{
				const int recordOffset = entryIndex * 2;
				[[maybe_unused]] const std::string entryType = entries->get<std::string>(recordOffset);
				json::ArrayPtr entry = entries->getArray(recordOffset + 1);

				[[maybe_unused]] const std::string dataType = entry->get<std::string>(0);
				const std::string dataId = entry->get<std::string>(1);
				json::ArrayPtr voxelData = entry->getArray(2);

				sharedPrimitiveData.sharedVoxelData[dataId] = toObject(voxelData);
			}
		}
		if( rootObject->hasKey("primitives") )
		{
			json::ArrayPtr primitives = rootObject->getArray("primitives");
			const int primitiveRecordCount = static_cast<int>(primitives->size());
			for( int primitiveIndex=0;primitiveIndex<primitiveRecordCount;++primitiveIndex )
			{
				withSchemaPath("primitives[" + std::to_string(primitiveIndex) + "]", [&]()
				{
					loadPrimitive(primitives->getArray(primitiveIndex), sharedPrimitiveData);
				});
			}
		}
		if( rootObject->hasKey("pointgroups") )
			withSchemaPath("pointgroups", [&]() { loadGroups(rootObject->getArray("pointgroups"), pointCount, m_pointGroups); });
		if( rootObject->hasKey("vertexgroups") )
			withSchemaPath("vertexgroups", [&]() { loadGroups(rootObject->getArray("vertexgroups"), vertexCount, m_vertexGroups); });
		if( rootObject->hasKey("primitivegroups") )
			withSchemaPath("primitivegroups", [&]() { loadGroups(rootObject->getArray("primitivegroups"), primitiveCount, m_primitiveGroups); });
	}


	void HouGeo::loadGroups( json::ArrayPtr groups, sint64 elementCount,
		std::map<std::string, std::vector<bool>> &destination )
	{
		if( !groups || elementCount < 0 )
			throw std::runtime_error( "HouGeo::loadGroups received invalid group data" );

		for( sint64 groupIndex=0;groupIndex<groups->size();++groupIndex )
		{
			json::ArrayPtr group = groups->getArray(static_cast<int>(groupIndex));
			if( !group || group->size() != 2 )
				throw std::runtime_error( "HouGeo::loadGroups expected definition and data arrays" );

			json::ObjectPtr definition = toObject(group->getArray(0));
			json::ObjectPtr data = toObject(group->getArray(1));
			const std::string name = definition->get<std::string>("name", "");
			if( name.empty() )
				throw std::runtime_error( "HouGeo::loadGroups encountered a group without a name" );
			if( destination.find(name) != destination.end() )
				throw std::runtime_error( "HouGeo::loadGroups encountered duplicate group " + name );
			if( !data->hasKey("selection") )
				throw std::runtime_error( "HouGeo::loadGroups missing selection for group " + name );

			json::ArrayPtr selection = data->getArray("selection");
			if( !selection || selection->size() != 2 || !selection->getValue(0).isString() )
				throw std::runtime_error( "HouGeo::loadGroups received malformed selection data for group " + name );
			if( selection->get<std::string>(0) != "unordered" )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeo::loadGroups supports only unordered selections for group " + name, -1, "selection"});

			json::ArrayPtr encodedMembership = selection->getArray(1);
			if( !encodedMembership || encodedMembership->size() != 2
				|| !encodedMembership->getValue(0).isString() )
				throw std::runtime_error( "HouGeo::loadGroups received malformed membership data for group " + name );
			if( encodedMembership->get<std::string>(0) != "i8" )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeo::loadGroups requires i8 membership encoding for group " + name, -1, "selection.encoding"});

			json::ArrayPtr membershipValues = encodedMembership->getArray(1);
			if( !membershipValues || membershipValues->size() != elementCount )
				throw std::runtime_error( "HouGeo::loadGroups membership count mismatch for group " + name );

			std::vector<bool> membership(static_cast<size_t>(elementCount), false);
			for( sint64 elementIndex=0;elementIndex<elementCount;++elementIndex )
			{
				const int value = membershipValues->get<int>(static_cast<int>(elementIndex));
				if( value != 0 && value != 1 )
					throw std::runtime_error( "HouGeo::loadGroups membership must contain only zero or one for group " + name );
				membership[static_cast<size_t>(elementIndex)] = value != 0;
			}
			destination.emplace(name, std::move(membership));
		}
	}

	HouGeo::HouAttribute::Ptr HouGeo::loadAttribute( json::ArrayPtr attribute, sint64 elementCount )
	{
		json::ObjectPtr attrDef = toObject(attribute->getArray(0));
		json::ObjectPtr attrData = toObject(attribute->getArray(1));

		HouGeo::HouAttribute::Ptr attr = std::make_shared<HouGeo::HouAttribute>();

		std::string attrName = attrDef->get<std::string>("name");
		AttributeAdapter::Type attrType = AttributeAdapter::type(attrDef->get<std::string>("type"));

		if( attrType == AttributeAdapter::ATTR_TYPE_NUMERIC )
		{
			AttributeAdapter::Storage attrStorage = AttributeAdapter::storage(attrData->get<std::string>("storage"));
			int attrTupleSize = attrData->get<int>("size");

			Attribute::ComponentType attrComponentType = Attribute::componentType(attrData->get<std::string>("storage"));
			int attrNumComponents = attrData->get<int>("size");
			attr->m_attr = std::make_shared<Attribute>( attrNumComponents, attrComponentType );
			attr->m_attr->resize(elementCount);
			char *data = (char*)attr->m_attr->getRawPointer();

			const int attrComponentSize = AttributeAdapter::storageSize(attrStorage);
			if( attrStorage == AttributeAdapter::ATTR_STORAGE_INVALID || attrComponentSize <= 0 )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
					"HouGeo::loadAttribute does not support storage " + attrData->get<std::string>("storage"),
					-1, "storage"});
			const int dstTupleSize = attrTupleSize;
			attr->m_name = attrName;
			attr->m_type = attrType;
			attr->m_storage = attrStorage;
			attr->tupleSize = dstTupleSize;
			attr->numElements = static_cast<int>(elementCount);

			if( attrData->hasKey("values") )
			{
				json::ObjectPtr values = toObject( attrData->getArray("values") );
				if( values->hasKey("tuples") )
				{
					json::ArrayPtr tuples = values->getArray("tuples");
					if( !tuples || tuples->size() != elementCount )
						throw std::runtime_error( "HouGeo::loadAttribute tuple count mismatch for attribute " + attrName );

					for( sint64 elementIndex=0;elementIndex<elementCount;++elementIndex )
					{
						json::ArrayPtr tuple = tuples->getArray(static_cast<int>(elementIndex));
						if( !tuple || tuple->size() != attrTupleSize )
							throw std::runtime_error( "HouGeo::loadAttribute tuple size mismatch for attribute " + attrName );

						for( int componentIndex=0;componentIndex<attrTupleSize;++componentIndex )
						{
							const size_t destinationIndex = static_cast<size_t>(elementIndex)
								* static_cast<size_t>(attrTupleSize) + static_cast<size_t>(componentIndex);
							storeNumericComponent(data, destinationIndex, attrStorage, tuple->getValue(componentIndex));
						}
					}
				}
				else if( values->hasKey("arrays") )
				{
					json::ArrayPtr componentArrays = values->getArray("arrays");
					if( !componentArrays || componentArrays->size() != attrTupleSize )
						throw std::runtime_error( "HouGeo::loadAttribute component array count mismatch for attribute " + attrName );

					for( int componentIndex=0;componentIndex<attrTupleSize;++componentIndex )
					{
						json::ArrayPtr componentValues = componentArrays->getArray(componentIndex);
						if( !componentValues || componentValues->size() != elementCount )
							throw std::runtime_error( "HouGeo::loadAttribute component value count mismatch for attribute " + attrName );

						for( sint64 elementIndex=0;elementIndex<elementCount;++elementIndex )
						{
							const size_t destinationIndex = static_cast<size_t>(elementIndex)
								* static_cast<size_t>(attrTupleSize) + static_cast<size_t>(componentIndex);
							storeNumericComponent(data, destinationIndex, attrStorage,
								componentValues->getValue(static_cast<int>(elementIndex)));
						}
					}
				}
				else if( values->hasKey("rawpagedata") )
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
					if( values->hasKey("packing") )
					{
						json::ArrayPtr packingArray = values->getArray("packing");
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
					if( values->hasKey("constantpageflags") )
					{
						json::ArrayPtr constantPageFlags = values->getArray("constantpageflags");
						if( !constantPageFlags || constantPageFlags->size() != static_cast<sint64>(attrPacking.size()) )
							throw std::runtime_error( "HouGeo::loadAttribute constantpageflags pack count mismatch for attribute " + attrName );

						for( size_t packIndex=0;packIndex<attrPacking.size();++packIndex )
						{
							json::ArrayPtr packConstantFlags = constantPageFlags->getArray(static_cast<int>(packIndex));
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

					json::ArrayPtr rawPageData = values->getArray("rawpagedata");
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
					attr->numElements = static_cast<int>(elementCount);
					size_t elementsRemaining = static_cast<size_t>(attr->numElements);
					//qDebug() << "numElements " << attr->numElements;
					//qDebug() << "rawPageData->size() " << (int)rawPageData->size();
					//qDebug() << "attrTupleSize " << attrTupleSize;

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
							//qDebug() << "constant? " << isConstant;


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
									//qDebug() << "elementIndex " << elementIndex;
									//qDebug() << "pageStartElement " << pageStartElement;
									//qDebug() << "attrTupleSize " << attrTupleSize;
									//qDebug() << "i " << i;
									//qDebug() << "pack " << pack;

								// get global element index for writing into our dense array
								const size_t destElementIndex = (pageStartElement + i) * static_cast<size_t>(dstTupleSize);

								// for each component of current pack
								for( size_t component=0;component<maxPack;++component )
								{
									// get component value from current rawpagedata
									// and copy that component to the location of that component in dense array
									// TODO: uniform arrays!
									const size_t rawIndex = elementIndex + component;
									if( rawIndex > static_cast<size_t>(std::numeric_limits<int>::max()) )
										throw std::overflow_error( "HouGeo::loadAttribute raw page index exceeds int range for attribute " + attrName );
									storeNumericComponent(data, destElementIndex + startComponentIndex + component,
										attrStorage, rawPageData->getValue(static_cast<int>(rawIndex)));
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
		if( attrType == AttributeAdapter::ATTR_TYPE_STRING )
		{
			if( !attrData->hasKey("strings") )
				throw std::runtime_error( "HouGeo::loadAttribute missing string table for attribute " + attrName );

			json::ArrayPtr stringsArray = attrData->getArray("strings");
			std::vector<std::string> stringTable;
			stringTable.reserve(static_cast<size_t>(stringsArray->size()));
			for( sint64 i=0;i<stringsArray->size();++i )
				stringTable.push_back(stringsArray->get<std::string>(static_cast<int>(i)));

			attr->m_name = attrName;
			attr->m_type = attrType;
			attr->m_storage = AttributeAdapter::ATTR_STORAGE_INT32;
			attr->tupleSize = 1;

			if( attrData->hasKey("indices") )
			{
				json::ObjectPtr indices = toObject(attrData->getArray("indices"));
				const std::vector<int> indexValues = expandPagedIntValues(indices, elementCount, attrName);

				attr->strings.reserve(static_cast<size_t>(elementCount));
				for( int stringIndex : indexValues )
				{
					if( stringIndex < 0 || static_cast<size_t>(stringIndex) >= stringTable.size() )
						throw std::runtime_error( "HouGeo::loadAttribute string index out of range for attribute " + attrName );
					attr->strings.push_back(stringTable[static_cast<size_t>(stringIndex)]);
				}
			}
			else if( stringTable.size() == static_cast<size_t>(elementCount) )
				attr->strings = stringTable;
			else if( stringTable.size() == 1 && elementCount > 0 )
				attr->strings.assign(static_cast<size_t>(elementCount), stringTable.front());
			else if( elementCount == 0 )
				attr->strings.clear();
			else
				throw std::runtime_error( "HouGeo::loadAttribute cannot map string table to elements for attribute " + attrName );

			attr->numElements = static_cast<int>(attr->strings.size());
		}

		return attr;
	}


	void HouGeo::loadTopology( json::ObjectPtr topologyObject, sint64 pointCount )
	{
		if( !topologyObject || pointCount < 0 )
			throw std::runtime_error( "HouGeo::loadTopology received invalid topology metadata" );

		HouTopology::Ptr topology = std::make_shared<HouTopology>();
		if( topologyObject->hasKey("pointref") )
		{
			json::ObjectPtr pointReferences = toObject( topologyObject->getArray("pointref") );
			if( pointReferences->hasKey("indices") )
			{
				json::ArrayPtr indices = pointReferences->getArray("indices");
				if( !indices )
					throw std::runtime_error( "HouGeo::loadTopology missing index array" );
				const sint64 indexCount = indices->size();
				for( sint64 indexPosition=0;indexPosition<indexCount;++indexPosition )
				{
					const int pointIndex = indices->get<int>(static_cast<int>(indexPosition));
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

		json::ObjectPtr definition = toObject(primitive->getArray(0));
		std::string primitiveType;
		if( definition->hasKey("type") )
			primitiveType = definition->get<std::string>("type", "");
		if( primitiveType.empty() )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadPrimitive missing primitive type", -1, "definition.type"});

		// primitive
		if( primitiveType=="Volume" )
			withSchemaPath("data", [&]() { loadVolumePrimitive(toObject(primitive->getArray(1)), sharedPrimitiveData); });
		else
		if( primitiveType=="Poly" )
			loadPolyPrimitive( toObject(primitive->getArray(1)) );
		else
		if( primitiveType=="Polygon_run" || primitiveType=="p_r" )
			loadPolygonRun( toObject(primitive->getArray(1)), true );
		else
		if( primitiveType=="PolygonCurve_run" || primitiveType=="c_r" )
			loadPolygonRun( toObject(primitive->getArray(1)), false );
		else
		if( primitiveType=="run" )
		{
			if( !definition->hasKey("runtype") )
				throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
					"HouGeo::loadPrimitive run record is missing runtype", -1, "definition.runtype"});
			if( definition->get<std::string>( "runtype" ) == "Poly" )
			{
				loadPolyPrimitiveRun(definition, primitive->getArray(1));
				return;
			}
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
				"HouGeo::loadPrimitive does not support run type " + definition->get<std::string>("runtype"), -1, "definition.runtype"});
		}
		else if( primitiveType!="Volume" && primitiveType!="Poly" && primitiveType!="Polygon_run"
			&& primitiveType!="p_r" && primitiveType!="PolygonCurve_run" && primitiveType!="c_r" )
		{
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
				"HouGeo::loadPrimitive does not support primitive type " + primitiveType, -1, "definition.type"});
		}

	}

	// HouGeo::HouVolume ==================================================

	void HouGeo::loadVolumePrimitive( json::ObjectPtr volume, SharedPrimitiveData& sharedPrimitiveData )
	{
		if( !volume )
			throw std::invalid_argument( "HouGeo::loadVolumePrimitive received null volume data" );
		if( !volume->hasKey("res") )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadVolumePrimitive missing resolution", -1, "res"});

		HouVolume::Ptr volumePrimitive = std::make_shared<HouVolume>();
		volumePrimitive->field = std::make_shared<ScalarField>();

		withSchemaPath("res", [&]()
		{
			json::ArrayPtr resolutionValues = volume->getArray("res");
			if( !resolutionValues || resolutionValues->size() != 3 )
				throw std::runtime_error( "HouGeo::loadVolumePrimitive resolution must contain three values" );
			const math::V3i resolution(resolutionValues->get<int>(0), resolutionValues->get<int>(1),
				resolutionValues->get<int>(2));
			volumeVoxelCount(resolution);
			volumePrimitive->field->resize(resolution);
		});

		const bool hasVertex = volume->hasKey("vertex");
		const bool hasTransform = volume->hasKey("transform");
		if( hasVertex != hasTransform )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadVolumePrimitive requires vertex and transform together", -1, hasVertex ? "transform" : "vertex"});

		if( hasVertex )
		{
			math::Matrix44d rotationScale;
			withSchemaPath("transform", [&]()
			{
				json::ArrayPtr transformValues = volume->getArray("transform");
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
				volumePrimitive->vertex = topologyVertex;

				const int pointIndex = m_topology->indexBuffer[static_cast<size_t>(topologyVertex)];
				HouAttribute::Ptr positionAttribute = std::dynamic_pointer_cast<HouAttribute>(getPointAttribute("P"));
				if( !positionAttribute || !positionAttribute->m_attr )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive requires a point P attribute" );
				if( positionAttribute->getTupleSize() < 3 )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive P requires at least three components" );
				if( pointIndex < 0 || static_cast<sint64>(pointIndex) >= positionAttribute->getNumElements() )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive point index is outside P" );

				HouGeoAdapter::RawPointer::Ptr rawPosition = positionAttribute->getRawPointer();
				if( !rawPosition || !rawPosition->ptr )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive P has no data" );

				const size_t tupleOffset = static_cast<size_t>(pointIndex)
					* static_cast<size_t>(positionAttribute->getTupleSize());
				if( positionAttribute->m_storage == AttributeAdapter::ATTR_STORAGE_FPREAL16 )
				{
					const uword *values = static_cast<const uword*>(rawPosition->ptr) + tupleOffset;
					position = math::V3f(halfBitsToFloat(values[0]), halfBitsToFloat(values[1]),
						halfBitsToFloat(values[2]));
				}
				else if( positionAttribute->m_storage == AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				{
					const real32 *values = static_cast<const real32*>(rawPosition->ptr) + tupleOffset;
					position = math::V3f(values[0], values[1], values[2]);
				}
				else if( positionAttribute->m_storage == AttributeAdapter::ATTR_STORAGE_FPREAL64 )
				{
					const real64 *values = static_cast<const real64*>(rawPosition->ptr) + tupleOffset;
					position = math::V3f(static_cast<real32>(values[0]), static_cast<real32>(values[1]),
						static_cast<real32>(values[2]));
				}
				else
				{
					throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::unsupported_input,
						"HouGeo::loadVolumePrimitive supports only floating-point P storage", -1, "P.storage"});
				}
			});

			const math::Matrix44d translation = math::Matrix44d::TranslationMatrix(position);
			const math::Matrix44d localToWorld = math::Matrix44d::ScaleMatrix(2.0)
				* math::Matrix44d::TranslationMatrix(-1.0, -1.0, -1.0) * rotationScale * translation;
			volumePrimitive->field->setLocalToWorld(localToWorld);
		}

		const bool hasSharedVoxels = volume->hasKey("sharedvoxels");
		const bool hasInlineVoxels = volume->hasKey("voxels");
		if( hasSharedVoxels == hasInlineVoxels )
			throw DiagnosticException(Diagnostic{DiagnosticSeverity::error, DiagnosticCategory::schema,
				"HouGeo::loadVolumePrimitive requires exactly one voxel payload", -1, "voxels"});

		if( hasSharedVoxels )
		{
			withSchemaPath("sharedvoxels", [&]()
			{
				const std::string dataId = volume->get<std::string>("sharedvoxels");
				const auto sharedData = sharedPrimitiveData.sharedVoxelData.find(dataId);
				if( sharedData == sharedPrimitiveData.sharedVoxelData.end() )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive shared voxel data was not found" );
				loadVoxelData(sharedData->second, volumePrimitive->field->getResolution(),
					volumePrimitive->field->getRawPointer());
			});
		}
		else
		{
			withSchemaPath("voxels", [&]()
			{
				loadVoxelData(toObject(volume->getArray("voxels")), volumePrimitive->field->getResolution(),
					volumePrimitive->field->getRawPointer());
			});
		}

		if( volume->hasKey("visualization") )
		{
			withSchemaPath("visualization", [&]()
			{
				json::ObjectPtr visualization = volume->getObject("visualization");
				if( !visualization )
					throw std::runtime_error( "HouGeo::loadVolumePrimitive visualization must be an object" );
				volumePrimitive->visualizationMode = visualization->get<std::string>("mode", "smoke");
				if( volumePrimitive->visualizationMode.empty() )
					volumePrimitive->visualizationMode = "smoke";
				volumePrimitive->visualizationIso = visualization->get<real32>("iso", 0.0f);
				volumePrimitive->visualizationDensity = visualization->get<real32>("density", 1.0f);
			});
		}

		m_primitives.push_back(volumePrimitive);
	}

	void HouGeo::loadVoxelData( json::ObjectPtr voxelObject, const math::V3i& resolution, float* voxelData )
	{
		if( !voxelObject )
			throw std::invalid_argument( "HouGeo::loadVoxelData received null voxel data" );
		const size_t voxelCount = volumeVoxelCount(resolution);
		if( !voxelData )
			throw std::invalid_argument( "HouGeo::loadVoxelData received null volume storage" );

		const bool hasTiledArray = voxelObject->hasKey("tiledarray");
		const bool hasConstantArray = voxelObject->hasKey("constantarray");
		if( hasTiledArray == hasConstantArray )
			throw std::runtime_error( "HouGeo::loadVoxelData requires exactly one supported voxel representation" );

		if( hasConstantArray )
		{
			const float constantValue = voxelObject->get<float>("constantarray");
			std::fill(voxelData, voxelData + voxelCount, constantValue);
			return;
		}

		json::ObjectPtr tiledArray = toObject(voxelObject->getArray("tiledarray"));
		if( !tiledArray || !tiledArray->hasKey("tiles") )
			throw std::runtime_error( "HouGeo::loadVoxelData tiled array is missing tiles" );
		json::ArrayPtr tiles = tiledArray->getArray("tiles");
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
						json::ArrayPtr tileValues = tiles->getArray(static_cast<int>(currentTileIndex));
						if( !tileValues )
							throw std::runtime_error( "HouGeo::loadVoxelData tile must be a flattened object" );
						json::ObjectPtr tile = toObject(tileValues);
						if( !tile->hasKey("data") )
							throw std::runtime_error( "HouGeo::loadVoxelData tile is missing data" );

						const int compression = tile->get<int>("compression", 1);
						if( compression == 0 || compression == 1 )
						{
							json::ArrayPtr data = tile->getArray("data");
							if( !data || data->size() != static_cast<sint64>(tileVoxelCount) )
								throw std::runtime_error( "HouGeo::loadVoxelData raw tile payload size mismatch" );

							size_t sourceIndex = 0;
							for( int localZ=0;localZ<tileSizeZ;++localZ )
								for( int localY=0;localY<tileSizeY;++localY )
									for( int localX=0;localX<tileSizeX;++localX, ++sourceIndex )
									{
										const size_t destinationIndex = volumeIndex(voxelOffsetX + localX,
											voxelOffsetY + localY, voxelOffsetZ + localZ, resolution);
										if( destinationIndex >= voxelCount )
											throw std::out_of_range( "HouGeo::loadVoxelData destination index exceeds volume" );
										voxelData[destinationIndex] = data->get<float>(static_cast<int>(sourceIndex));
									}
						}
						else if( compression == 2 )
						{
							const float constantValue = tile->get<float>("data");
							for( int localZ=0;localZ<tileSizeZ;++localZ )
								for( int localY=0;localY<tileSizeY;++localY )
									for( int localX=0;localX<tileSizeX;++localX )
									{
										const size_t destinationIndex = volumeIndex(voxelOffsetX + localX,
											voxelOffsetY + localY, voxelOffsetZ + localZ, resolution);
										if( destinationIndex >= voxelCount )
											throw std::out_of_range( "HouGeo::loadVoxelData destination index exceeds volume" );
										voxelData[destinationIndex] = constantValue;
									}
						}
						else
						{
							throw DiagnosticException(Diagnostic{DiagnosticSeverity::error,
								DiagnosticCategory::unsupported_input,
								"HouGeo::loadVoxelData does not support tile compression " + std::to_string(compression),
								-1, "compression"});
						}
					});
				}
			}
		}
	}

	int HouGeo::HouVolume::getVertex()const
	{
		return vertex;
	}

	real32 HouGeo::HouVolume::getVoxel( int i, int j, int k )const
	{
		return field->sample(i, j, k);
	}

	std::string HouGeo::HouVolume::getVisualizationMode()const
	{
		return visualizationMode;
	}

	real32 HouGeo::HouVolume::getVisualizationIso()const
	{
		return visualizationIso;
	}

	real32 HouGeo::HouVolume::getVisualizationDensity()const
	{
		return visualizationDensity;
	}
	
	math::Vec3i HouGeo::HouVolume::getResolution()const
	{
		return field->getResolution();
	}

	math::M44f HouGeo::HouVolume::getTransform()const
	{
		return field->m_localToWorld;
	}

	// returns raw pointer to the data
	HouGeoAdapter::RawPointer::Ptr HouGeo::HouVolume::getRawPointer()
	{
		return HouGeoAdapter::RawPointer::create(field->getRawPointer());
	}


	// HouGeo::HouPoly ==================================================
	void HouGeo::loadPolyPrimitive( json::ObjectPtr polygonObject )
	{
		HouPoly::Ptr polygonPrimitive = std::make_shared<HouPoly>();
		polygonPrimitive->m_closed = polygonObject->get<bool>("closed", true);

		if( !m_topology )
			throw std::runtime_error( "HouGeo::loadPolyPrimitive expects topology to be loaded already" );

		if( polygonObject->hasKey( "vertex" ) )
		{
			json::ArrayPtr topologyIndices = polygonObject->getArray("vertex");
			if( !topologyIndices )
				throw std::runtime_error( "HouGeo::loadPolyPrimitive missing vertex array" );
			// These values index the topology array rather than the point domain directly.
			const int vertexCount = static_cast<int>(topologyIndices->size());
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
		polygonRun->m_numPolys = static_cast<int>(runEntries->size());
		polygonRun->m_closed = true;
		int vertexOffset = 0;
		for( int primitiveIndex=0;primitiveIndex<polygonRun->m_numPolys;++primitiveIndex )
		{
			json::ArrayPtr runEntry = runEntries->getArray(primitiveIndex);
			if( !runEntry || runEntry->size() == 0 )
				throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun invalid polygon entry" );
			json::ArrayPtr topologyIndices = runEntry->getArray(0);
			if( !topologyIndices )
				throw std::runtime_error( "HouGeo::loadPolyPrimitiveRun missing polygon vertices" );
			const int vertexCount = static_cast<int>(topologyIndices->size());
			polygonRun->m_perPolyVertexCount.push_back(vertexCount);
			polygonRun->m_perPolyVertexListOffset.push_back(vertexOffset);
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
		if( !m_topology )
			throw std::runtime_error( "HouGeo::loadPolygonRun expects topology to be loaded already" );

		const std::string startVertexKey = polygonRun->hasKey("startvertex") ? "startvertex" : "s_v";
		const std::string primitiveCountKey = polygonRun->hasKey("nprimitives") ? "nprimitives" : "n_p";
		const std::string runLengthKey = polygonRun->hasKey("nvertices_rle") ? "nvertices_rle" : "r_v";
		const std::string vertexCountsKey = polygonRun->hasKey("nvertices") ? "nvertices" : "n_v";
		const bool hasRunLengthData = polygonRun->hasKey(runLengthKey);
		const bool hasVertexCounts = polygonRun->hasKey(vertexCountsKey);
		if( !polygonRun->hasKey(startVertexKey) || !polygonRun->hasKey(primitiveCountKey)
			|| (!hasRunLengthData && !hasVertexCounts) )
			throw std::runtime_error( "HouGeo::loadPolygonRun missing required fields" );

		const int startVertex = polygonRun->get<int>(startVertexKey, -1);
		const int expectedPrimitiveCount = polygonRun->get<int>(primitiveCountKey, -1);
		json::ArrayPtr vertexCountData = polygonRun->getArray(hasRunLengthData ? runLengthKey : vertexCountsKey);
		if( startVertex < 0 || expectedPrimitiveCount < 0 || !vertexCountData )
			throw std::runtime_error( "HouGeo::loadPolygonRun invalid run metadata" );
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

			for( int repetition=0;repetition<repetitionCount;++repetition )
			{
				const size_t nextTopologyIndex = topologyIndex + static_cast<size_t>(verticesPerPrimitive);
				if( nextTopologyIndex > m_topology->indexBuffer.size() )
					throw std::runtime_error( "HouGeo::loadPolygonRun topology range exceeds index buffer" );

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
				appendPolygons(vertexCountData->get<int>((int)i), vertexCountData->get<int>((int)i+1));
		}
		else
		{
			for( sint64 i=0;i<vertexCountData->size();++i )
				appendPolygons(vertexCountData->get<int>((int)i), 1);
		}

		if( primitiveCount != expectedPrimitiveCount )
			throw std::runtime_error( "HouGeo::loadPolygonRun primitive count mismatch" );

		polygonRunPrimitive->m_numPolys = primitiveCount;
		m_primitives.push_back( polygonRunPrimitive );
	}

	int HouGeo::HouPoly::numPolys()const
	{
		return m_numPolys;
	}

	int HouGeo::HouPoly::numVertices( int poly )const
	{
		if( poly < 0 || poly >= m_numPolys
			|| static_cast<size_t>(poly) >= m_perPolyVertexCount.size() )
			throw std::out_of_range( "HouPoly polygon index is out of range" );
		const int vertexCount = m_perPolyVertexCount[static_cast<size_t>(poly)];
		if( vertexCount < 0 )
			throw std::runtime_error( "HouPoly polygon has a negative vertex count" );
		return vertexCount;
	}

	int const *HouGeo::HouPoly::vertices( int poly )const
	{
		const int vertexCount = numVertices(poly);
		if( static_cast<size_t>(poly) >= m_perPolyVertexListOffset.size() )
			throw std::runtime_error( "HouPoly polygon offset table is incomplete" );
		const int offset = m_perPolyVertexListOffset[static_cast<size_t>(poly)];
		if( offset < 0 )
			throw std::runtime_error( "HouPoly polygon has a negative vertex offset" );
		const size_t offsetValue = static_cast<size_t>(offset);
		const size_t countValue = static_cast<size_t>(vertexCount);
		if( offsetValue > m_vertices.size() || countValue > m_vertices.size() - offsetValue )
			throw std::runtime_error( "HouPoly polygon range exceeds stored vertices" );
		return m_vertices.data() + offsetValue;
	}

	bool HouGeo::HouPoly::closed()const
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
		const int elementCount = static_cast<int>(flattenedArray->size());
		for( int keyIndex=0;keyIndex<elementCount;keyIndex+=2 )
		{
			if( !flattenedArray->getValue(keyIndex).isString() )
				throw std::runtime_error( "HouGeo::toObject requires string keys" );

			const std::string key = flattenedArray->get<std::string>(keyIndex);
			if( object->hasKey(key) )
				throw std::runtime_error( "HouGeo::toObject encountered duplicate key " + key );
			object->append(key, flattenedArray->getValue(keyIndex + 1));
		}

		return object;
	}








	




}  // namespace houio
