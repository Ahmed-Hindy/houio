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


	float triangleArea(
		const Vec3f& first_vertex,
		const Vec3f& second_vertex,
		const Vec3f& third_vertex)
	{
		return cross(
			second_vertex - first_vertex,
			third_vertex - first_vertex).length() * 0.5f;
	}

	float distancePointToLine(
		const Vec3f& point,
		const Vec3f& first_line_point,
		const Vec3f& second_line_point)
	{
		const Vec3f line_direction = second_line_point - first_line_point;
		const float length_squared = line_direction.squaredLength();
		if (length_squared <= std::numeric_limits<float>::epsilon())
			return (point - first_line_point).length();
		const float parameter = dot(point - first_line_point, line_direction) / length_squared;
		const Vec3f projected_point = first_line_point + line_direction * parameter;
		return (point - projected_point).length();
	}

	float distancePointToTriangle(
		const Vec3f& point,
		const Vec3f& first_vertex,
		const Vec3f& second_vertex,
		const Vec3f& third_vertex)
	{
		const Vec3f first_edge = second_vertex - first_vertex;
		const Vec3f second_edge = third_vertex - first_vertex;
		const Vec3f triangle_normal = cross(first_edge, second_edge);

		auto segment_distance = [&point](const Vec3f& start, const Vec3f& end)
		{
			const Vec3f segment = end - start;
			const float length_squared = dot(segment, segment);
			if (length_squared <= std::numeric_limits<float>::epsilon())
				return (point - start).length();
			const float parameter = std::clamp(
				dot(point - start, segment) / length_squared,
				0.0f,
				1.0f);
			return (point - (start + segment * parameter)).length();
		};

		if (triangle_normal.squaredLength() <= std::numeric_limits<float>::epsilon())
		{
			return std::min({
				segment_distance(first_vertex, second_vertex),
				segment_distance(second_vertex, third_vertex),
				segment_distance(third_vertex, first_vertex),
			});
		}

		const Vec3f from_first = point - first_vertex;
		const float first_projection = dot(first_edge, from_first);
		const float second_projection = dot(second_edge, from_first);
		if (first_projection <= 0.0f && second_projection <= 0.0f)
			return from_first.length();

		const Vec3f from_second = point - second_vertex;
		const float third_projection = dot(first_edge, from_second);
		const float fourth_projection = dot(second_edge, from_second);
		if (third_projection >= 0.0f && fourth_projection <= third_projection)
			return from_second.length();

		const float first_edge_region =
			first_projection * fourth_projection - third_projection * second_projection;
		if (first_edge_region <= 0.0f && first_projection >= 0.0f && third_projection <= 0.0f)
		{
			const float parameter = first_projection / (first_projection - third_projection);
			const Vec3f closest_point = first_vertex + first_edge * parameter;
			return (point - closest_point).length();
		}

		const Vec3f from_third = point - third_vertex;
		const float fifth_projection = dot(first_edge, from_third);
		const float sixth_projection = dot(second_edge, from_third);
		if (sixth_projection >= 0.0f && fifth_projection <= sixth_projection)
			return from_third.length();

		const float second_edge_region =
			fifth_projection * second_projection - first_projection * sixth_projection;
		if (second_edge_region <= 0.0f && second_projection >= 0.0f && sixth_projection <= 0.0f)
		{
			const float parameter = second_projection / (second_projection - sixth_projection);
			const Vec3f closest_point = first_vertex + second_edge * parameter;
			return (point - closest_point).length();
		}

		const float opposite_edge_region =
			third_projection * sixth_projection - fifth_projection * fourth_projection;
		const float second_edge_parameter = fourth_projection - third_projection;
		const float third_edge_parameter = fifth_projection - sixth_projection;
		if (opposite_edge_region <= 0.0f
			&& second_edge_parameter >= 0.0f
			&& third_edge_parameter >= 0.0f)
		{
			const float parameter = second_edge_parameter
				/ (second_edge_parameter + third_edge_parameter);
			const Vec3f closest_point = second_vertex
				+ (third_vertex - second_vertex) * parameter;
			return (point - closest_point).length();
		}

		const float inverse_total = 1.0f
			/ (opposite_edge_region + second_edge_region + first_edge_region);
		const float first_weight = second_edge_region * inverse_total;
		const float second_weight = first_edge_region * inverse_total;
		const Vec3f closest_point = first_vertex
			+ first_edge * first_weight
			+ second_edge * second_weight;
		return (point - closest_point).length();
	}


	float signedDistanceToPlane(
		const Vec3f& point,
		const Vec3f& plane_normal,
		float plane_offset)
	{
		const float normal_length = plane_normal.length();
		if (normal_length <= std::numeric_limits<float>::epsilon())
			throw std::invalid_argument("signedDistanceToPlane requires a non-zero normal");
		return (dot(plane_normal, point) + plane_offset) / normal_length;
	}

	float distance(const Vec3f& first_point, const Vec3f& second_point)
	{
		return (second_point - first_point).length();
	}

	float squaredDistance(const Vec3f& first_point, const Vec3f& second_point)
	{
		return (second_point - first_point).squaredLength();
	}

	Vec3f projectPointOntoPlane(
		const Vec3f& point,
		const Vec3f& plane_normal,
		float plane_offset)
	{
		const float normal_length_squared = plane_normal.squaredLength();
		if (normal_length_squared <= std::numeric_limits<float>::epsilon())
			throw std::invalid_argument("projectPointOntoPlane requires a non-zero normal");
		const float scale = (dot(plane_normal, point) + plane_offset)
			/ normal_length_squared;
		return point - plane_normal * scale;
	}

	Vec3f projectPointOntoLine(
		const Vec3f& point,
		const Vec3f& first_line_point,
		const Vec3f& second_line_point)
	{
		const Vec3f line_direction = second_line_point - first_line_point;
		const float length_squared = line_direction.squaredLength();
		if (length_squared <= std::numeric_limits<float>::epsilon())
			return first_line_point;
		const float parameter = dot(point - first_line_point, line_direction)
			/ length_squared;
		return first_line_point + line_direction * parameter;
	}













	float normalizeRange(
		float value,
		float source_minimum,
		float source_maximum)
	{
		const float source_width = source_maximum - source_minimum;
		if (source_width == 0.0f)
			throw std::invalid_argument("normalizeRange requires a non-zero source range");
		return (value - source_minimum) / source_width;
	}

	float remap(
		float value,
		float source_minimum,
		float source_maximum,
		float target_minimum,
		float target_maximum)
	{
		return target_minimum
			+ normalizeRange(value, source_minimum, source_maximum)
				* (target_maximum - target_minimum);
	}

	Vec3f sphericalLerp(
		const Vec3f& start_direction,
		const Vec3f& end_direction,
		float factor)
	{
		if (start_direction.squaredLength() <= std::numeric_limits<float>::epsilon()
			|| end_direction.squaredLength() <= std::numeric_limits<float>::epsilon())
		{
			throw std::invalid_argument("sphericalLerp requires non-zero directions");
		}

		const Vec3f start = start_direction.normalized();
		const Vec3f end = end_direction.normalized();
		const float cosine = std::clamp(dot(start, end), -1.0f, 1.0f);
		constexpr float parallel_threshold = 0.9995f;

		if (cosine > parallel_threshold)
		{
			const Vec3f interpolated = start * (1.0f - factor) + end * factor;
			if (interpolated.squaredLength() <= std::numeric_limits<float>::epsilon())
				return start;
			return interpolated.normalized();
		}

		if (cosine < -parallel_threshold)
		{
			const float absolute_x = std::abs(start.x);
			const float absolute_y = std::abs(start.y);
			const float absolute_z = std::abs(start.z);
			const Vec3f reference_axis = absolute_x <= absolute_y && absolute_x <= absolute_z
				? Vec3f(1.0f, 0.0f, 0.0f)
				: (absolute_y <= absolute_z
					? Vec3f(0.0f, 1.0f, 0.0f)
					: Vec3f(0.0f, 0.0f, 1.0f));
			const Vec3f orthogonal = cross(start, reference_axis).normalized();
			const float angle = std::numbers::pi_v<float> * factor;
			return start * std::cos(angle) + orthogonal * std::sin(angle);
		}

		const float angle = std::acos(cosine) * factor;
		const Vec3f orthogonal = (end - start * cosine).normalized();
		return start * std::cos(angle) + orthogonal * std::sin(angle);
	}

	float smoothstep(float value)
	{
		if (!std::isfinite(value))
			throw std::invalid_argument("smoothstep requires a finite value");
		const float normalized = std::clamp(value, 0.0f, 1.0f);
		return normalized * normalized * (3.0f - 2.0f * normalized);
	}

	float smoothstep(float first_edge, float second_edge, float value)
	{
		if (!std::isfinite(first_edge)
			|| !std::isfinite(second_edge)
			|| !std::isfinite(value))
		{
			throw std::invalid_argument("smoothstep requires finite inputs");
		}
		if (!(first_edge < second_edge))
			throw std::invalid_argument("smoothstep requires increasing edges");
		return smoothstep((value - first_edge) / (second_edge - first_edge));
	}

	namespace
	{
		void validateInterpolationData(
			std::span<const float> key_positions,
			std::span<const float> key_times,
			std::size_t tuple_width,
			float time,
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
			if (!std::isfinite(time))
				throw std::invalid_argument("Interpolation time must be finite");
			for (const float key_time : key_times)
			{
				if (!std::isfinite(key_time))
					throw std::invalid_argument("Interpolation key times must be finite");
			}
			for (const float key_position : key_positions)
			{
				if (!std::isfinite(key_position))
					throw std::invalid_argument("Interpolation key values must be finite");
			}
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
		validateInterpolationData(key_positions, key_times, tuple_width, time, output);
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
		validateInterpolationData(key_positions, key_times, tuple_width, time, output);
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
