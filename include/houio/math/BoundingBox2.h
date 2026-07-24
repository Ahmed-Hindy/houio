#pragma once

#include <algorithm>
#include <limits>

#include <houio/math/Vec2.h>

namespace houio::math
{
    template<typename T>
    struct BoundingBox2
    {
        Vec2<T> minPoint;
        Vec2<T> maxPoint;

        constexpr BoundingBox2() noexcept
        {
            reset();
        }

        constexpr BoundingBox2(Vec2<T> minimum, Vec2<T> maximum) noexcept
            : minPoint(minimum), maxPoint(maximum)
        {
        }

        [[nodiscard]] constexpr Vec2<T> size() const noexcept
        {
            return maxPoint - minPoint;
        }

        [[nodiscard]] constexpr Vec2<T> center() const noexcept
        {
            return Vec2<T>(
                minPoint.x + (maxPoint.x - minPoint.x) / static_cast<T>(2),
                minPoint.y + (maxPoint.y - minPoint.y) / static_cast<T>(2));
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return minPoint.x >= maxPoint.x || minPoint.y >= maxPoint.y;
        }

        constexpr void reset() noexcept
        {
            const T maximum = std::numeric_limits<T>::max();
            const T minimum = std::numeric_limits<T>::lowest();
            minPoint = Vec2<T>(maximum);
            maxPoint = Vec2<T>(minimum);
        }

        constexpr void extend(const Vec2<T>& point) noexcept
        {
            minPoint.x = std::min(minPoint.x, point.x);
            minPoint.y = std::min(minPoint.y, point.y);
            maxPoint.x = std::max(maxPoint.x, point.x);
            maxPoint.y = std::max(maxPoint.y, point.y);
        }

        constexpr void extend(const BoundingBox2& bounds) noexcept
        {
            extend(bounds.minPoint);
            extend(bounds.maxPoint);
        }

        [[nodiscard]] constexpr bool encloses(const Vec2<T>& point) const noexcept
        {
            return point.x > minPoint.x && point.x < maxPoint.x
                && point.y > minPoint.y && point.y < maxPoint.y;
        }

        [[nodiscard]] constexpr bool encloses(
            const Vec2<T>& minimum,
            const Vec2<T>& maximum) const noexcept
        {
            return minimum.x >= minPoint.x && minimum.y >= minPoint.y
                && maximum.x <= maxPoint.x && maximum.y <= maxPoint.y;
        }
    };

    using BoundingBox2f = BoundingBox2<float>;
    using BoundingBox2d = BoundingBox2<double>;
    using Box2f = BoundingBox2<float>;
    using Box2d = BoundingBox2<double>;
}
