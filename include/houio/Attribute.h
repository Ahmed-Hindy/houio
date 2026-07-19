#pragma once
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include <houio/math/Math.h>



namespace houio
{

	// basicly a list manager
	struct Attribute
	{
		typedef std::shared_ptr<Attribute> Ptr;
		typedef std::shared_ptr<const Attribute> CPtr;

		enum ComponentType
		{
			INVALID,
			INT,
			FLOAT
		};

		Attribute( int numComponents=3, ComponentType componentType = FLOAT );
		~Attribute();

		Attribute::Ptr copy();

		template<typename T>
		unsigned int appendElement( const T &value );
		template<typename T>
		unsigned int appendElement( const T &v0, const T &v1 );
		template<typename T>
		unsigned int appendElement( const T &v0, const T &v1, const T &v2 );
		template<typename T>
		unsigned int appendElement( const T &v0, const T &v1, const T &v2, const T &v3 );

		template<typename T>
		T &get( unsigned int index );

		template<typename T>
		void set( unsigned int index, T value );
		template<typename T>
		void set( unsigned int index, T v0, T v1 );
		template<typename T>
		void set( unsigned int index, T v0, T v1, T v2 );
		template<typename T>
		void set( unsigned int index, T v0, T v1, T v2, T v3 );


		void clear()
		{
			m_data.clear();
			m_numElements = 0;
			m_isDirty = true;
		}

		void resize( size_t numElements )
		{
			const size_t componentCount = static_cast<size_t>(numComponents());
			const size_t componentBytes = static_cast<size_t>(elementComponentSize());
			if( componentCount != 0 && numElements > std::numeric_limits<size_t>::max() / componentCount )
				throw std::length_error( "Attribute element count exceeds addressable storage" );
			const size_t scalarCount = numElements * componentCount;
			if( componentBytes != 0 && scalarCount > std::numeric_limits<size_t>::max() / componentBytes )
				throw std::length_error( "Attribute byte count exceeds addressable storage" );
			m_data.resize(scalarCount * componentBytes);
			m_numElements = numElements;
			m_isDirty = true;
		}


		int numElements()const
		{
			if( m_numElements > static_cast<size_t>(std::numeric_limits<int>::max()) )
				throw std::overflow_error( "Attribute element count exceeds int range" );
			return static_cast<int>(m_numElements);
		}

		int numComponents()const
		{
			return m_numComponents;
		}

		ComponentType elementComponentType();

		int elementComponentSize()const
		{
			return m_componentSize;
		}

		void *getRawPointer()
		{
			if (m_data.empty())
				return 0;
			return (void *)&m_data[0];
		}
		void *getRawPointer( int index )
		{
			if( index < 0 )
				throw std::out_of_range( "Attribute index cannot be negative" );
			const size_t byteOffset = static_cast<size_t>(index) * static_cast<size_t>(numComponents())
				* static_cast<size_t>(elementComponentSize());
			if( byteOffset >= m_data.size() )
				throw std::out_of_range( "Attribute index exceeds stored data" );
			return static_cast<void*>(m_data.data() + byteOffset);
		}





		std::vector<unsigned char> m_data;
		int               m_componentSize; // size in memory of a component of an element in byte
		ComponentType     m_componentType;
		int               m_numComponents; // number of components per element
		size_t              m_numElements;

		//
		// static creators
		//
		static Attribute::Ptr create(int numComponents, ComponentType componentType, const unsigned char *raw, int numElements );
		static Attribute::Ptr createM33f();
		static Attribute::Ptr createM44f();
		static Attribute::Ptr createV4f( int numElements = 0 );
		static Attribute::Ptr createV3f( int numElements = 0 );
		static Attribute::Ptr createV2f( int numElements = 0 );
		static Attribute::Ptr createFloat(int numElements = 0);
		static Attribute::Ptr createInt(int numElements = 0);

		//
		// static utilities
		//
		static int             componentSize( ComponentType ct );
		static ComponentType   componentType(const std::string& ct );

		bool                   m_isDirty; // indicates update on gpu required
	};


	template<typename T>
	unsigned int Attribute::appendElement( const T &value )
	{
		if( m_numElements > static_cast<size_t>(std::numeric_limits<unsigned int>::max()) )
			throw std::overflow_error( "Attribute element index exceeds unsigned int range" );
		const unsigned int elementIndex = static_cast<unsigned int>(m_numElements);
		const size_t byteOffset = m_data.size();
		m_data.resize(byteOffset + sizeof(T));
		std::memcpy(m_data.data() + byteOffset, &value, sizeof(T));
		++m_numElements;
		m_isDirty = true;
		return elementIndex;
	}

	template<typename T>
	unsigned int Attribute::appendElement( const T &v0, const T &v1 )
	{
		const T values[] = {v0, v1};
		if( m_numElements > static_cast<size_t>(std::numeric_limits<unsigned int>::max()) )
			throw std::overflow_error( "Attribute element index exceeds unsigned int range" );
		const unsigned int elementIndex = static_cast<unsigned int>(m_numElements);
		const size_t byteOffset = m_data.size();
		m_data.resize(byteOffset + sizeof(values));
		std::memcpy(m_data.data() + byteOffset, values, sizeof(values));
		++m_numElements;
		m_isDirty = true;
		return elementIndex;
	}

	template<typename T>
	unsigned int Attribute::appendElement( const T &v0, const T &v1, const T &v2 )
	{
		const T values[] = {v0, v1, v2};
		if( m_numElements > static_cast<size_t>(std::numeric_limits<unsigned int>::max()) )
			throw std::overflow_error( "Attribute element index exceeds unsigned int range" );
		const unsigned int elementIndex = static_cast<unsigned int>(m_numElements);
		const size_t byteOffset = m_data.size();
		m_data.resize(byteOffset + sizeof(values));
		std::memcpy(m_data.data() + byteOffset, values, sizeof(values));
		++m_numElements;
		m_isDirty = true;
		return elementIndex;
	}

	template<typename T>
	unsigned int Attribute::appendElement( const T &v0, const T &v1, const T &v2, const T &v3 )
	{
		const T values[] = {v0, v1, v2, v3};
		if( m_numElements > static_cast<size_t>(std::numeric_limits<unsigned int>::max()) )
			throw std::overflow_error( "Attribute element index exceeds unsigned int range" );
		const unsigned int elementIndex = static_cast<unsigned int>(m_numElements);
		const size_t byteOffset = m_data.size();
		m_data.resize(byteOffset + sizeof(values));
		std::memcpy(m_data.data() + byteOffset, values, sizeof(values));
		++m_numElements;
		m_isDirty = true;
		return elementIndex;
	}

	template<typename T>
	T &Attribute::get( unsigned int index )
	{
		T *data = (T*)&m_data[index * sizeof(T)];
		return *data;
	}

	template<typename T>
	void Attribute::set( unsigned int index, T value )
	{
		T *data = (T*)&m_data[index * sizeof(T)];
		*data = value;
		m_isDirty = true;
	}

	template<typename T>
	void Attribute::set( unsigned int index, T v0, T v1 )
	{
		T *data = (T*)&m_data[index * sizeof(T) * 2];
		*data++ = v0;
		*data++ = v1;
		m_isDirty = true;
	}

	template<typename T>
	void Attribute::set( unsigned int index, T v0, T v1, T v2 )
	{
		T *data = (T*)&m_data[index * sizeof(T) * 3];
		*data++ = v0;
		*data++ = v1;
		*data++ = v2;
		m_isDirty = true;
	}

	template<typename T>
	void Attribute::set( unsigned int index, T v0, T v1, T v2, T v3 )
	{
		T *data = (T*)&m_data[index * sizeof(T) * 4];
		*data++ = v0;
		*data++ = v1;
		*data++ = v2;
		*data++ = v3;
		m_isDirty = true;
	}

} // namespace houio
