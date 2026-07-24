#pragma once

#include <algorithm>
#include <limits>

#include <houio/math/Vec3.h>

namespace houio::math
{
    template<typename T>
    struct BoundingBox3
    {
        Vec3<T> minPoint;
        Vec3<T> maxPoint;

        constexpr BoundingBox3() noexcept
        {
            reset();
        }

        constexpr BoundingBox3(Vec3<T> minimum, Vec3<T> maximum) noexcept
            : minPoint(minimum), maxPoint(maximum)
        {
        }

        constexpr BoundingBox3(
            T minimum_x,
            T minimum_y,
            T minimum_z,
            T maximum_x,
            T maximum_y,
            T maximum_z) noexcept
            : minPoint(minimum_x, minimum_y, minimum_z),
              maxPoint(maximum_x, maximum_y, maximum_z)
        {
        }

        template<typename S>
        constexpr BoundingBox3(const BoundingBox3<S>& other) noexcept
            : minPoint(other.minPoint), maxPoint(other.maxPoint)
        {
        }

        [[nodiscard]] constexpr Vec3<T> size() const noexcept
        {
            return maxPoint - minPoint;
        }

        [[nodiscard]] constexpr Vec3<T> center() const noexcept
        {
            return Vec3<T>(
                minPoint.x + (maxPoint.x - minPoint.x) / static_cast<T>(2),
                minPoint.y + (maxPoint.y - minPoint.y) / static_cast<T>(2),
                minPoint.z + (maxPoint.z - minPoint.z) / static_cast<T>(2));
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return minPoint.x >= maxPoint.x
                || minPoint.y >= maxPoint.y
                || minPoint.z >= maxPoint.z;
        }

        constexpr void reset() noexcept
        {
            const T maximum = std::numeric_limits<T>::max();
            const T minimum = std::numeric_limits<T>::lowest();
            minPoint = Vec3<T>(maximum);
            maxPoint = Vec3<T>(minimum);
        }

        [[nodiscard]] constexpr int longestAxis() const noexcept
        {
            const Vec3<T> diagonal = maxPoint - minPoint;
            if (diagonal.x > diagonal.y && diagonal.x > diagonal.z)
                return 0;
            return diagonal.y > diagonal.z ? 1 : 2;
        }

        constexpr void extend(const Vec3<T>& point) noexcept
        {
            minPoint.x = std::min(minPoint.x, point.x);
            minPoint.y = std::min(minPoint.y, point.y);
            minPoint.z = std::min(minPoint.z, point.z);
            maxPoint.x = std::max(maxPoint.x, point.x);
            maxPoint.y = std::max(maxPoint.y, point.y);
            maxPoint.z = std::max(maxPoint.z, point.z);
        }

        constexpr void extend(const BoundingBox3& bounds) noexcept
        {
            extend(bounds.minPoint);
            extend(bounds.maxPoint);
        }

        [[nodiscard]] constexpr bool encloses(const Vec3<T>& point) const noexcept
        {
            return point.x > minPoint.x && point.x < maxPoint.x
                && point.y > minPoint.y && point.y < maxPoint.y
                && point.z > minPoint.z && point.z < maxPoint.z;
        }

        [[nodiscard]] constexpr bool encloses(
            const Vec3<T>& minimum,
            const Vec3<T>& maximum) const noexcept
        {
            return minimum.x >= minPoint.x
                && minimum.y >= minPoint.y
                && minimum.z >= minPoint.z
                && maximum.x <= maxPoint.x
                && maximum.y <= maxPoint.y
                && maximum.z <= maxPoint.z;
        }
    };

    using BoundingBox3f = BoundingBox3<float>;
    using BoundingBox3d = BoundingBox3<double>;
    using Box3f = BoundingBox3<float>;
    using Box3d = BoundingBox3<double>;
}
