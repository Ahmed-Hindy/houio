#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <houio/math/Matrix44.h>
#include <houio/math/Vec3Algo.h>

namespace houio::math
{
    template<typename T>
    struct AxisAngle
    {
        Vec3<T> axis{T{1}, T{}, T{}};
        T angle{};
    };

    enum class PolarAxis
    {
        unchanged,
        x,
        z,
    };

    template<typename T>
    [[nodiscard]] Vec3<T> eulerAnglesXYZ(const Matrix44<T>& matrix)
    {
        const Matrix44<T> normalized = matrix.normalizedOrientation();
        using std::abs;
        using std::asin;
        using std::atan2;
        using std::cos;

        Vec3<T> rotation;
        const T sine_y = std::clamp(normalized(0, 2), T{-1}, T{1});
        rotation.y = static_cast<T>(asin(sine_y));
        const T cosine_y = static_cast<T>(cos(rotation.y));
        if (abs(cosine_y) > std::numeric_limits<T>::epsilon() * T{16})
        {
            rotation.x = static_cast<T>(atan2(-normalized(1, 2), normalized(2, 2)));
            rotation.z = static_cast<T>(atan2(-normalized(0, 1), normalized(0, 0)));
        }
        else
        {
            rotation.x = static_cast<T>(atan2(sine_y * normalized(1, 0), normalized(1, 1)));
            rotation.z = T{};
        }
        return rotation;
    }

    template<typename T>
    [[nodiscard]] AxisAngle<T> axisAngle(const Matrix44<T>& matrix)
    {
        using std::abs;
        using std::acos;
        using std::sqrt;

        AxisAngle<T> result;
        const T cosine = std::clamp(
            (matrix(0, 0) + matrix(1, 1) + matrix(2, 2) - T{1}) / T{2},
            T{-1},
            T{1});
        result.angle = static_cast<T>(acos(cosine));

        constexpr T epsilon = static_cast<T>(1.0e-6);
        if (abs(result.angle) <= epsilon)
        {
            result.angle = T{};
            return result;
        }

        if (abs(result.angle - std::numbers::pi_v<T>) <= static_cast<T>(1.0e-4))
        {
            const T xx = std::max(T{}, (matrix(0, 0) + T{1}) / T{2});
            const T yy = std::max(T{}, (matrix(1, 1) + T{1}) / T{2});
            const T zz = std::max(T{}, (matrix(2, 2) + T{1}) / T{2});
            result.axis = Vec3<T>(
                static_cast<T>(sqrt(xx)),
                static_cast<T>(sqrt(yy)),
                static_cast<T>(sqrt(zz)));
            if (matrix(0, 1) > T{})
                result.axis.y = -result.axis.y;
            if (matrix(0, 2) > T{})
                result.axis.z = -result.axis.z;
            if (result.axis.squaredLength() <= epsilon)
                result.axis = Vec3<T>(T{1}, T{}, T{});
            else
                result.axis.normalize();
            return result;
        }

        result.axis = Vec3<T>(
            matrix(2, 1) - matrix(1, 2),
            matrix(0, 2) - matrix(2, 0),
            matrix(1, 0) - matrix(0, 1));
        if (result.axis.squaredLength() <= epsilon)
            result.axis = Vec3<T>(T{1}, T{}, T{});
        else
            result.axis.normalize();
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator+(
        Matrix44<T> lhs,
        const Matrix44<T>& rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator-(
        Matrix44<T> lhs,
        const Matrix44<T>& rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator+(Matrix44<T> lhs, T rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator-(Matrix44<T> lhs, T rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator*(Matrix44<T> lhs, T rhs) noexcept
    {
        lhs *= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator/(Matrix44<T> lhs, T rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator+(T lhs, Matrix44<T> rhs) noexcept
    {
        rhs += lhs;
        return rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator-(T lhs, const Matrix44<T>& rhs) noexcept
    {
        Matrix44<T> result;
        for (std::size_t index = 0; index < result.ma.size(); ++index)
            result.ma[index] = lhs - rhs.ma[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator*(T lhs, Matrix44<T> rhs) noexcept
    {
        rhs *= lhs;
        return rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator/(T lhs, const Matrix44<T>& rhs)
    {
        Matrix44<T> result;
        for (std::size_t index = 0; index < result.ma.size(); ++index)
            result.ma[index] = lhs / rhs.ma[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator-(const Matrix44<T>& matrix) noexcept
    {
        Matrix44<T> result = matrix;
        result *= T{-1};
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator*(
        const Matrix44<T>& lhs,
        const Matrix44<T>& rhs) noexcept
    {
        Matrix44<T> result = Matrix44<T>::zero();
        for (std::size_t row = 0; row < Matrix44<T>::dimension; ++row)
        {
            for (std::size_t column = 0; column < Matrix44<T>::dimension; ++column)
            {
                for (std::size_t inner = 0; inner < Matrix44<T>::dimension; ++inner)
                    result(row, column) += lhs(row, inner) * rhs(inner, column);
            }
        }
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> transform(
        const Vec3<T>& vector,
        const Matrix44<T>& matrix)
    {
        return Vec3<T>(
            vector.x * matrix(0, 0) + vector.y * matrix(1, 0)
                + vector.z * matrix(2, 0) + matrix(3, 0),
            vector.x * matrix(0, 1) + vector.y * matrix(1, 1)
                + vector.z * matrix(2, 1) + matrix(3, 1),
            vector.x * matrix(0, 2) + vector.y * matrix(1, 2)
                + vector.z * matrix(2, 2) + matrix(3, 2));
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator*(
        const Vec3<T>& vector,
        const Matrix44<T>& matrix)
    {
        return transform(vector, matrix);
    }

    template<typename T>
    [[nodiscard]] T frobeniusNorm(const Matrix44<T>& matrix)
    {
        T squared{};
        for (const T& value : matrix.ma)
            squared += value * value;
        using std::sqrt;
        return static_cast<T>(sqrt(squared));
    }

    template<typename T>
    [[nodiscard]] Matrix44<T> lookAtTransform(
        const Vec3<T>& position,
        const Vec3<T>& target,
        const Vec3<T>& up_hint,
        bool orientation_only = false)
    {
        Vec3<T> forward = position - target;
        if (forward.squaredLength() <= std::numeric_limits<T>::epsilon())
            throw std::invalid_argument("Look-at position and target must differ");
        forward.normalize();

        Vec3<T> right = cross(up_hint, forward);
        if (right.squaredLength() <= std::numeric_limits<T>::epsilon())
            throw std::invalid_argument("Look-at up vector is parallel to the view direction");
        right.normalize();
        const Vec3<T> up = cross(forward, right).normalized();

        Matrix44<T> result(right, up, forward);
        if (!orientation_only)
            result.setTranslation(position);
        return result;
    }

    template<typename T>
    [[nodiscard]] Matrix44<T> polarTransform(
        T azimuth,
        T elevation,
        T distance,
        PolarAxis axis = PolarAxis::z,
        bool orientation_only = false)
    {
        if (axis == PolarAxis::unchanged)
            return Matrix44<T>::identity();

        Matrix44<T> matrix = Matrix44<T>::identity();
        if (!orientation_only)
            matrix.translate(Vec3<T>(T{}, distance, T{}));
        matrix.rotateZ(elevation).rotateY(azimuth);

        if (axis == PolarAxis::x)
        {
            return Matrix44<T>(
                -matrix.direction(),
                -matrix.right(),
                matrix.up(),
                matrix.translation());
        }
        return matrix;
    }

    template<typename T>
    [[nodiscard]] Matrix44<T> transformFromDirection(
        const Vec3<T>& direction,
        const Vec3<T>& translation = Vec3<T>{})
    {
        const Vec3<T> normal = direction.normalized();
        if (normal.squaredLength() <= std::numeric_limits<T>::epsilon())
            throw std::invalid_argument("transformFromDirection requires a non-zero direction");
        const auto [tangent, bitangent] = orthonormalBasis(normal);
        return Matrix44<T>(tangent, bitangent, normal, translation);
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> orthographicProjection(
        T left,
        T right,
        T bottom,
        T top,
        T near_plane,
        T far_plane)
    {
        if (right == left || top == bottom || far_plane == near_plane)
            throw std::invalid_argument("Orthographic projection bounds must have non-zero extent");
        return Matrix44<T>(
            T{2} / (right - left), T{}, T{}, T{},
            T{}, T{2} / (top - bottom), T{}, T{},
            T{}, T{}, T{-2} / (far_plane - near_plane), T{},
            -(right + left) / (right - left),
            -(top + bottom) / (top - bottom),
            -(far_plane + near_plane) / (far_plane - near_plane),
            T{1});
    }

    template<typename T>
    [[nodiscard]] Matrix44<T> perspectiveProjection(
        T field_of_view,
        T aspect_ratio,
        T near_plane,
        T far_plane)
    {
        if (!(field_of_view > T{} && field_of_view < std::numbers::pi_v<T>))
            throw std::invalid_argument("Perspective field of view must be between zero and pi");
        if (!(aspect_ratio > T{}))
            throw std::invalid_argument("Perspective aspect ratio must be positive");
        if (!(near_plane > T{} && far_plane > near_plane))
            throw std::invalid_argument("Perspective clipping planes must satisfy 0 < near < far");

        using std::tan;
        const T height = T{2} * static_cast<T>(tan(field_of_view / T{2}));
        const T width = height * aspect_ratio;
        const T scale = (far_plane + near_plane) / (far_plane - near_plane);
        const T offset = T{2} * far_plane * near_plane / (far_plane - near_plane);
        return Matrix44<T>(
            T{2} / width, T{}, T{}, T{},
            T{}, T{2} / height, T{}, T{},
            T{}, T{}, -scale, T{-1},
            T{}, T{}, -offset, T{});
    }
}
