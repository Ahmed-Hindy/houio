#pragma once

#include <memory>
#include <string>
#include <vector>

#include <houio/Attribute.h>
#include <houio/math/Math.h>
#include <houio/types.h>



namespace houio
{
	namespace json
	{
		struct Object;
	}

	struct HouGeoAdapter
	{
		using Ptr = std::shared_ptr<HouGeoAdapter>;

		struct RawPointer final
		{
			using Ptr = std::shared_ptr<RawPointer>;

			explicit RawPointer(const void* data_value) noexcept : ptr(data_value) {}
			static Ptr create(const void* data);

			const void* ptr = nullptr;
		};

		struct AttributeAdapter
		{
			using Ptr = std::shared_ptr<AttributeAdapter>;

			virtual ~AttributeAdapter();
			enum Type
			{
				ATTR_TYPE_INVALID  = 0,
				ATTR_TYPE_NUMERIC  = 1,
				ATTR_TYPE_STRING   = 2,
				ATTR_TYPE_DICT     = 3
			};
			enum Storage
			{
				ATTR_STORAGE_INVALID  = 0,
				ATTR_STORAGE_FPREAL32 = 1,
				ATTR_STORAGE_FPREAL64 = 2,
				ATTR_STORAGE_INT32 = 3,
				ATTR_STORAGE_INT64 = 4,
				ATTR_STORAGE_FPREAL16 = 5
			};
			virtual std::string              getName()const;
			virtual Type                     getType()const;
			virtual int                      getTupleSize()const;
			virtual Storage                  getStorage()const;
			virtual void                     getPacking( std::vector<int> &packing )const;
			virtual int                      getNumElements()const;
			virtual RawPointer::Ptr          getRawPointer();
			virtual std::string              getString( int index )const=0;
			virtual std::shared_ptr<json::Object> getDictionary( int index )const;
			static Type                      type( const std::string &typeName );
			static Storage                   storage( const std::string &storageName );
			static int                       storageSize( Storage storage );
		};

		struct Topology
		{
			using Ptr = std::shared_ptr<Topology>;

			virtual ~Topology();

			virtual void                          getIndices( std::vector<int> &indices )const=0;
			virtual void                          addIndices( std::vector<int> &indices )=0;
			virtual sint64                        getNumIndices()const=0;
		};

		struct Primitive
		{
			using Ptr = std::shared_ptr<Primitive>;
			enum Type
			{
				PRIM_VOLUME,
				PRIM_POLY
			};
			virtual ~Primitive() = default;

			// Primitive runs override this to report the number of represented records.
			virtual int numPrimitives()const{return 1;}
		};

		struct VolumePrimitive : public Primitive
		{
			using Ptr = std::shared_ptr<VolumePrimitive>;

			virtual math::M44f                 getTransform()const=0;
			virtual int                        getVertex()const=0;
			virtual math::Vec3i                getResolution()const;
			virtual real32                     getVoxel( int i, int j, int k )const=0;
			virtual std::string                getVisualizationMode()const;
			virtual real32                     getVisualizationIso()const;
			virtual real32                     getVisualizationDensity()const;
			virtual RawPointer::Ptr            getRawPointer(); // returns raw pointer to the data
		};

		struct PolyPrimitive : public Primitive
		{
			using Ptr = std::shared_ptr<PolyPrimitive>;

			virtual int                        numPolys()const;
			virtual int                        numVertices( int poly )const;
			virtual int const*                 vertices(int poly=0)const;
			virtual int numPrimitives()const override{return numPolys();}
			virtual bool closed()const;
		};


		virtual sint64                pointcount()const;
		virtual sint64                vertexcount()const;
		virtual sint64                primitivecount()const;
		virtual void                  getPointAttributeNames( std::vector<std::string> &names )const;
		virtual AttributeAdapter::Ptr getPointAttribute( const std::string &name );
		virtual void                  getVertexAttributeNames( std::vector<std::string> &names )const;
		virtual AttributeAdapter::Ptr getVertexAttribute( const std::string &name );
		virtual void                  getGlobalAttributeNames( std::vector<std::string> &names )const;
		virtual AttributeAdapter::Ptr getGlobalAttribute( const std::string &name );
		virtual void                  getPointGroupNames( std::vector<std::string> &names )const;
		virtual bool                  getPointGroupMembership( const std::string &name, std::vector<bool> &membership )const;
		virtual void                  getVertexGroupNames( std::vector<std::string> &names )const;
		virtual bool                  getVertexGroupMembership( const std::string &name, std::vector<bool> &membership )const;
		virtual void                  getPrimitiveGroupNames( std::vector<std::string> &names )const;
		virtual bool                  getPrimitiveGroupMembership( const std::string &name, std::vector<bool> &membership )const;
		virtual bool                  hasPrimitiveAttribute( const std::string &name )const;
		virtual void                  getPrimitiveAttributeNames( std::vector<std::string> &names )const=0;
		virtual AttributeAdapter::Ptr getPrimitiveAttribute( const std::string &name )=0;
		virtual void                  getPrimitives( std::vector<HouGeoAdapter::Primitive::Ptr>& primitives );
		virtual Topology::Ptr         getTopology();
	};
}


