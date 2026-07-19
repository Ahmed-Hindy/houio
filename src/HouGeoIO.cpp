#include <houio/HouGeoIO.h>



namespace houio
{
	thread_local json::BinaryWriter* HouGeoIO::g_writer = nullptr;

	HouGeo::Ptr HouGeoIO::import( std::istream *in )
	{
		return import(in, json::ParserLimits());
	}

	HouGeo::Ptr HouGeoIO::import( std::istream *in, const json::ParserLimits &limits )
	{
		json::JSONReader reader;
		json::Parser p(limits);

		//core::Timer timer;
		//timer.start();
		if(!p.parse( in, &reader ))
			return HouGeo::Ptr();
		//timer.stop();
		//float time_parse = timer.elapsedSeconds();

		//timer.reset();
		// now reader contains the json data (structured after the scheme of the file)
		// we will create an empty HouGeo and have it load its data from the json data
		HouGeo::Ptr houGeo = HouGeo::create();
		//timer.start();
		houGeo->load( HouGeo::toObject(reader.getRoot().asArray()) );
		//timer.stop();
		//float time_load = timer.elapsedSeconds();
		//qDebug() << "loading time: " << time_load;
		//qDebug() << "total time: " << (time_load + time_parse);
		return houGeo;
	}

	Geometry::Ptr HouGeoIO::importGeometry( const std::string &path )
	{
		Geometry::Ptr result;
		std::ifstream in( path.c_str(), std::ios_base::in | std::ios_base::binary );
		HouGeo::Ptr hgeo = HouGeoIO::import( &in );
		if( hgeo )
		{
			std::vector<HouGeoAdapter::Primitive::Ptr> primitives;
			hgeo->getPrimitives(primitives);

			HouGeo::Primitive::Ptr prim;

			// warning: the list of primitives does not coincide with the actuall
			// list of primitives because some primitives represent multiple primitives
			// such as the PolyPrimitive representing multiple polygons
			// just be ware when letting users access primitives...
			if( !primitives.empty() )
			{
				prim = primitives[0];
				if(std::dynamic_pointer_cast<HouGeo::HouPoly>(prim) )
					result = convertToGeometry(hgeo, prim);
			}else
				// no valid primitive to convert
				// we pass an invalid point to get some default geometry created
				// if there are points present, we will get those points
				result = convertToGeometry(hgeo, HouGeoAdapter::Primitive::Ptr());
		}else
		{
			std::cout << "HouGeoIO::importGeometry: failed to import houGeo\n";
		}
		return result;
	}

	ScalarField::Ptr HouGeoIO::importVolume( const std::string &path )
	{
		ScalarField::Ptr result;
		std::ifstream in( path.c_str(), std::ios_base::in | std::ios_base::binary );
		HouGeo::Ptr hgeo = HouGeoIO::import( &in );
		if( hgeo )
		{
			std::vector<HouGeoAdapter::Primitive::Ptr> primitives;
			hgeo->getPrimitives(primitives);

			// warning: the list of primitives does not coincide with the actuall
			// list of primitives because some primitives represent multiple primitives
			// such as the PolyPrimitive representing multiple polygons
			// just be ware when letting users access primitives...
			HouGeo::Primitive::Ptr prim = primitives[0];

			//volume
			if(std::dynamic_pointer_cast<HouGeo::HouVolume>(prim) )
			{
				HouGeo::HouVolume::Ptr houVolume = std::dynamic_pointer_cast<HouGeo::HouVolume>(prim);
				result = houVolume->field;
			}
		}else
		{
			std::cout << "HouGeoIO::importVolume: failed to import houGeo\n";
		}
		return result;
	}

	// prim -1 means we will get a simple pointsgeometry
	Geometry::Ptr HouGeoIO::convertToGeometry(HouGeo::Ptr houGeo, HouGeoAdapter::Primitive::Ptr houPrim )
	{
		Geometry::Ptr result;

		// cast and get some first numbers...
		sint64 numPoints = houGeo->pointcount();
		sint64 numVertices = houGeo->vertexcount();

		// we only support geometry with non mixed primitives (e.g. triangles only)
		// this code checks if that is the case...
		int numVerticesPerPoly = 0;
		if( houPrim )
		{
			HouGeo::HouPoly::Ptr poly = std::dynamic_pointer_cast<HouGeo::HouPoly>(houPrim);
			sint64 numPolys = poly->numPolys();

			bool hasConstantVertexCountPerPoly = true;
			{
				numVerticesPerPoly = numPolys > 0 ? poly->numVertices(0) : 0;
				for( int i=0;i<numPolys;++i )
					if(poly->numVertices(i) != numVerticesPerPoly)
					{
						hasConstantVertexCountPerPoly = false;
						break;
					}
			}
			if( !hasConstantVertexCountPerPoly )
				throw std::runtime_error( "convertHouGeoPrimitive: non constant polygon primitvies" );
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
			int numComponents = houAttr->getTupleSize();

			Attribute::Ptr attr;
			if( attrName == "P" )
			{
				if( numComponents < 3 )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: P requires at least three components" );
				HouGeoAdapter::RawPointer::Ptr rawPointer = houAttr->getRawPointer();
				if( !rawPointer || !rawPointer->ptr )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: P has no data" );

				attr = result->getAttr("P");
				for( sint64 pointIndex=0;pointIndex<numPoints;++pointIndex )
				{
					math::Vec3f position;
					const size_t tupleOffset = static_cast<size_t>(pointIndex)*static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						const real32* values = static_cast<const real32*>(rawPointer->ptr) + tupleOffset;
						position = math::Vec3f(values[0], values[1], values[2]);
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						const real64* values = static_cast<const real64*>(rawPointer->ptr) + tupleOffset;
						position = math::Vec3f(static_cast<real32>(values[0]), static_cast<real32>(values[1]), static_cast<real32>(values[2]));
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
				HouGeoAdapter::RawPointer::Ptr rawPointer = houAttr->getRawPointer();
				if( !rawPointer || !rawPointer->ptr )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: UV has no data" );

				attrName = "UV";
				attr = Attribute::createV2f();
				for( sint64 pointIndex=0;pointIndex<numPoints;++pointIndex )
				{
					math::Vec2f uv;
					const size_t tupleOffset = static_cast<size_t>(pointIndex)*static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						const real32* values = static_cast<const real32*>(rawPointer->ptr) + tupleOffset;
						uv = math::Vec2f(values[0], values[1]);
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						const real64* values = static_cast<const real64*>(rawPointer->ptr) + tupleOffset;
						uv = math::Vec2f(static_cast<real32>(values[0]), static_cast<real32>(values[1]));
					}
					else
						throw std::runtime_error( "HouGeoIO::convertToGeometry: unsupported UV storage" );
					attr->appendElement(uv);
				}
			}else
			if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				attr = Attribute::create( numComponents, Attribute::FLOAT, (unsigned char *)houAttr->getRawPointer()->ptr, houAttr->getNumElements() );
			else
			if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
			{
				attr = Attribute::create( numComponents, Attribute::INT, (unsigned char *)houAttr->getRawPointer()->ptr, houAttr->getNumElements() );

			}else
				std::cout << "HouGeoIO::convertToGeometry: warning: unable to handle storage type " << houAttr->getStorage() << " for attribute " << attrName << std::endl;

			if( attr )
				result->setAttr( attrName, attr );
		}

		// convert vertex attributes ---
		std::vector<std::pair<Attribute::Ptr, Attribute::Ptr>> vertex2pointAttr;
		for( std::vector<std::string>::iterator it = vertexAttributesNames.begin(), end = vertexAttributesNames.end();  it != end; ++it )
		{
			std::string attrName = *it;
			HouGeoAdapter::AttributeAdapter::Ptr houAttr = houGeo->getVertexAttribute(attrName);

			int numComponents = houAttr->getTupleSize();

			Attribute::Ptr attr;
			if( (attrName == "UV")||(attrName == "uv") )
			{
				if( numComponents < 2 )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: vertex UV requires at least two components" );
				HouGeoAdapter::RawPointer::Ptr rawPointer = houAttr->getRawPointer();
				if( !rawPointer || !rawPointer->ptr )
					throw std::runtime_error( "HouGeoIO::convertToGeometry: vertex UV has no data" );

				attrName = "UV";
				attr = Attribute::createV2f();

				for( sint64 vertexIndex=0;vertexIndex<numVertices;++vertexIndex )
				{
					math::Vec2f uv;
					const size_t tupleOffset = static_cast<size_t>(vertexIndex)*static_cast<size_t>(numComponents);
					if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					{
						const real32* values = static_cast<const real32*>(rawPointer->ptr) + tupleOffset;
						uv = math::Vec2f(values[0], values[1]);
					}
					else if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					{
						const real64* values = static_cast<const real64*>(rawPointer->ptr) + tupleOffset;
						uv = math::Vec2f(static_cast<real32>(values[0]), static_cast<real32>(values[1]));
					}
					else
						throw std::runtime_error( "HouGeoIO::convertToGeometry: unsupported vertex UV storage" );
					attr->appendElement(uv);
				}
			}else
			if( houAttr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				attr = Attribute::create( numComponents, Attribute::FLOAT, (unsigned char *)houAttr->getRawPointer()->ptr, houAttr->getNumElements() );


			if( !attr )
			{
				std::cout << "HouGeoIO::convertToGeometry: warning: unable to convert vertex attribute " << attrName << std::endl;
				continue;
			}

			// create point attribute which we will derive from this vertex attribute
			Attribute::Ptr pointAttr = attr->copy();
			pointAttr->resize( (int)numPoints );
			memset( pointAttr->getRawPointer(), 0, numPoints*pointAttr->elementComponentSize()*pointAttr->numComponents() );

			result->setAttr( attrName, pointAttr );
			vertex2pointAttr.push_back( std::make_pair( attr, pointAttr ) );
		}


		// only done when we have primitives...
		if( houPrim )
		{
			HouGeo::HouPoly::Ptr poly = std::dynamic_pointer_cast<HouGeo::HouPoly>(houPrim);
			sint64 numPolys = poly->numPolys();


			// since opengl doesnt support face varying data we also dont support it in our geometry
			// therefore we need convert our vertex attribute to point attributes and duplicate those points
			// which have different vertex data
			const int *vertex_ptr = poly->vertices();

			std::vector<bool> pointsToSplit(numPoints, false);
			std::vector<bool> pointsInitialized(numPoints, false);

			// mark all points which have to be duplicated
			std::vector<int> firstVertex(numPoints, -1);
			for( int i=0;i<numVertices;++i )
			{
				int point = vertex_ptr[i];
				if( !pointsToSplit[point] )
				{
					if( firstVertex[point] >=0 )
					{
						// compare
						for( std::vector<std::pair<Attribute::Ptr, Attribute::Ptr>>::iterator it = vertex2pointAttr.begin(), end = vertex2pointAttr.end(); it != end; ++it )
							if( memcmp( it->first->getRawPointer(i), it->first->getRawPointer(firstVertex[point]), it->first->elementComponentSize()*it->first->numComponents() ) )
							{
								pointsToSplit[point] = true;
								//result->getAttr("Cd")->set( point, 1.0f, 0.0f, 0.0f );
								break;
							}
					}else
						firstVertex[point] = i;
				}
			}

			int vertexIndex = 0;
			for( int i=0;i<numPolys;++i )
			{
				int numVerts = poly->numVertices(i);
				std::vector<int> vertices;
				vertices.reserve(4);

				for( int j=0;j<numVerts;++j, ++vertexIndex )
				{
					unsigned int finalPointIndex = vertex_ptr[j];

					if( pointsToSplit[finalPointIndex] && pointsInitialized[finalPointIndex] )
					{
						finalPointIndex = result->duplicatePoint(finalPointIndex);
						// copy vertex attributes to duplicated point location
						for( std::vector<std::pair<Attribute::Ptr, Attribute::Ptr>>::iterator it = vertex2pointAttr.begin(), end = vertex2pointAttr.end(); it != end; ++it )
							memcpy( it->second->getRawPointer(finalPointIndex), it->first->getRawPointer(vertexIndex), it->first->elementComponentSize()*it->first->numComponents());
					}
					else if( !pointsInitialized[finalPointIndex]  )
					{
						pointsInitialized[finalPointIndex] = true;
						// copy vertex attributes to duplicated point location
						for( std::vector<std::pair<Attribute::Ptr, Attribute::Ptr>>::iterator it = vertex2pointAttr.begin(), end = vertex2pointAttr.end(); it != end; ++it )
							memcpy( it->second->getRawPointer(finalPointIndex), it->first->getRawPointer(vertexIndex), it->first->elementComponentSize()*it->first->numComponents());
					}

					//if( vertex2pointAttr[0].first->get<math::Vec2f>(vertexIndex).x == result->getAttr("UV")->get<math::Vec2f>(finalPointIndex).x )
					//	result->getAttr("Cd")->set(finalPointIndex, 1.0f, 0.0f, 0.0f);

					vertices.push_back(finalPointIndex);
				}

				if( numVerts == 2 )
					result->addLine( vertices[0], vertices[1] );
				else
				if( numVerts == 3 )
					result->addTriangle( vertices[0], vertices[1], vertices[2] );
				else
				if( numVerts == 4 )
					result->addQuad( vertices[0], vertices[1], vertices[2], vertices[3] );

				vertex_ptr+=numVerts;
			}

			// houdini polys are CW - opengl defaults to CCW
			result->reverse();
		}
		return result;
	}

	void HouGeoIO::makeLog( const std::string &path, std::ostream *out )
	{
		std::ifstream in( path.c_str(), std::ios_base::in | std::ios_base::binary );
		json::JSONLogger logger(*out);
		json::Parser p;
		p.parse( &in, &logger );
	}



	// convinience funcion for quickly saving volume to bgeo
	bool HouGeoIO::xport( const std::string& filename, ScalarField::Ptr volume )
	{
		std::ofstream out( filename.c_str(), std::ios_base::out | std::ios_base::binary );
		HouGeo::Ptr houGeo = std::make_shared<HouGeo>();
		houGeo->addPrimitive(volume);
		return HouGeoIO::xport( &out, houGeo );
	}

	bool HouGeoIO::xport(const std::string &filename, Geometry::Ptr geo)
	{
		std::ofstream out( filename.c_str(), std::ios_base::out | std::ios_base::binary );
		HouGeo::Ptr houGeo = std::make_shared<HouGeo>();


		// contruct point attributes  ---
		std::vector<std::string> pointAttributeNames;
		geo->getAttrNames(pointAttributeNames);
		for( auto name : pointAttributeNames )
		{
			Attribute::Ptr attr = geo->getAttr(name);

			// some attributes need special treatment
			if( name == "P" && attr->numComponents() == 3 )
			{
				// promote P attribute from v3f to v4f
				Attribute::Ptr attr_new = std::make_shared<Attribute>( 4, Attribute::FLOAT);
				int numElements = attr->numElements();
				for( int i=0;i<numElements;++i )
				{
					math::V3f p = attr->get<math::V3f>(i);
					attr_new->appendElement<math::V4f>( math::V4f(p.x, p.y, p.z, 1.0) );
				}
				attr = attr_new;
			}

			std::cout << "adding " << name << std::endl;
			HouGeo::HouAttribute::Ptr hattr = std::make_shared<HouGeo::HouAttribute>( name, attr );
			houGeo->setPointAttribute( hattr );
		}

		if(geo->numPrimitives()>0)
		{
			// topology ---
			HouGeo::HouTopology::Ptr top = std::make_shared<HouGeo::HouTopology>();

			size_t numIndices = geo->m_indexBuffer.size();
			top->indexBuffer.resize(numIndices);
			for( size_t i=0;i<numIndices;++i )
				top->indexBuffer[i] = geo->m_indexBuffer[i];

			houGeo->setTopology(top);

			// primitives ---

			// each poly is a seperate primitive...
			/*
			for( int i=0;i<geo->numPrimitives();++i )
			{
				HouGeo::HouPoly::Ptr poly = std::make_shared<HouGeo::HouPoly>();
				poly->m_numPolys = 1;
				poly->m_perPolyVertexCount = std::vector<int>(1, geo->numPrimitiveVertices());
				poly->m_vertices.resize(geo->numPrimitiveVertices());
				for( int j=0;j<geo->numPrimitiveVertices();++j )
					poly->m_vertices[j] = i*geo->numPrimitiveVertices() + j;
				houGeo->addPrimitive(poly);
			}
			*/

			// all polys are combined making a run
			{
				HouGeo::HouPoly::Ptr poly = std::make_shared<HouGeo::HouPoly>();
				poly->m_numPolys = geo->numPrimitives();
				poly->m_perPolyVertexListOffset = std::vector<int>(geo->numPrimitives(), 0);
				int numPrims = geo->numPrimitives();
				int numPrimVertices = geo->numPrimitiveVertices();
				for( int i=1;i<numPrims;++i )
				{
					poly->m_perPolyVertexListOffset[i] = poly->m_perPolyVertexListOffset[i-1] + numPrimVertices;
				}
				poly->m_perPolyVertexCount = std::vector<int>(numPrims, numPrimVertices);

				if( geo->m_indexBuffer.size() > static_cast<size_t>(std::numeric_limits<int>::max()) )
					throw std::overflow_error( "HouGeoIO::xport index buffer exceeds int range" );
				poly->m_vertices.resize(geo->m_indexBuffer.size());
				for( size_t i=0;i<geo->m_indexBuffer.size();++i )
					poly->m_vertices[i] = static_cast<int>(i);

				poly->m_closed = false;
				if(geo->primitiveType() != Geometry::LINE)
					poly->m_closed = true;

				houGeo->addPrimitive(poly);
			}
		}


		return HouGeoIO::xport( &out, houGeo );
	}

	bool HouGeoIO::xport( const std::string& filename, const std::vector<math::V3f>& points )
	{
		std::map<std::string, std::vector<math::V3f>> pattr_v3f;
		pattr_v3f["P"] = points;
		return xport(filename, pattr_v3f);
	}

	bool HouGeoIO::xport( const std::string& filename, const std::map<std::string, std::vector<math::V3f>>& pattr_v3f )
	{
		Geometry::Ptr geo = Geometry::createPointGeometry();

		for( auto&it:pattr_v3f )
		{
			std::string name = it.first;
			const std::vector<math::V3f>& data = it.second;

			int numElements = int(data.size());
			if (numElements > 0)
			{
				Attribute::Ptr attr = Attribute::createV3f(numElements);
				memcpy(attr->getRawPointer(), &data[0], sizeof(math::V3f)*numElements);
				geo->setAttr(name, attr);
			}
		}

		return xport( filename, geo );
	}




	bool HouGeoIO::xport( std::ostream *out, HouGeoAdapter::Ptr geo, bool binary )
	{
		if( !out || !geo || !out->good() )
			return false;
		if( !binary )
			return false;

		json::BinaryWriter writer(out);
		struct WriterBinding
		{
			WriterBinding(json::BinaryWriter*& writerSlot, json::BinaryWriter& activeWriter)
				: slot(writerSlot), previous(writerSlot)
			{
				slot = &activeWriter;
			}

			~WriterBinding()
			{
				slot = previous;
			}

			WriterBinding(const WriterBinding&) = delete;
			WriterBinding& operator=(const WriterBinding&) = delete;

			json::BinaryWriter*& slot;
			json::BinaryWriter* previous;
		};
		WriterBinding writerBinding(g_writer, writer);

		const sint64 pointCount = geo->pointcount();
		const sint64 vertexCount = geo->vertexcount();
		const sint64 primitiveCount = geo->primitivecount();
		if( pointCount < 0 || vertexCount < 0 || primitiveCount < 0 )
			throw std::runtime_error( "HouGeoIO::xport received negative geometry counts" );

		g_writer->jsonBeginArray();

		g_writer->jsonString( "pointcount" );
		g_writer->jsonInt( pointCount );

		g_writer->jsonString( "vertexcount" );
		g_writer->jsonInt( vertexCount );

		g_writer->jsonString( "primitivecount" );
		g_writer->jsonInt( primitiveCount );

		// -- topology (required)
		g_writer->jsonString( "topology" );
		g_writer->jsonBeginArray();
			if( geo->getTopology() )
				exportTopology( geo->getTopology() );
		g_writer->jsonEndArray();


		// -- attributes
		g_writer->jsonString( "attributes" );
		g_writer->jsonBeginArray();

			// -- point attributes
			g_writer->jsonString( "pointattributes" );
			g_writer->jsonBeginArray();
				std::vector<std::string> pointAttrNames;
				geo->getPointAttributeNames(pointAttrNames);
				for( std::vector<std::string>::iterator it = pointAttrNames.begin(); it != pointAttrNames.end(); ++it )
					exportAttribute( geo->getPointAttribute(*it) );
			g_writer->jsonEndArray(); // pointattributes

			// -- vertex attributes
			g_writer->jsonString( "vertexattributes" );
			g_writer->jsonBeginArray();
				std::vector<std::string> vertexAttrNames;
				geo->getVertexAttributeNames(vertexAttrNames);
				for( std::vector<std::string>::iterator it = vertexAttrNames.begin(); it != vertexAttrNames.end(); ++it )
					exportAttribute( geo->getVertexAttribute(*it) );
			g_writer->jsonEndArray(); // vertexattributes


			// -- primitive attributes
			g_writer->jsonString( "primitiveattributes" );
			g_writer->jsonBeginArray();
				std::vector<std::string> primitiveAttrNames;
				geo->getPrimitiveAttributeNames(primitiveAttrNames);
				for( std::vector<std::string>::iterator it = primitiveAttrNames.begin(); it != primitiveAttrNames.end(); ++it )
					exportAttribute( geo->getPrimitiveAttribute(*it) );
			g_writer->jsonEndArray(); // primitiveattributes

			// -- global attributes
			g_writer->jsonString( "globalattributes" );
			g_writer->jsonBeginArray();
				std::vector<std::string> globalAttrNames;
				geo->getGlobalAttributeNames(globalAttrNames);
				for( std::vector<std::string>::iterator it = globalAttrNames.begin(); it != globalAttrNames.end(); ++it )
					exportAttribute( geo->getGlobalAttribute(*it) );
			g_writer->jsonEndArray(); // globalattributes
		g_writer->jsonEndArray(); // attributes


		// -- primitives
		if( primitiveCount > 0 )
		{
			g_writer->jsonString( "primitives" );
			g_writer->jsonBeginArray();

			std::vector<HouGeoAdapter::Primitive::Ptr> primitives;
			geo->getPrimitives(primitives);

			int topologyVertexOffset = 0;
			for( auto prim : primitives )
			{
				if( auto volume = std::dynamic_pointer_cast<HouGeoAdapter::VolumePrimitive>(prim) )
				{
					exportPrimitive(volume);
					++topologyVertexOffset;
				}
				else if( auto poly = std::dynamic_pointer_cast<HouGeoAdapter::PolyPrimitive>(prim) )
				{
					exportPrimitive(poly, topologyVertexOffset);
					for( int polygonIndex=0;polygonIndex<poly->numPolys();++polygonIndex )
						topologyVertexOffset += poly->numVertices(polygonIndex);
				}
			}
			g_writer->jsonEndArray(); // primitives
		}

		std::vector<std::string> pointGroupNames;
		geo->getPointGroupNames(pointGroupNames);
		if( !pointGroupNames.empty() )
		{
			g_writer->jsonString("pointgroups");
			g_writer->jsonBeginArray();
			for( const std::string &name : pointGroupNames )
			{
				std::vector<bool> membership;
				if( !geo->getPointGroupMembership(name, membership)
					|| membership.size() != static_cast<size_t>(pointCount) )
					throw std::runtime_error( "HouGeoIO::xport invalid point group " + name );
				exportGroup(name, membership);
			}
			g_writer->jsonEndArray();
		}

		std::vector<std::string> vertexGroupNames;
		geo->getVertexGroupNames(vertexGroupNames);
		if( !vertexGroupNames.empty() )
		{
			g_writer->jsonString("vertexgroups");
			g_writer->jsonBeginArray();
			for( const std::string &name : vertexGroupNames )
			{
				std::vector<bool> membership;
				if( !geo->getVertexGroupMembership(name, membership)
					|| membership.size() != static_cast<size_t>(vertexCount) )
					throw std::runtime_error( "HouGeoIO::xport invalid vertex group " + name );
				exportGroup(name, membership);
			}
			g_writer->jsonEndArray();
		}

		std::vector<std::string> primitiveGroupNames;
		geo->getPrimitiveGroupNames(primitiveGroupNames);
		if( !primitiveGroupNames.empty() )
		{
			g_writer->jsonString("primitivegroups");
			g_writer->jsonBeginArray();
			for( const std::string &name : primitiveGroupNames )
			{
				std::vector<bool> membership;
				if( !geo->getPrimitiveGroupMembership(name, membership)
					|| membership.size() != static_cast<size_t>(primitiveCount) )
					throw std::runtime_error( "HouGeoIO::xport invalid primitive group " + name );
				exportGroup(name, membership);
			}
			g_writer->jsonEndArray();
		}

		g_writer->jsonEndArray(); // /root

		return out->good();
	}






	bool HouGeoIO::exportAttribute( HouGeoAdapter::AttributeAdapter::Ptr attr )
	{
		if( !attr )
			return false;

		std::string type;
		std::string storage;
		const int sourceSize = attr->getTupleSize();
		const std::string name = attr->getName();
		const bool promotePosition = name == "P" && sourceSize == 3;
		const int size = promotePosition ? 4 : sourceSize;

		if( attr->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC )
			type = "numeric";
		else if( attr->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_STRING )
			type = "string";

		if( attr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
				storage = "fpreal32";
		else if( attr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
				storage = "fpreal64";
		else if( attr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
				storage = "int32";


		if( name == "P" && size != 4 )
			throw std::runtime_error( "HouGeoIO::exportAttribute: P must contain either three or four components" );

		g_writer->jsonBeginArray();


		// attribute definition ------------
		g_writer->jsonBeginArray();

		g_writer->jsonString( "name" );
		g_writer->jsonString( name );

		g_writer->jsonString( "type" );
		g_writer->jsonString(type);

		g_writer->jsonEndArray(); // definition

		// attribute content ------------
		g_writer->jsonBeginArray();

		if( attr->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_NUMERIC )
		{
			g_writer->jsonString( "size" );
			g_writer->jsonInt( size );

			g_writer->jsonString( "storage" );
			g_writer->jsonString( storage );


			g_writer->jsonString( "values" );
			g_writer->jsonBeginArray();

				g_writer->jsonString( "size" );
				g_writer->jsonInt( size );

				g_writer->jsonString( "storage" );
				g_writer->jsonString( storage );

				g_writer->jsonString( "pagesize" );
				g_writer->jsonInt( 1024 );


				//g_writer->jsonString( "packing" );
				//std::vector<int> packing;
				//attr->getPacking( packing );
				//g_writer->jsonUniformArray( packing );

				//std::cout << "CHECK " << name << std::endl;
				//std::cout << "CHECK " << attr->getNumElements() << std::endl;
				//std::cout << "CHECK " << attr->getTupleSize() << std::endl;
				//attr->getRawPointer();

				g_writer->jsonString( "rawpagedata" );
				HouGeoAdapter::RawPointer::Ptr rawPointer = attr->getRawPointer();
				if( !rawPointer || (!rawPointer->ptr && attr->getNumElements() > 0) )
					throw std::runtime_error( "HouGeoIO::exportAttribute: missing data for attribute " + name );

				if( promotePosition )
				{
					if( attr->getStorage() != HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
						throw std::runtime_error( "HouGeoIO::exportAttribute: three-component P promotion currently requires fpreal32 storage" );

					const real32* source = static_cast<const real32*>(rawPointer->ptr);
					std::vector<real32> promotedData(static_cast<size_t>(attr->getNumElements())*4u);
					for( int elementIndex=0;elementIndex<attr->getNumElements();++elementIndex )
					{
						const size_t sourceOffset = static_cast<size_t>(elementIndex)*3u;
						const size_t destinationOffset = static_cast<size_t>(elementIndex)*4u;
						promotedData[destinationOffset] = source[sourceOffset];
						promotedData[destinationOffset+1u] = source[sourceOffset+1u];
						promotedData[destinationOffset+2u] = source[sourceOffset+2u];
						promotedData[destinationOffset+3u] = 1.0f;
					}
					g_writer->jsonUniformArray(promotedData);
				}
				else if( attr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL32 )
					g_writer->jsonUniformArray<real32>( static_cast<const real32*>(rawPointer->ptr), attr->getNumElements()*sourceSize );
				else if( attr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_FPREAL64 )
					g_writer->jsonUniformArray<real64>( static_cast<const real64*>(rawPointer->ptr), attr->getNumElements()*sourceSize );
				else if( attr->getStorage() == HouGeoAdapter::AttributeAdapter::ATTR_STORAGE_INT32 )
					g_writer->jsonUniformArray<sint32>( static_cast<const sint32*>(rawPointer->ptr), attr->getNumElements()*sourceSize );

			g_writer->jsonEndArray(); // values
		}else
		if(attr->getType() == HouGeoAdapter::AttributeAdapter::ATTR_TYPE_STRING )
		{
			g_writer->jsonString( "size" );
			g_writer->jsonInt( size );

			g_writer->jsonString( "storage" );
			g_writer->jsonString( "int32" );

			std::map<std::string, sint32> stringLookup;
			std::vector<std::string> stringTable;
			std::vector<sint32> indices;
			stringTable.reserve(static_cast<size_t>(attr->getNumElements()));
			indices.reserve(static_cast<size_t>(attr->getNumElements()));
			for( int i=0;i<attr->getNumElements();++i )
			{
				const std::string value = attr->getString(i);
				auto insertion = stringLookup.emplace(value, static_cast<sint32>(stringTable.size()));
				if( insertion.second )
					stringTable.push_back(value);
				indices.push_back(insertion.first->second);
			}

			g_writer->jsonString( "strings" );
			g_writer->jsonBeginArray();
				for( const std::string& value : stringTable )
					g_writer->jsonString(value);
			g_writer->jsonEndArray();

			g_writer->jsonString( "indices" );
			g_writer->jsonBeginArray();
				g_writer->jsonString( "size" );
				g_writer->jsonInt32( 1 );

				g_writer->jsonString( "storage" );
				g_writer->jsonString( "int32" );

				g_writer->jsonString( "pagesize" );
				g_writer->jsonInt32( 1024 );

				g_writer->jsonString( "rawpagedata" );
				g_writer->jsonUniformArray(indices);

			g_writer->jsonEndArray();


		}



		g_writer->jsonEndArray(); // attribute content



		g_writer->jsonEndArray(); // attribute

		return true;
	}

	// export topo
	// TODO: datatype is int ~~~~
	bool HouGeoIO::exportTopology( HouGeoAdapter::Topology::Ptr topo )
	{
		std::vector<sint32> indices;
		topo->getIndices(indices);
		
		// Determine the type used for the index array, either 16- or 32-bit integers.
		bool indexExceeds16Bit = false;
		for( const sint32 index : indices )
		{
			if( index < 0 )
				throw std::runtime_error( "HouGeoIO::exportTopology cannot export negative point indices" );
			if( index > std::numeric_limits<sint16>::max() )
				indexExceeds16Bit = true;
		}

		g_writer->jsonString( "pointref" );
		g_writer->jsonBeginArray();
			g_writer->jsonString( "indices" );
			if( indexExceeds16Bit )
			{
				g_writer->jsonUniformArray<sint32>(indices);
			}
			else
			{
				std::vector<sint16> compactIndices;
				compactIndices.reserve(indices.size());
				for( const sint32 index : indices )
					compactIndices.push_back(static_cast<sint16>(index));
				g_writer->jsonUniformArray<sint16>(compactIndices);
			}
			
		g_writer->jsonEndArray();

		return true;
	}


	bool HouGeoIO::exportGroup( const std::string &name, const std::vector<bool> &membership )
	{
		if( name.empty() )
			throw std::runtime_error( "HouGeoIO::exportGroup requires a non-empty name" );

		std::vector<sbyte> encodedMembership;
		encodedMembership.reserve(membership.size());
		for( bool selected : membership )
			encodedMembership.push_back(static_cast<sbyte>(selected ? 1 : 0));

		g_writer->jsonBeginArray();
			g_writer->jsonBeginArray();
				g_writer->jsonString("name");
				g_writer->jsonString(name);
			g_writer->jsonEndArray();

			g_writer->jsonBeginArray();
				g_writer->jsonString("selection");
				g_writer->jsonBeginArray();
					g_writer->jsonString("unordered");
					g_writer->jsonBeginArray();
						g_writer->jsonString("i8");
						g_writer->jsonUniformArray(encodedMembership);
					g_writer->jsonEndArray();
				g_writer->jsonEndArray();
			g_writer->jsonEndArray();
		g_writer->jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( HouGeoAdapter::VolumePrimitive::Ptr volume )
	{
		math::V3i res = volume->getResolution();

		g_writer->jsonBeginArray();

		// primitive type
		g_writer->jsonBeginArray();
			g_writer->jsonString("type");
			g_writer->jsonString("Volume");
		g_writer->jsonEndArray();

		g_writer->jsonBeginArray();
			g_writer->jsonString("vertex");
			g_writer->jsonInt32(volume->getVertex());

			g_writer->jsonString("transform");
			math::M44f houLocalToWorldTranslation = math::M44f::TranslationMatrix(volume->getTransform().getTranslation());
			math::M44f houLocalToWorldRotationScale = math::M44f::ScaleMatrix(0.5f)*math::M44f::TranslationMatrix(1.0,1.0,1.0)*volume->getTransform()*houLocalToWorldTranslation.inverted();
			math::M33f transform( houLocalToWorldRotationScale.ma[0], houLocalToWorldRotationScale.ma[1], houLocalToWorldRotationScale.ma[2],
						houLocalToWorldRotationScale.ma[4], houLocalToWorldRotationScale.ma[5], houLocalToWorldRotationScale.ma[6],
						houLocalToWorldRotationScale.ma[8], houLocalToWorldRotationScale.ma[9], houLocalToWorldRotationScale.ma[10]);
			g_writer->jsonUniformArray<real32>( transform.ma, 9 );

			g_writer->jsonString("res");
			g_writer->jsonUniformArray<sint32>( &res.x, 3 );

			g_writer->jsonString("border");
			g_writer->jsonBeginMap();
				g_writer->jsonKey("type");
				g_writer->jsonString("constant");
				g_writer->jsonKey("value");
				g_writer->jsonReal32(0.0f);
			g_writer->jsonEndMap();

			g_writer->jsonString("compression");
			g_writer->jsonBeginMap();
				g_writer->jsonKey("tolerance");
				g_writer->jsonReal32(0.0f);
			g_writer->jsonEndMap();

			g_writer->jsonString("voxels");
			g_writer->jsonBeginArray();
				g_writer->jsonString("tiledarray");
				g_writer->jsonBeginArray();
					g_writer->jsonString("version");
					g_writer->jsonInt32( 1 );

					g_writer->jsonString("compressiontype");
					// !!! uniform string array?
					g_writer->jsonBeginArray();
						g_writer->jsonString("raw");
						g_writer->jsonString("rawfull");
						g_writer->jsonString("constant");
						g_writer->jsonString("fpreal16");
						g_writer->jsonString("FP32Range");
					g_writer->jsonEndArray();

					g_writer->jsonString("tiles");
					g_writer->jsonBeginArray();

						// get first invalid tileindex in each dimension
						math::Vec3i tileEnd;
						tileEnd.x = res.x / 16;
						tileEnd.y = res.y / 16;
						tileEnd.z = res.z / 16;

						// if there are some voxels remaining, add another tile
						if( res.x%16 )
							++tileEnd.x;
						if( res.y%16 )
							++tileEnd.y;
						if( res.z%16 )
							++tileEnd.z;


						math::Vec3i voxelOffset; // start offset (in voxels) for current tile
						math::Vec3i numVoxels;   // number of voxels for current tile (may differ in each dimension)
						std::vector<real32> data;
						int remainK = res.z;
						for( int tk=0; tk<tileEnd.z;++tk )
						{
							voxelOffset.z = tk*16;
							numVoxels.z = std::min( 16,  remainK );

							int remainJ = res.y;
							for( int tj=0; tj<tileEnd.y;++tj )
							{
								voxelOffset.y = tj*16;
								numVoxels.y = std::min( 16,  remainJ );

								int remainI = res.x;
								for( int ti=0; ti<tileEnd.x;++ti )
								{
									voxelOffset.x = ti*16;
									numVoxels.x = std::min( 16,  remainI );

									// write tile ---
									g_writer->jsonBeginArray();
										g_writer->jsonString("compression");
										g_writer->jsonInt32(0);
										g_writer->jsonString("data");
										data.clear();
										math::V3i voxelEnd( voxelOffset.x + numVoxels.x, voxelOffset.y + numVoxels.y, voxelOffset.z + numVoxels.z );
										for( int k=voxelOffset.z;k<voxelEnd.z;++k )
											for( int j=voxelOffset.y;j<voxelEnd.y;++j )
												for( int i=voxelOffset.x;i<voxelEnd.x;++i )
													data.push_back(volume->getVoxel( i, j, k ));
										g_writer->jsonUniformArray(data);
									g_writer->jsonEndArray();

									remainI -=16;
								}
								remainJ -=16;
							}
							remainK -=16;
						}

					g_writer->jsonEndArray();

				g_writer->jsonEndArray();

			g_writer->jsonEndArray();

			g_writer->jsonString("visualization");
			g_writer->jsonBeginMap();
				g_writer->jsonKey("mode");
				g_writer->jsonString("smoke");
				g_writer->jsonKey("iso");
				g_writer->jsonReal32(0.0f);
				g_writer->jsonKey("density");
				g_writer->jsonReal32(1.0f);
			g_writer->jsonEndMap();

			g_writer->jsonString("taperx");
			g_writer->jsonReal32(1.0f);

			g_writer->jsonString("tapery");
			g_writer->jsonReal32(1.0f);



		g_writer->jsonEndArray();

		g_writer->jsonEndArray();
		return true;
	}

	bool HouGeoIO::exportPrimitive( HouGeoAdapter::PolyPrimitive::Ptr poly, int startVertex )
	{
		if( !poly || startVertex < 0 || poly->numPolys() <= 0 )
			return false;

		std::vector<sint32> vertexCounts;
		vertexCounts.reserve(static_cast<size_t>(poly->numPolys()));
		for( int polygonIndex=0;polygonIndex<poly->numPolys();++polygonIndex )
		{
			const int vertexCount = poly->numVertices(polygonIndex);
			if( vertexCount <= 0 )
				throw std::runtime_error( "HouGeoIO::exportPrimitive: polygon has no vertices" );
			vertexCounts.push_back(vertexCount);
		}

		g_writer->jsonBeginArray();
			g_writer->jsonBeginArray();
				g_writer->jsonString("type");
				g_writer->jsonString(poly->closed() ? "Polygon_run" : "PolygonCurve_run");
			g_writer->jsonEndArray();

			g_writer->jsonBeginArray();
				g_writer->jsonString("startvertex");
				g_writer->jsonInt32(startVertex);

				g_writer->jsonString("nprimitives");
				g_writer->jsonInt32(poly->numPolys());

				g_writer->jsonString("nvertices");
				g_writer->jsonUniformArray(vertexCounts);
			g_writer->jsonEndArray();
		g_writer->jsonEndArray();
		return true;
	}

}










