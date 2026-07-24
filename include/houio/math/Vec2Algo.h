#pragma once

#include <algorithm>

#include <houio/math/Vec2.h>

namespace houio::math
{
    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator-(const Vec2<T>& value) noexcept
    {
        return Vec2<T>(-value.x, -value.y);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator+(Vec2<T> lhs, const Vec2<T>& rhs) noexcept
    {
        return lhs += rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator-(Vec2<T> lhs, const Vec2<T>& rhs) noexcept
    {
        return lhs -= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator*(
        const Vec2<T>& lhs,
        const Vec2<T>& rhs) noexcept
    {
        return Vec2<T>(lhs.x * rhs.x, lhs.y * rhs.y);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator/(
        const Vec2<T>& lhs,
        const Vec2<T>& rhs)
    {
        return Vec2<T>(lhs.x / rhs.x, lhs.y / rhs.y);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator+(Vec2<T> lhs, T rhs) noexcept
    {
        return lhs += rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator-(Vec2<T> lhs, T rhs) noexcept
    {
        return lhs -= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator*(Vec2<T> lhs, T rhs) noexcept
    {
        return lhs *= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator/(Vec2<T> lhs, T rhs)
    {
        return lhs /= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator+(T lhs, Vec2<T> rhs) noexcept
    {
        return rhs += lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator-(T lhs, const Vec2<T>& rhs) noexcept
    {
        return Vec2<T>(lhs - rhs.x, lhs - rhs.y);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator*(T lhs, Vec2<T> rhs) noexcept
    {
        return rhs *= lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> operator/(T lhs, const Vec2<T>& rhs)
    {
        return Vec2<T>(lhs / rhs.x, lhs / rhs.y);
    }

    template<typename T>
    [[nodiscard]] constexpr T dot(const Vec2<T>& lhs, const Vec2<T>& rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y;
    }

    template<typename T>
    [[nodiscard]] constexpr T cross(const Vec2<T>& lhs, const Vec2<T>& rhs) noexcept
    {
        return lhs.x * rhs.y - lhs.y * rhs.x;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> componentMin(
        const Vec2<T>& lhs,
        const Vec2<T>& rhs) noexcept
    {
        return Vec2<T>(std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y));
    }

    template<typename T>
    [[nodiscard]] constexpr Vec2<T> componentMax(
        const Vec2<T>& lhs,
        const Vec2<T>& rhs) noexcept
    {
        return Vec2<T>(std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y));
    }
}
