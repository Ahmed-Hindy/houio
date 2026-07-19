#include <houio/Attribute.h>
#include <houio/types.h>

#include <iostream>
#include <cstring>



namespace houio
{
	Attribute::Attribute( int numComponents, ComponentType componentType ) :
		m_componentType(componentType),
		m_numComponents(numComponents),
		m_numElements(0),
		m_isDirty(true)
	{
		if( numComponents <= 0 )
			throw std::invalid_argument( "Attribute requires at least one component" );

		switch(componentType)
		{
		case INT:
			{
				m_componentSize=sizeof(sint32);
			}break;
		case INT64:
			{
				m_componentSize=sizeof(sint64);
			}break;
		default:
		case FLOAT:
			{
				m_componentSize=sizeof(float);
			}break;
		};

	}

	Attribute::~Attribute()
	{
	}

	Attribute::Ptr Attribute::copy()
	{
		return create( numComponents(), elementComponentType(), static_cast<const unsigned char*>(getRawPointer()), numElements() );
	}

	Attribute::ComponentType Attribute::elementComponentType()
	{
		return m_componentType;
	}


	Attribute::Ptr Attribute::create(int numComponents, ComponentType componentType, const unsigned char *raw, int numElements)
	{
		if( numElements < 0 )
			throw std::invalid_argument( "Attribute element count cannot be negative" );

		Attribute::Ptr attr = std::make_shared<Attribute>( numComponents, componentType );
		attr->m_numElements = static_cast<size_t>(numElements);
		const size_t size = static_cast<size_t>(attr->elementComponentSize())
			* static_cast<size_t>(attr->numComponents()) * attr->m_numElements;
		attr->m_data.resize(size);
		if( size > 0 )
		{
			if( !raw )
				throw std::invalid_argument( "Attribute raw data cannot be null for non-empty storage" );
			std::memcpy(attr->m_data.data(), raw, size);
		}

		return attr;
	}

	

	Attribute::Ptr Attribute::createM33f()
	{
		return Attribute::Ptr( new Attribute(9, Attribute::FLOAT) );
	}

	Attribute::Ptr Attribute::createM44f()
	{
		return Attribute::Ptr( new Attribute(16, Attribute::FLOAT) );
	}

	Attribute::Ptr Attribute::createV4f( int numElements )
	{
		Attribute::Ptr attr = Attribute::Ptr( new Attribute(4, Attribute::FLOAT) );
		attr->resize(numElements);
		return attr;
	}

	Attribute::Ptr Attribute::createV3f( int numElements )
	{
		Attribute::Ptr attr = Attribute::Ptr( new Attribute(3, Attribute::FLOAT) );
		attr->resize(numElements);
		return attr;
	}

	Attribute::Ptr Attribute::createV2f( int numElements )
	{
		Attribute::Ptr attr = Attribute::Ptr( new Attribute(2, Attribute::FLOAT) );
		attr->resize(numElements);
		return attr;
	}

	Attribute::Ptr Attribute::createFloat( int numElements )
	{
		Attribute::Ptr attr = Attribute::Ptr( new Attribute(1, Attribute::FLOAT) );
		attr->resize(numElements);
		return attr;
	}

	Attribute::Ptr Attribute::createInt( int numElements )
	{
		Attribute::Ptr attr = Attribute::Ptr( new Attribute(1, Attribute::INT) );
		attr->resize(numElements);
		return attr;
	}


	int Attribute::componentSize( ComponentType ct )
	{
		switch(ct)
		{
		case INT:return sizeof(sint32);
		case FLOAT:return sizeof(real32);
		case INT64:return sizeof(sint64);
		default:
			throw std::runtime_error( "unknown component type" );
		};
	}


	Attribute::ComponentType Attribute::componentType( const std::string& ct )
	{
		if( ct == "fpreal32" )
			return FLOAT;
		else
		if( ct == "float" )
			return FLOAT;
		else
		//if( ct == "fpreal64" )
		//	return REAL64;
		//else
		if( ct == "int32" )
			return INT;
		else if( ct == "int" )
			return INT;
		else if( ct == "int64" )
			return INT64;
		return INVALID;
	}


}
