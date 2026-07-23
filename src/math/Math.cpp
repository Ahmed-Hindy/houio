/*---------------------------------------------------------------------



----------------------------------------------------------------------*/
#include <houio/math/Math.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace houio
{


namespace math
{


	//
	// computes area of an triangle
	//
	float area( const Vec3f &p0, const Vec3f &p1, const Vec3f &p2 )
	{
		float la = (p1 - p0).getLength(); // compute lengths of the triangle sides
		float lb = (p2 - p1).getLength();
		float lc = (p2 - p0).getLength();
		float s = 0.5f*( la+lb+lc ); // compute the semiperimeter
		return sqrt( s*(s-la)*(s-lb)*(s-lc) ); // compute the area
	}



	//
	// returns the distance of the given point to the line specified by two points
	//
	float distancePointLine( const math::Vec3f &point, const Vec3f &p1, const Vec3f &p2 )
	{
		math::Vec3f vec = point - p1;
		math::Vec3f direction = math::normalize( p2 - p1 );

		return (vec - dotProduct( vec, direction  ) * direction).getLength();
	}


	// returns distance to the closest point on triangle given
	float distancePointTriangle( const Vec3f &point, const Vec3f &p1, const Vec3f &p2, const Vec3f &p3  )
	{
		// copied from http://www.mathworks.com/matlabcentral/fileexchange/22857-distance-between-a-point-and-a-triangle-in-3d

		// rewrite triangle in normal form
		math::Vec3f B = p1;
		math::Vec3f E0 = p2-B;
		//E0 = E0/sqrt(sum(E0.^2)); %normalize vector
		math::Vec3f E1 = p3-B;
		//E1 = E1/sqrt(sum(E1.^2)); %normalize vector


		math::Vec3f D = B - point;
		float a = dot(E0,E0);
		float b = dot(E0,E1);
		float c = dot(E1,E1);
		float d = dot(E0,D);
		float e = dot(E1,D);
		float f = dot(D,D);

		float det = a*c - b*b; // do we have to use abs here?
		float s   = b*e - c*d;
		float t   = b*d - a*e;

		float sqrDistance, invDet, tmp0, tmp1, numer, denom;

		// Terible tree of conditionals to determine in which region of the diagram
		// shown above the projection of the point into the triangle-plane lies.
	
		if ((s+t) <= det)
		{
		  if (s < 0)
		  {
			if (t < 0)
			{
			  // region4
			  if (d < 0)
			  {
				t = 0;
				if (-d >= a)
				{
				  s = 1;
				  sqrDistance = a + 2*d + f;
				}else
				{
				  s = -d/a;
				  sqrDistance = d*s + f;
				}
			  }else
			  {
				s = 0;
				if (e >= 0)
				{
				  t = 0;
				  sqrDistance = f;
				}else
				{
				  if (-e >= c)
				  {
					t = 1;
					sqrDistance = c + 2*e + f;
				  }else
				  {
					t = -e/c;
					sqrDistance = e*t + f;
				  }
				}
			   } //end %of region 4
			}else
			{
			  // region 3
			  s = 0;
			  if (e >= 0)
			  {
				t = 0;
				sqrDistance = f;
			  }else
			  {
				if (-e >= c)
				{
				  t = 1;
				  sqrDistance = c + 2*e +f;
				}else
				{
				  t = -e/c;
				  sqrDistance = e*t + f;
				}
			  }
			} //end %of region 3 
		  }else
		  {
			if (t < 0)
			{
			  // region 5
			  t = 0;
			  if (d >= 0)
			  {
				s = 0;
				sqrDistance = f;
			  }else
			  {
				if (-d >= a)
				{
				  s = 1;
				  sqrDistance = a + 2*d + f; // GF 20101013 fixed typo d*s ->2*d
				}else
				{
				  s = -d/a;
				  sqrDistance = d*s + f;
				}
			  }
			}else
			{
			  //% region 0
			  invDet = 1/det;
			  s = s*invDet;
			  t = t*invDet;
			  sqrDistance = s*(a*s + b*t + 2*d) + t*(b*s + c*t + 2*e) + f;
			}
		  }
		}else
		{
		  if (s < 0)
		  {
			// region 2
			tmp0 = b + d;
			tmp1 = c + e;
			if (tmp1 > tmp0) // minimum on edge s+t=1
			{
			  numer = tmp1 - tmp0;
			  denom = a - 2*b + c;
			  if(numer >= denom)
			  {
				s = 1;
				t = 0;
				sqrDistance = a + 2*d + f; // GF 20101014 fixed typo 2*b -> 2*d
			  }else
			  {
				s = numer/denom;
				t = 1-s;
				sqrDistance = s*(a*s + b*t + 2*d) + t*(b*s + c*t + 2*e) + f;
			  }
			}else       // minimum on edge s=0
			{
			  s = 0;
			  if (tmp1 <= 0)
			  {
				t = 1;
				sqrDistance = c + 2*e + f;
			  }else
				if (e >= 0)
				{
				  t = 0;
				  sqrDistance = f;
				}else
				{
				  t = -e/c;
				  sqrDistance = e*t + f;
				}
			 } //of region 2
		  }else
		  {
			if (t < 0)
			{
			  //region6 
			  tmp0 = b + e;
			  tmp1 = a + d;
			  if (tmp1 > tmp0)
			  {
				numer = tmp1 - tmp0;
				denom = a-2*b+c;
				if (numer >= denom)
				{
				  t = 1;
				  s = 0;
				  sqrDistance = c + 2*e + f;
				}else
				{
				  t = numer/denom;
				  s = 1 - t;
				  sqrDistance = s*(a*s + b*t + 2*d) + t*(b*s + c*t + 2*e) + f;
				}
			  }else  
			  {
				t = 0;
				if (tmp1 <= 0)
				{
					s = 1;
					sqrDistance = a + 2*d + f;
				}else
				{
				  if (d >= 0)
				  {
					  s = 0;
					  sqrDistance = f;
				  }else
				  {
					  s = -d/a;
					  sqrDistance = d*s + f;
				  }
				}
			   }
			  //end region 6
			}else
			{
			  // region 1
			  numer = c + e - b - d;
			  if (numer <= 0)
			  {
				s = 0;
				t = 1;
				sqrDistance = c + 2*e + f;
			  }else
			  {
				denom = a - 2*b + c;
				if (numer >= denom)
				{
				  s = 1;
				  t = 0;
				  sqrDistance = a + 2*d + f;
				}else
				{
				  s = numer/denom;
				  t = 1-s;
				  sqrDistance = s*(a*s + b*t + 2*d) + t*(b*s + c*t + 2*e) + f;
				}
			  } // of region 1
			}
		  }
		}



		// account for numerical round-off error
		if (sqrDistance < 0)
		  sqrDistance = 0;

		float dist = sqrt(sqrDistance);

		//if nargout>1
		//  PP0 = B + s*E0 + t*E1;
		//end
		return dist;
	}


	//
	// computes the distance of a point to a plane
	//
	inline float distancePointPlane( const math::Vec3f &point, const Vec3f &normal, const float &distance )
	{
		return dotProduct( normal, point ) + distance;
	}

	//
	// computes the euclidian distance between 2 points in space
	//
	float distance( const Vec3f &p0, const Vec3f &p1 )
	{
		return (p1-p0).getLength();
	}

	//
	// computes the squared euclidian distance between 2 points in space
	//
	inline float squaredDistance( const Vec3f &p0, const Vec3f &p1 )
	{
		return (p1-p0).getSquaredLength();
	}





	//
	// returns the projection of the given point on the normal and distance specified plane
	//
	math::Vec3f projectPointOnPlane( const math::Vec3f &normal, const float &distance, const math::Vec3f &point )
	{
		return point - distancePointPlane( point, normal, distance )*normal;
	}

	math::Vec3f projectPointOnLine( const math::Vec3f &point, const math::Vec3f &p1, const math::Vec3f &p2 )
	{
		math::Vec3f direction = math::normalize( p2 - p1 );

		return p1 + dotProduct( point - p1, direction  ) * direction;
	}













	//
	//
	//
	float mapValueToRange( const float &sourceRangeMin, const float &sourceRangeMax, const float &targetRangeMin, const float &targetRangeMax, const float &value )
	{
		return (value-sourceRangeMin) / (sourceRangeMax - sourceRangeMin) * (targetRangeMax - targetRangeMin) + targetRangeMin;
	}

	//
	//
	//
	float mapValueTo0_1( const float &sourceRangeMin, const float &sourceRangeMax, const float &value )
	{
		return (value-sourceRangeMin) / (sourceRangeMax - sourceRangeMin);
	}








	Vec3f slerp( Vec3f v0, Vec3f v1, float t  )
	{
		 // Dot product - the cosine of the angle between 2 vectors.
		 float dot = dotProduct(v0, v1);
		 // Clamp it to be in the range of Acos()
		 clamp(dot, -1.0f, 1.0f);
		 // Acos(dot) returns the angle between start and end,
		 // And multiplying that by percent returns the angle between
		 // start and the final result.
		 float theta = acos(dot)*t;
		 Vec3f RelativeVec = v1 - v0*dot;
		 RelativeVec.normalize();
		 // The final result.
		 return ((v0*std::cos(theta)) + (RelativeVec*std::sin(theta)));
	}

	float clamp( float x, float left, float right )
	{
		return (x < left) ? left : (x > right ? right : x);
	}

	float smoothstep( float x )
	{
		return (x) * (x) * (3 - 2 * (x));
	}

	float smoothstep(float edge0, float edge1, float x)
	{
		// Scale, and clamp x to 0..1 range
		x = clamp((x - edge0)/(edge1 - edge0), 0.0f, 1.0f);
		// Evaluate polynomial
		return x*x*x*(x*(x*6.0f - 15.0f) + 10.0f);
	}

	namespace
	{
		void validateInterpolationData(
			std::span<const float> key_positions,
			std::span<const float> key_times,
			std::size_t tuple_width,
			std::span<float> output)
		{
			if (key_times.empty())
				throw std::invalid_argument("Interpolation requires at least one key");
			if (tuple_width == 0)
				throw std::invalid_argument("Interpolation tuple width must be positive");
			if (key_times.size() > std::numeric_limits<std::size_t>::max() / tuple_width
				|| key_positions.size() != key_times.size() * tuple_width)
			{
				throw std::invalid_argument("Interpolation key storage does not match its metadata");
			}
			if (output.size() != tuple_width)
				throw std::invalid_argument("Interpolation output size does not match tuple width");
			for (std::size_t key_index = 1; key_index < key_times.size(); ++key_index)
			{
				if (!(key_times[key_index] > key_times[key_index - 1]))
					throw std::invalid_argument("Interpolation key times must be strictly increasing");
			}
		}

		void copyInterpolationKey(
			std::span<const float> key_positions,
			std::size_t tuple_width,
			std::size_t key_index,
			std::span<float> output)
		{
			const std::size_t offset = key_index * tuple_width;
			std::copy_n(key_positions.begin() + static_cast<std::ptrdiff_t>(offset),
				tuple_width, output.begin());
		}

		std::size_t rightInterpolationKey(
			std::span<const float> key_times,
			float time)
		{
			return static_cast<std::size_t>(
				std::upper_bound(key_times.begin(), key_times.end(), time) - key_times.begin());
		}
	}

	void evaluateLinear(
		std::span<const float> key_positions,
		std::span<const float> key_times,
		std::size_t tuple_width,
		float time,
		std::span<float> output)
	{
		validateInterpolationData(key_positions, key_times, tuple_width, output);
		if (key_times.size() == 1 || time <= key_times.front())
		{
			copyInterpolationKey(key_positions, tuple_width, 0, output);
			return;
		}
		if (time >= key_times.back())
		{
			copyInterpolationKey(key_positions, tuple_width, key_times.size() - 1, output);
			return;
		}

		const std::size_t right_key = rightInterpolationKey(key_times, time);
		const std::size_t left_key = right_key - 1;
		const float factor = (time - key_times[left_key])
			/ (key_times[right_key] - key_times[left_key]);
		const std::size_t left_offset = left_key * tuple_width;
		const std::size_t right_offset = right_key * tuple_width;
		for (std::size_t component = 0; component < tuple_width; ++component)
		{
			output[component] = (1.0f - factor) * key_positions[left_offset + component]
				+ factor * key_positions[right_offset + component];
		}
	}

	void evaluateCatmullRom(
		std::span<const float> key_positions,
		std::span<const float> key_times,
		std::size_t tuple_width,
		float time,
		std::span<float> output)
	{
		validateInterpolationData(key_positions, key_times, tuple_width, output);
		if (key_times.size() == 1 || time <= key_times.front())
		{
			copyInterpolationKey(key_positions, tuple_width, 0, output);
			return;
		}
		if (time >= key_times.back())
		{
			copyInterpolationKey(key_positions, tuple_width, key_times.size() - 1, output);
			return;
		}

		const std::size_t key_2 = rightInterpolationKey(key_times, time);
		const std::size_t key_1 = key_2 - 1;
		const std::size_t key_0 = key_1 == 0 ? 0 : key_1 - 1;
		const std::size_t key_3 = std::min(key_2 + 1, key_times.size() - 1);
		const float factor = (time - key_times[key_1])
			/ (key_times[key_2] - key_times[key_1]);
		const float factor_squared = factor * factor;
		const float factor_cubed = factor_squared * factor;

		for (std::size_t component = 0; component < tuple_width; ++component)
		{
			const float value_0 = key_positions[key_0 * tuple_width + component];
			const float value_1 = key_positions[key_1 * tuple_width + component];
			const float value_2 = key_positions[key_2 * tuple_width + component];
			const float value_3 = key_positions[key_3 * tuple_width + component];
			output[component] = 0.5f * (
				2.0f * value_1
				+ (-value_0 + value_2) * factor
				+ (2.0f * value_0 - 5.0f * value_1 + 4.0f * value_2 - value_3)
					* factor_squared
				+ (-value_0 + 3.0f * value_1 - 3.0f * value_2 + value_3)
					* factor_cubed);
		}
	}
}

} // namespace houio
