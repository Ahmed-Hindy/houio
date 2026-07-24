/*---------------------------------------------------------------------



----------------------------------------------------------------------*/
#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>




#include "Vec2.h"
#include "Vec2Algo.h"
#include "Vec3.h"
#include "Vec3Algo.h"
#include "Vec4.h"
#include "Vec4Algo.h"
#include "Ray3.h"
#include "Matrix22.h"
#include "Matrix22Algo.h"
#include "Matrix33.h"
#include "Matrix33Algo.h"
#include "Matrix44.h"
#include "Matrix44Algo.h"
#include "Color.h"
#include "BoundingBox3.h"
#include "BoundingBox2.h"
#include "RNG.h"

namespace houio
{


///
/// \brief vectors matrices and mathematical operations
///
namespace math
{
	template<std::floating_point T>
	[[nodiscard]] constexpr T radiansToDegrees(T radians) noexcept
	{
		return radians * static_cast<T>(180) / std::numbers::pi_v<T>;
	}

	template<std::floating_point T>
	[[nodiscard]] constexpr T degreesToRadians(T degrees) noexcept
	{
		return degrees * std::numbers::pi_v<T> / static_cast<T>(180);
	}

	[[nodiscard]] constexpr float sign(float value) noexcept
	{
		return value < 0.0f ? -1.0f : 1.0f;
	}

	template<std::floating_point T>
	struct SphericalCoordinates
	{
		T azimuth{};
		T polarAngle{};
		T radius{};
	};

	template<std::floating_point T>
	[[nodiscard]] Vec3<T> fromSphericalCoordinates(
		const SphericalCoordinates<T>& coordinates)
	{
		const T polar_sine = std::sin(coordinates.polarAngle);
		return Vec3<T>(
			coordinates.radius * polar_sine * std::cos(coordinates.azimuth),
			coordinates.radius * std::cos(coordinates.polarAngle),
			coordinates.radius * polar_sine * std::sin(coordinates.azimuth));
	}

	template<std::floating_point T>
	[[nodiscard]] SphericalCoordinates<T> toSphericalCoordinates(const Vec3<T>& point)
	{
		const T radius = point.length();
		if (radius == T{})
			return {};
		return SphericalCoordinates<T>{
			std::atan2(point.z, point.x),
			std::acos(std::clamp(point.y / radius, T{-1}, T{1})),
			radius,
		};
	}

	//
	// color conversion functions
	//



	inline constexpr std::uint32_t red_mask = 0x000000ffu;
	inline constexpr std::uint32_t green_mask = 0x0000ff00u;
	inline constexpr std::uint32_t blue_mask = 0x00ff0000u;
	inline constexpr std::uint32_t alpha_mask = 0xff000000u;

	[[nodiscard]] constexpr std::uint8_t rgbaRed(std::uint32_t color) noexcept
	{
		return static_cast<std::uint8_t>(color & red_mask);
	}

	[[nodiscard]] constexpr std::uint8_t rgbaGreen(std::uint32_t color) noexcept
	{
		return static_cast<std::uint8_t>((color & green_mask) >> 8u);
	}

	[[nodiscard]] constexpr std::uint8_t rgbaBlue(std::uint32_t color) noexcept
	{
		return static_cast<std::uint8_t>((color & blue_mask) >> 16u);
	}

	[[nodiscard]] constexpr std::uint8_t rgbaAlpha(std::uint32_t color) noexcept
	{
		return static_cast<std::uint8_t>((color & alpha_mask) >> 24u);
	}

	[[nodiscard]] constexpr std::uint32_t packRgba(
		std::uint8_t red,
		std::uint8_t green,
		std::uint8_t blue,
		std::uint8_t alpha) noexcept
	{
		return static_cast<std::uint32_t>(red)
			| (static_cast<std::uint32_t>(green) << 8u)
			| (static_cast<std::uint32_t>(blue) << 16u)
			| (static_cast<std::uint32_t>(alpha) << 24u);
	}

	[[nodiscard]] inline std::uint8_t redByte(const Color& color) noexcept
	{
		return static_cast<std::uint8_t>(color.clamped().r * 255.0f);
	}

	[[nodiscard]] inline std::uint8_t greenByte(const Color& color) noexcept
	{
		return static_cast<std::uint8_t>(color.clamped().g * 255.0f);
	}

	[[nodiscard]] inline std::uint8_t blueByte(const Color& color) noexcept
	{
		return static_cast<std::uint8_t>(color.clamped().b * 255.0f);
	}

	[[nodiscard]] inline std::uint8_t alphaByte(const Color& color) noexcept
	{
		return static_cast<std::uint8_t>(color.clamped().a * 255.0f);
	}

	[[nodiscard]] constexpr Color colorFromBytes(
		std::uint8_t red,
		std::uint8_t green,
		std::uint8_t blue,
		std::uint8_t alpha = 255) noexcept
	{
		return Color::fromBytes(red, green, blue, alpha);
	}


	[[nodiscard]] float triangleArea(
		const Vec3f& first_vertex,
		const Vec3f& second_vertex,
		const Vec3f& third_vertex);
	[[nodiscard]] float distance(const Vec3f& first_point, const Vec3f& second_point);
	[[nodiscard]] float squaredDistance(const Vec3f& first_point, const Vec3f& second_point);
	[[nodiscard]] float signedDistanceToPlane(
		const Vec3f& point,
		const Vec3f& plane_normal,
		float plane_offset);
	[[nodiscard]] float distancePointToLine(
		const Vec3f& point,
		const Vec3f& first_line_point,
		const Vec3f& second_line_point);
	[[nodiscard]] float distancePointToTriangle(
		const Vec3f& point,
		const Vec3f& first_vertex,
		const Vec3f& second_vertex,
		const Vec3f& third_vertex);
	[[nodiscard]] Vec3f projectPointOntoPlane(
		const Vec3f& point,
		const Vec3f& plane_normal,
		float plane_offset);
	[[nodiscard]] Vec3f projectPointOntoLine(
		const Vec3f& point,
		const Vec3f& first_line_point,
		const Vec3f& second_line_point);

	[[nodiscard]] float normalizeRange(
		float value,
		float source_minimum,
		float source_maximum);
	[[nodiscard]] float remap(
		float value,
		float source_minimum,
		float source_maximum,
		float target_minimum,
		float target_maximum);

	template<typename T, typename R>
	[[nodiscard]] constexpr T lerp(T start, T end, R factor)
	{
		return T(start * (static_cast<R>(1.0) - factor) + end * factor);
	}

	template<typename T>
	[[nodiscard]] constexpr T step(T value, T edge) noexcept
	{
		return value < edge ? T{} : T{1};
	}

	[[nodiscard]] Vec3f sphericalLerp(
		const Vec3f& start_direction,
		const Vec3f& end_direction,
		float factor);
	[[nodiscard]] float smoothstep(float value);
	[[nodiscard]] float smoothstep(float first_edge, float second_edge, float value);
	void evaluateCatmullRom(
		std::span<const float> key_positions,
		std::span<const float> key_times,
		std::size_t tuple_width,
		float time,
		std::span<float> output);
	void evaluateLinear(
		std::span<const float> key_positions,
		std::span<const float> key_times,
		std::size_t tuple_width,
		float time,
		std::span<float> output);


	inline float sRGBToLinear( float c_srgb )
	{
		const float a = 0.055f;
		const float c_srgb_clamped = std::clamp(c_srgb, 0.0f, 1.0f);

		if( c_srgb_clamped <= 0.04045f )
			return c_srgb_clamped/12.92f;
		else
			return pow( (c_srgb_clamped+a)/(1.0f+a), 2.4f);
	}


	inline float linearToSRGB( float c_linear )
	{
		if( c_linear <= 0.0031308f )
			return 12.92f*c_linear;
		else
		{
			const float a = 0.055f;
			return (1.0f+a)*pow( c_linear, 1.0f/2.4f ) - a;
		}
	}

	struct QuadraticRoots
	{
		float first{};
		float second{};

		[[nodiscard]] constexpr bool repeated() const noexcept
		{
			return first == second;
		}
	};

	[[nodiscard]] inline std::optional<QuadraticRoots> solveQuadratic(
		float coefficient_a,
		float coefficient_b,
		float coefficient_c)
	{
		if (!std::isfinite(coefficient_a)
			|| !std::isfinite(coefficient_b)
			|| !std::isfinite(coefficient_c))
		{
			throw std::invalid_argument("Quadratic coefficients must be finite");
		}
		if (coefficient_a == 0.0f)
			throw std::invalid_argument("Quadratic coefficient A must be non-zero");

		const float discriminant = coefficient_b * coefficient_b
			- 4.0f * coefficient_a * coefficient_c;
		if (discriminant < 0.0f)
			return std::nullopt;
		if (discriminant == 0.0f)
		{
			const float root = -coefficient_b / (2.0f * coefficient_a);
			return QuadraticRoots{root, root};
		}

		const float root_discriminant = std::sqrt(discriminant);
		const float q = coefficient_b < 0.0f
			? -0.5f * (coefficient_b - root_discriminant)
			: -0.5f * (coefficient_b + root_discriminant);
		QuadraticRoots roots{q / coefficient_a, coefficient_c / q};
		if (roots.first > roots.second)
			std::swap(roots.first, roots.second);
		return roots;
	}

}

} // namespace houio
