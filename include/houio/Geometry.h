#pragma once
#include <memory>
#include <string>
#include <map>

#include <houio/Attribute.h>
#include <houio/math/Math.h>



namespace houio
{

	class Geometry
	{
	public:
		typedef std::shared_ptr<Geometry> Ptr;

		enum PrimitiveType
		{
			POINT = 0, // GL_POINTS
			LINE = 1,
			TRIANGLE = 4,
			QUAD = 7,
			POLYGON = 9
		};

		Geometry( PrimitiveType pt = TRIANGLE );


		void                                                        clear(); // removes all attributes and primitives
		//Geometry                                                  *copy();




		//
		// manipulation
		//
		void                                                        reverse(); // reverses the order of vertices for each primitive (CW polys become CCW)
		unsigned int                                                duplicatePoint( unsigned int index ); // duplicates point and returns index of duplicate (all attributes are copied etc.)
		void                                                        transform( const math::M44f& tm );
		void                                                        addNormals();

		//
		// info
		//
		math::BoundingBox3f                                         getBound();



		//
		// Attribute management
		//
		Attribute::Ptr                                              getAttr( const std::string &name );
		void                                                        setAttr( const std::string &name, Attribute::Ptr attr );
		bool                                                        hasAttr( const std::string &name );
		void                                                        getAttrNames( std::vector<std::string>& attrNames )const;
		void                                                        removeAttr( const std::string &name );
		void                                                        getAttrNames(std::vector<std::string>& names);

		std::map< std::string, Attribute::Ptr >                     m_attributes;


		//
		// primitive management
		//
		PrimitiveType                                               primitiveType();
		unsigned int                                                numPrimitives();
		unsigned int                                                numPrimitiveVertices(); // Point=1; Line=2; Triangle=3; Quad=4
		unsigned int                                                addPoint( unsigned int vId );
		unsigned int                                                addLine( unsigned int vId0, unsigned int vId1 );
		unsigned int                                                addTriangle( unsigned int vId0, unsigned int vId1, unsigned int vId2 );
		unsigned int                                                addQuad( unsigned int vId0, unsigned int vId1, unsigned int vId2, unsigned int vId3 );
		unsigned int                                                addPolygonVertex( unsigned int v );


		PrimitiveType                                               m_primitiveType; // determines the primitive type indexBuffer is pointing to...
		std::vector<unsigned int>                                   m_indexBuffer;
		bool                                                        m_indexBufferIsDirty;
		unsigned int                                                m_numPrimitives;
		unsigned int                                                m_numPrimitiveVertices; // Point=1; Line=2; Triangle=3; Quad=4; polygon:*

		//
		// OpenGL specific
		//
		unsigned int                                                m_indexBufferId;


		//
		// static creators
		//
		static Ptr                                                  createPointGeometry();
		static Ptr                                                  createLineGeometry();
		static Ptr                                                  createTriangleGeometry();
		static Ptr                                                  createQuadGeometry();
		static Ptr                                                  createPolyGeometry();
		//static GeometryPtr                                                           createReferenceMesh();

		static Ptr                                                  createQuad(Geometry::PrimitiveType primType = Geometry::TRIANGLE);
		static Ptr                                                  createGrid( int xres, int zres, Geometry::PrimitiveType primType = Geometry::TRIANGLE );
		static Ptr                                                  createGrid( int xres, int yres, int zres, Geometry::PrimitiveType primType = Geometry::TRIANGLE);
		static Ptr                                                  createSphere( int uSubdivisions, int vSubdivisions, float radius, math::Vec3f center = math::V3f(0.0f), Geometry::PrimitiveType primType = Geometry::TRIANGLE );
		static Ptr                                                  createBox(const math::BoundingBox3f &bound, Geometry::PrimitiveType primType = Geometry::TRIANGLE);
		static Ptr                                                  createLine(const math::V3f &p0, const math::V3f &p1);

		static Ptr                                                  merge(const std::vector<Geometry::Ptr>& geo_list);
	};

}
