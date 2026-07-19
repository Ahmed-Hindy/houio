#pragma once

#include <bit>
#include <cstdint>

#include <houio/types.h>

namespace houio
{
	inline real32 halfBitsToFloat( uword halfBits ) noexcept
	{
		const std::uint32_t sign = static_cast<std::uint32_t>(halfBits & 0x8000u) << 16u;
		std::uint32_t exponent = (halfBits >> 10u) & 0x1fu;
		std::uint32_t mantissa = halfBits & 0x03ffu;
		std::uint32_t floatBits = sign;

		if( exponent == 0 )
		{
			if( mantissa != 0 )
			{
				std::uint32_t shift = 0;
				while( (mantissa & 0x0400u) == 0 )
				{
					mantissa <<= 1u;
					++shift;
				}
				mantissa &= 0x03ffu;
				const std::uint32_t floatExponent = 113u - shift;
				floatBits |= (floatExponent << 23u) | (mantissa << 13u);
			}
		}
		else if( exponent == 0x1fu )
		{
			floatBits |= 0x7f800000u | (mantissa << 13u);
		}
		else
		{
			exponent += 112u;
			floatBits |= (exponent << 23u) | (mantissa << 13u);
		}
		return std::bit_cast<real32>(floatBits);
	}

	inline uword floatToHalfBits( real32 value ) noexcept
	{
		const std::uint32_t floatBits = std::bit_cast<std::uint32_t>(value);
		const uword sign = static_cast<uword>((floatBits >> 16u) & 0x8000u);
		const std::uint32_t sourceExponent = (floatBits >> 23u) & 0xffu;
		std::uint32_t mantissa = floatBits & 0x007fffffu;

		if( sourceExponent == 0xffu )
		{
			if( mantissa == 0 )
				return static_cast<uword>(sign | 0x7c00u);
			std::uint32_t halfMantissa = mantissa >> 13u;
			if( halfMantissa == 0 )
				halfMantissa = 1;
			return static_cast<uword>(sign | 0x7c00u | halfMantissa);
		}

		std::int32_t exponent = static_cast<std::int32_t>(sourceExponent) - 127 + 15;
		if( exponent <= 0 )
		{
			if( exponent < -10 )
				return sign;
			mantissa |= 0x00800000u;
			const std::uint32_t shift = static_cast<std::uint32_t>(14 - exponent);
			std::uint32_t halfMantissa = mantissa >> shift;
			const std::uint32_t remainderMask = (std::uint32_t(1) << shift) - 1u;
			const std::uint32_t remainder = mantissa & remainderMask;
			const std::uint32_t halfway = std::uint32_t(1) << (shift - 1u);
			if( remainder > halfway || (remainder == halfway && (halfMantissa & 1u) != 0) )
				++halfMantissa;
			return static_cast<uword>(sign | halfMantissa);
		}

		if( exponent >= 31 )
			return static_cast<uword>(sign | 0x7c00u);

		std::uint32_t halfMantissa = mantissa >> 13u;
		const std::uint32_t remainder = mantissa & 0x1fffu;
		if( remainder > 0x1000u || (remainder == 0x1000u && (halfMantissa & 1u) != 0) )
		{
			++halfMantissa;
			if( halfMantissa == 0x0400u )
			{
				halfMantissa = 0;
				++exponent;
				if( exponent >= 31 )
					return static_cast<uword>(sign | 0x7c00u);
			}
		}
		return static_cast<uword>(sign | (static_cast<uword>(exponent) << 10u)
			| static_cast<uword>(halfMantissa));
	}
}
