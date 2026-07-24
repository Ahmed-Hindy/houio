#pragma once

#include <cmath>
#include <ostream>
#include <stdexcept>
#include <type_traits>

namespace houio::math
{
    template<typename T>
    struct Vec3
    {
        T x{};
        T y{};
        T z{};

        constexpr Vec3() noexcept = default;
        constexpr Vec3(const T& x_value, const T& y_value, const T& z_value) noexcept
            : x(x_value), y(y_value), z(z_value)
        {
        }
        explicit constexpr Vec3(const T& value) noexcept
            : x(value), y(value), z(value)
        {
        }

        template<typename S>
        constexpr Vec3(const Vec3<S>& value) noexcept
            : x(static_cast<T>(value.x)),
              y(static_cast<T>(value.y)),
              z(static_cast<T>(value.z))
        {
        }

        constexpr void set(const T& x_value, const T& y_value, const T& z_value) noexcept
        {
            x = x_value;
            y = y_value;
            z = z_value;
        }

        [[nodiscard]] T length() const
        {
            using std::sqrt;
            return static_cast<T>(sqrt(x * x + y * y + z * z));
        }

        [[nodiscard]] constexpr T squaredLength() const noexcept
        {
            return x * x + y * y + z * z;
        }

        void setLength(const T& length)
        {
            normalize();
            x *= length;
            y *= length;
            z *= length;
        }

        void normalize()
        {
            const T length = this->length();
            if (length != T{})
            {
                x /= length;
                y /= length;
                z /= length;
            }
        }

        [[nodiscard]] Vec3 normalized() const
        {
            const T length = this->length();
            return length == T{} ? Vec3{} : Vec3(x / length, y / length, z / length);
        }

        constexpr void negate() noexcept
        {
            x = -x;
            y = -y;
            z = -z;
        }

        void reflect(const Vec3& normal) noexcept
        {
            const T scale = static_cast<T>(2)
                * (normal.x * x + normal.y * y + normal.z * z);
            x -= normal.x * scale;
            y -= normal.y * scale;
            z -= normal.z * scale;
        }

        [[nodiscard]] constexpr bool operator==(const Vec3& rhs) const noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                constexpr T tolerance = static_cast<T>(0.00001);
                return std::abs(x - rhs.x) < tolerance
                    && std::abs(y - rhs.y) < tolerance
                    && std::abs(z - rhs.z) < tolerance;
            }
            return x == rhs.x && y == rhs.y && z == rhs.z;
        }

        [[nodiscard]] constexpr bool operator!=(const Vec3& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr Vec3& operator+=(const Vec3& rhs) noexcept
        {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
            return *this;
        }

        constexpr Vec3& operator-=(const Vec3& rhs) noexcept
        {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
            return *this;
        }

        constexpr Vec3& operator+=(const T& rhs) noexcept
        {
            x += rhs;
            y += rhs;
            z += rhs;
            return *this;
        }

        constexpr Vec3& operator-=(const T& rhs) noexcept
        {
            x -= rhs;
            y -= rhs;
            z -= rhs;
            return *this;
        }

        constexpr Vec3& operator*=(const T& rhs) noexcept
        {
            x *= rhs;
            y *= rhs;
            z *= rhs;
            return *this;
        }

        constexpr Vec3& operator/=(const T& rhs)
        {
            x /= rhs;
            y /= rhs;
            z /= rhs;
            return *this;
        }

        [[nodiscard]] constexpr const T& operator[](std::size_t index) const
        {
            if (index == 0)
                return x;
            if (index == 1)
                return y;
            if (index == 2)
                return z;
            throw std::out_of_range("Vec3 index is out of range");
        }

        [[nodiscard]] constexpr T& operator[](std::size_t index)
        {
            if (index == 0)
                return x;
            if (index == 1)
                return y;
            if (index == 2)
                return z;
            throw std::out_of_range("Vec3 index is out of range");
        }
    };

    template<typename T>
    std::ostream& operator<<(std::ostream& stream, const Vec3<T>& value)
    {
        return stream << '(' << value.x << ' ' << value.y << ' ' << value.z << ')';
    }

    using Vec3f = Vec3<float>;
    using Vec3d = Vec3<double>;
    using Vec3i = Vec3<int>;
    using V3f = Vec3<float>;
    using V3d = Vec3<double>;
    using V3i = Vec3<int>;
}
