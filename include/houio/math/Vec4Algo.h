#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <stdexcept>

#include <houio/math/Vec4.h>

namespace houio::math
{
    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator-(const Vec4<T>& value) noexcept
    {
        return Vec4<T>(-value.x, -value.y, -value.z, -value.w);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator+(Vec4<T> lhs, const Vec4<T>& rhs) noexcept
    {
        return lhs += rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator-(Vec4<T> lhs, const Vec4<T>& rhs) noexcept
    {
        return lhs -= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator*(
        const Vec4<T>& lhs,
        const Vec4<T>& rhs) noexcept
    {
        return Vec4<T>(
            lhs.x * rhs.x,
            lhs.y * rhs.y,
            lhs.z * rhs.z,
            lhs.w * rhs.w);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator/(
        const Vec4<T>& lhs,
        const Vec4<T>& rhs)
    {
        return Vec4<T>(
            lhs.x / rhs.x,
            lhs.y / rhs.y,
            lhs.z / rhs.z,
            lhs.w / rhs.w);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator+(Vec4<T> lhs, T rhs) noexcept
    {
        return lhs += rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator-(Vec4<T> lhs, T rhs) noexcept
    {
        return lhs -= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator*(Vec4<T> lhs, T rhs) noexcept
    {
        return lhs *= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator/(Vec4<T> lhs, T rhs)
    {
        return lhs /= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator+(T lhs, Vec4<T> rhs) noexcept
    {
        return rhs += lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator-(T lhs, const Vec4<T>& rhs) noexcept
    {
        return Vec4<T>(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z, lhs - rhs.w);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator*(T lhs, Vec4<T> rhs) noexcept
    {
        return rhs *= lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> operator/(T lhs, const Vec4<T>& rhs)
    {
        return Vec4<T>(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z, lhs / rhs.w);
    }

    template<typename T>
    [[nodiscard]] constexpr T dot(const Vec4<T>& lhs, const Vec4<T>& rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> reflected(
        const Vec4<T>& incident,
        const Vec4<T>& normal) noexcept
    {
        return incident - normal * (static_cast<T>(2) * dot(incident, normal));
    }

    template<typename T>
    [[nodiscard]] std::size_t leastAbsoluteAxis(const Vec4<T>& value) noexcept
    {
        using std::abs;
        const T components[] = {abs(value.x), abs(value.y), abs(value.z), abs(value.w)};
        return static_cast<std::size_t>(
            std::min_element(std::begin(components), std::end(components)) - components);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> unitAxis4(std::size_t axis)
    {
        if (axis == 0)
            return Vec4<T>(T{1}, T{}, T{}, T{});
        if (axis == 1)
            return Vec4<T>(T{}, T{1}, T{}, T{});
        if (axis == 2)
            return Vec4<T>(T{}, T{}, T{1}, T{});
        if (axis == 3)
            return Vec4<T>(T{}, T{}, T{}, T{1});
        throw std::out_of_range("Vec4 basis axis is out of range");
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> componentMin(
        const Vec4<T>& lhs,
        const Vec4<T>& rhs) noexcept
    {
        return Vec4<T>(
            std::min(lhs.x, rhs.x),
            std::min(lhs.y, rhs.y),
            std::min(lhs.z, rhs.z),
            std::min(lhs.w, rhs.w));
    }

    template<typename T>
    [[nodiscard]] constexpr Vec4<T> componentMax(
        const Vec4<T>& lhs,
        const Vec4<T>& rhs) noexcept
    {
        return Vec4<T>(
            std::max(lhs.x, rhs.x),
            std::max(lhs.y, rhs.y),
            std::max(lhs.z, rhs.z),
            std::max(lhs.w, rhs.w));
    }
}
