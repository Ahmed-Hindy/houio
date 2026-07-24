#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <houio/math/BoundingBox3.h>
#include <houio/math/Matrix33.h>
#include <houio/math/Vec3.h>
#include <houio/math/Vec3Algo.h>

namespace houio::math
{
    template<typename T>
    class Ray3
    {
    public:
        Vec3<T> origin;
        Vec3<T> direction;
        T minimumDistance = T{};
        T maximumDistance = std::numeric_limits<T>::max();

        constexpr Ray3() noexcept = default;

        constexpr Ray3(
            const Vec3<T>& ray_origin,
            const Vec3<T>& ray_direction,
            T minimum_distance = T{},
            T maximum_distance = std::numeric_limits<T>::max()) noexcept
            : origin(ray_origin),
              direction(ray_direction),
              minimumDistance(minimum_distance),
              maximumDistance(maximum_distance)
        {
        }

        [[nodiscard]] constexpr Vec3<T> position(T distance) const noexcept
        {
            return origin + direction * distance;
        }

        [[nodiscard]] constexpr Vec3<T> operator()(T distance) const noexcept
        {
            return position(distance);
        }

        [[nodiscard]] constexpr bool contains(T distance) const noexcept
        {
            return distance >= minimumDistance && distance <= maximumDistance;
        }
    };

    template<typename T>
    [[nodiscard]] bool intersectionRayPlane(
        const Ray3<T>& ray,
        const Vec3<T>& normal,
        T plane_distance,
        Vec3<T>& hit_point)
    {
        const T denominator = dot(ray.direction, normal);
        if (denominator == T{})
            return false;

        const T hit_distance = -(dot(normal, ray.origin) + plane_distance) / denominator;
        if (hit_distance <= ray.minimumDistance || hit_distance >= ray.maximumDistance)
            return false;

        hit_point = ray.position(hit_distance);
        return true;
    }

    template<typename T>
    [[nodiscard]] bool intersectionRayRay(
        const Ray3<T>& first_ray,
        const Ray3<T>& second_ray,
        Vec3<T>& hit_point)
    {
        const Vec3<T> cross_direction = cross(first_ray.direction, second_ray.direction);
        const T denominator = cross_direction.squaredLength();
        if (denominator == T{})
            return false;

        const Vec3<T> origin_delta = second_ray.origin - first_ray.origin;
        const T first_distance = Matrix33<T>(
            origin_delta.x, second_ray.direction.x, cross_direction.x,
            origin_delta.y, second_ray.direction.y, cross_direction.y,
            origin_delta.z, second_ray.direction.z, cross_direction.z).determinant() / denominator;
        const T second_distance = Matrix33<T>(
            origin_delta.x, first_ray.direction.x, cross_direction.x,
            origin_delta.y, first_ray.direction.y, cross_direction.y,
            origin_delta.z, first_ray.direction.z, cross_direction.z).determinant() / denominator;

        if (!first_ray.contains(first_distance) || !second_ray.contains(second_distance))
            return false;

        hit_point = second_ray.position(second_distance);
        return true;
    }

    template<typename T>
    [[nodiscard]] bool intersectionRaySphere(
        const Ray3<T>& ray,
        T radius,
        T& near_distance,
        T& far_distance)
    {
        if (radius < T{})
            return false;

        const T direction_length_squared = dot(ray.direction, ray.direction);
        if (direction_length_squared == T{})
            return false;

        const T half_linear = dot(ray.direction, ray.origin);
        const T constant = dot(ray.origin, ray.origin) - radius * radius;
        const T discriminant = half_linear * half_linear
            - direction_length_squared * constant;
        if (discriminant < T{})
            return false;

        using std::sqrt;
        const T square_root = static_cast<T>(sqrt(discriminant));
        near_distance = (-half_linear - square_root) / direction_length_squared;
        far_distance = (-half_linear + square_root) / direction_length_squared;
        if (near_distance > far_distance)
            std::swap(near_distance, far_distance);
        return true;
    }

    template<typename T>
    [[nodiscard]] bool intersectionRayBox(
        const Ray3<T>& ray,
        const BoundingBox3<T>& bounds,
        T& near_distance,
        T& far_distance)
    {
        T interval_minimum = ray.minimumDistance;
        T interval_maximum = ray.maximumDistance;

        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            const T direction = ray.direction[axis];
            const T origin = ray.origin[axis];
            const T slab_minimum = bounds.minPoint[axis];
            const T slab_maximum = bounds.maxPoint[axis];

            if (direction == T{})
            {
                if (origin < slab_minimum || origin > slab_maximum)
                    return false;
                continue;
            }

            const T inverse_direction = T{1} / direction;
            T axis_minimum = (slab_minimum - origin) * inverse_direction;
            T axis_maximum = (slab_maximum - origin) * inverse_direction;
            if (axis_minimum > axis_maximum)
                std::swap(axis_minimum, axis_maximum);

            interval_minimum = std::max(interval_minimum, axis_minimum);
            interval_maximum = std::min(interval_maximum, axis_maximum);
            if (interval_minimum > interval_maximum)
                return false;
        }

        near_distance = interval_minimum;
        far_distance = interval_maximum;
        return true;
    }

    using Ray3f = Ray3<float>;
    using Ray3d = Ray3<double>;
}
