/*---------------------------------------------------------------------



----------------------------------------------------------------------*/
#include <houio/math/Color.h>
#include <houio/math/Math.h>

namespace houio
{

namespace math
{
	//
	//
	//
	Color::Color()
	{
		r=0.0f; g=0.0f; b=0.0f; a=0.0f;
	}

	//
	//
	//
	Color::Color( const float &red, const float &green, const float &blue, const float &alpha )
	{
		r = red; g = green; b = blue; a = alpha;
	}


	//
	//
	//
	Color::~Color()
	{
	}

	//
	//
	//
    void Color::set( const float &red, const float &green, const float &blue, const float &alpha )
	{
		r = red; g = green; b = blue; a = alpha;
	}

	//
	//
	//
	void Color::clamp( void )
	{
		if( r>1.0f )
			r=1.0f;
		if( r<0.0f )
			r=0.0f;

		if( g>1.0f )
			g=1.0f;
		if( g<0.0f )
			g=0.0f;

		if( b>1.0f )
			b=1.0f;
		if( b<0.0f )
			b=0.0f;

		if( a>1.0f )
			a=1.0f;
		if( a<0.0f )
			a=0.0f;
	}

	//
	//
	//
	void Color::invert( void )
	{
		r = 1.0f-r;
		g = 1.0f-g;
		b = 1.0f-b;
		a = 1.0f-a;
	}

	//
	//
	//
	unsigned long Color::makeDWORD( void )
	{
		clamp();

		return setColor( (unsigned int)(r*255.0f), (unsigned int)(g*255.0f), (unsigned int)(b*255.0f), (unsigned int)(a*255.0f) );
	}


	//
	//
	//
	bool Color::operator==( const Color &rhs )
	{
		if( r==rhs.r && g==rhs.g && b==rhs.b && a==rhs.a )
			return true;
		else
			return false;
	}

	//
	//
	//
	bool Color::operator!=( const Color &rhs )
	{
		return !((*this)==rhs);
	}

	//
	//
	//
	bool Color::operator+=( const Color &rhs )
	{
		r+=rhs.r;
		g+=rhs.g;
		b+=rhs.b;
		a+=rhs.a;

		return true;
	}

	//
	//
	//
	bool Color::operator-=( const Color &rhs )
	{
		r-=rhs.r;
		g-=rhs.g;
		b-=rhs.b;
		a-=rhs.a;

		return true;
	}

	//
	//
	//
	bool Color::operator*=( const Color &rhs )
	{
		r*=rhs.r;
		g*=rhs.g;
		b*=rhs.b;
		a*=rhs.a;

		return true;
	}

	//
	//
	//
    bool Color::operator+=( const float &rhs )
	{
		r+=rhs;
		g+=rhs;
		b+=rhs;
		a+=rhs;

		return true;
	}

	//
	//
	//
	bool Color::operator-=( const float &rhs )
	{
		r-=rhs;
		g-=rhs;
		b-=rhs;
		a-=rhs;

		return true;
	}

	//
	//
	//
	bool Color::operator*=( const float &rhs )
	{
		r*=rhs;
		g*=rhs;
		b*=rhs;
		a*=rhs;

		return true; 
	}

	//
	//
	//
	bool Color::operator/=( const float &rhs )
	{
		r/=rhs;
		g/=rhs;
		b/=rhs;
		a/=rhs;

		return true; 
	}

	//
	//
	//
    const float& Color::operator[]( int i ) const
	{
        return v[i];
	}

	//
	//
	//
    float& Color::operator[]( int i )
	{
		return v[i];
	}


	// standard colors

	Color Color::White()
	{
		return Color( 1.0f, 1.0f, 1.0f );
	}

	Color Color::Black()
	{
		return Color( 0.0f, 0.0f, 0.0f );
	}

	Color Color::Blue()
	{
		return Color( 0.0f, 0.0f, 1.0f );
	}

	Color Color::Yellow()
	{
		return Color( 1.0f, 1.0f, 0.0f );
	}

	Color Color::Green()
	{
		return Color( 0.0f, 1.0f, 0.0f );
	}

	Color Color::Red()
	{
		return Color( 1.0f, 0.0f, 0.0f );
	}

	Color Color::From255( const unsigned char &red, const unsigned char &green, const unsigned char &blue, const unsigned char &alpha )
	{
		return Color( static_cast<float>(red)/255.0f, static_cast<float>(green)/255.0f,
			static_cast<float>(blue)/255.0f, static_cast<float>(alpha)/255.0f );
	}
}

} // namespace houio
