#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include <houio/math/Vec3.h>

namespace houio::math
{
    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator-(const Vec3<T>& value) noexcept
    {
        return Vec3<T>(-value.x, -value.y, -value.z);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator+(Vec3<T> lhs, const Vec3<T>& rhs) noexcept
    {
        return lhs += rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator-(Vec3<T> lhs, const Vec3<T>& rhs) noexcept
    {
        return lhs -= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator*(
        const Vec3<T>& lhs,
        const Vec3<T>& rhs) noexcept
    {
        return Vec3<T>(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator/(
        const Vec3<T>& lhs,
        const Vec3<T>& rhs)
    {
        return Vec3<T>(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator+(Vec3<T> lhs, T rhs) noexcept
    {
        return lhs += rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator-(Vec3<T> lhs, T rhs) noexcept
    {
        return lhs -= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator*(Vec3<T> lhs, T rhs) noexcept
    {
        return lhs *= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator/(Vec3<T> lhs, T rhs)
    {
        return lhs /= rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator+(T lhs, Vec3<T> rhs) noexcept
    {
        return rhs += lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator-(T lhs, const Vec3<T>& rhs) noexcept
    {
        return Vec3<T>(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator*(T lhs, Vec3<T> rhs) noexcept
    {
        return rhs *= lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator/(T lhs, const Vec3<T>& rhs)
    {
        return Vec3<T>(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z);
    }

    template<typename T>
    [[nodiscard]] constexpr T dot(const Vec3<T>& lhs, const Vec3<T>& rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> cross(
        const Vec3<T>& lhs,
        const Vec3<T>& rhs) noexcept
    {
        return Vec3<T>(
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> reflected(
        const Vec3<T>& incident,
        const Vec3<T>& normal) noexcept
    {
        return incident - normal * (static_cast<T>(2) * dot(incident, normal));
    }

    template<typename T>
    [[nodiscard]] std::size_t leastAbsoluteAxis(const Vec3<T>& value) noexcept
    {
        using std::abs;
        const T absolute_x = abs(value.x);
        const T absolute_y = abs(value.y);
        const T absolute_z = abs(value.z);
        if (absolute_x <= absolute_y && absolute_x <= absolute_z)
            return 0;
        return absolute_y <= absolute_z ? 1u : 2u;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> unitAxis3(std::size_t axis)
    {
        if (axis == 0)
            return Vec3<T>(T{1}, T{}, T{});
        if (axis == 1)
            return Vec3<T>(T{}, T{1}, T{});
        if (axis == 2)
            return Vec3<T>(T{}, T{}, T{1});
        throw std::out_of_range("Vec3 basis axis is out of range");
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> componentMin(
        const Vec3<T>& lhs,
        const Vec3<T>& rhs) noexcept
    {
        return Vec3<T>(
            std::min(lhs.x, rhs.x),
            std::min(lhs.y, rhs.y),
            std::min(lhs.z, rhs.z));
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> componentMax(
        const Vec3<T>& lhs,
        const Vec3<T>& rhs) noexcept
    {
        return Vec3<T>(
            std::max(lhs.x, rhs.x),
            std::max(lhs.y, rhs.y),
            std::max(lhs.z, rhs.z));
    }

    template<typename T>
    [[nodiscard]] std::pair<Vec3<T>, Vec3<T>> orthonormalBasis(
        const Vec3<T>& direction)
    {
        if (direction.squaredLength() == T{})
            throw std::invalid_argument("orthonormalBasis requires a non-zero direction");

        const Vec3<T> normal = direction.normalized();
        using std::abs;
        Vec3<T> tangent = abs(normal.x) > abs(normal.y)
            ? Vec3<T>(-normal.z, T{}, normal.x)
            : Vec3<T>(T{}, normal.z, -normal.y);
        tangent.normalize();
        return {tangent, cross(normal, tangent)};
    }
}
