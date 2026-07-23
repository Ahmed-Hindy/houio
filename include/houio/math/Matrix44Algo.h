#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include <houio/math/Matrix44.h>
#include <houio/math/Vec3Algo.h>

namespace houio::math
{
    template<typename T>
    void extractEulerXYZ(const Matrix44<T>& matrix, Vec3<T>& rotation)
    {
        Vec3<T> right(matrix(0, 0), matrix(0, 1), matrix(0, 2));
        Vec3<T> up(matrix(1, 0), matrix(1, 1), matrix(1, 2));
        Vec3<T> direction(matrix(2, 0), matrix(2, 1), matrix(2, 2));
        right.normalize();
        up.normalize();
        direction.normalize();

        const Matrix44<T> normalized(right, up, direction);
        using std::atan2;
        using std::sqrt;
        rotation.x = static_cast<T>(atan2(normalized(1, 2), normalized(2, 2)));

        Matrix44<T> remove_x = Matrix44<T>::RotationMatrixX(-rotation.x);
        const Matrix44<T> remaining = Matrix44<T>::multiplied(remove_x, normalized);
        const T cosine_y = static_cast<T>(sqrt(
            remaining(0, 0) * remaining(0, 0)
            + remaining(0, 1) * remaining(0, 1)));
        rotation.y = static_cast<T>(atan2(-remaining(0, 2), cosine_y));
        rotation.z = static_cast<T>(atan2(-remaining(1, 0), remaining(1, 1)));
    }

    template<typename T>
    void extractAxisAngle(const Matrix44<T>& matrix, Vec3<T>& axis, T& angle)
    {
        using std::acos;
        using std::abs;
        using std::sqrt;

        const T cosine = std::clamp(
            (matrix(0, 0) + matrix(1, 1) + matrix(2, 2) - T{1}) / T{2},
            T{-1},
            T{1});
        angle = static_cast<T>(acos(cosine));

        constexpr T epsilon = static_cast<T>(1.0e-6);
        if (abs(angle) <= epsilon)
        {
            axis = Vec3<T>(T{1}, T{}, T{});
            angle = T{};
            return;
        }

        if (abs(angle - std::numbers::pi_v<T>) <= static_cast<T>(1.0e-4))
        {
            const T xx = std::max(T{}, (matrix(0, 0) + T{1}) / T{2});
            const T yy = std::max(T{}, (matrix(1, 1) + T{1}) / T{2});
            const T zz = std::max(T{}, (matrix(2, 2) + T{1}) / T{2});
            axis = Vec3<T>(
                static_cast<T>(sqrt(xx)),
                static_cast<T>(sqrt(yy)),
                static_cast<T>(sqrt(zz)));
            if (matrix(0, 1) < T{})
                axis.y = -axis.y;
            if (matrix(0, 2) < T{})
                axis.z = -axis.z;
            if (axis.getSquaredLength() <= epsilon)
                axis = Vec3<T>(T{1}, T{}, T{});
            else
                axis.normalize();
            return;
        }

        axis = Vec3<T>(
            matrix(1, 2) - matrix(2, 1),
            matrix(2, 0) - matrix(0, 2),
            matrix(0, 1) - matrix(1, 0));
        axis.normalize();
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
    [[nodiscard]] constexpr Matrix44<T> operator+(Matrix44<T> lhs, const T& rhs) noexcept
    {
        lhs += rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator-(Matrix44<T> lhs, const T& rhs) noexcept
    {
        lhs -= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator*(Matrix44<T> lhs, const T& rhs) noexcept
    {
        lhs *= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator/(Matrix44<T> lhs, const T& rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator+(const T& lhs, Matrix44<T> rhs) noexcept
    {
        rhs += lhs;
        return rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator-(const T& lhs, const Matrix44<T>& rhs) noexcept
    {
        Matrix44<T> result;
        for (std::size_t index = 0; index < result.ma.size(); ++index)
            result.ma[index] = lhs - rhs.ma[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator*(const T& lhs, Matrix44<T> rhs) noexcept
    {
        rhs *= lhs;
        return rhs;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator/(const T& lhs, const Matrix44<T>& rhs)
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
    constexpr void matrixMultiply(
        Matrix44<T>& result,
        const Matrix44<T>& lhs,
        const Matrix44<T>& rhs) noexcept
    {
        result = Matrix44<T>::multiplied(lhs, rhs);
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
    constexpr void transform(
        Vec3<T>& result,
        const Vec3<T>& vector,
        const Matrix44<T>& matrix)
    {
        result = transform(vector, matrix);
    }

    template<typename T>
    [[nodiscard]] constexpr Vec3<T> operator*(
        const Vec3<T>& vector,
        const Matrix44<T>& matrix)
    {
        return transform(vector, matrix);
    }

    template<typename T>
    constexpr Matrix44<T>& transpose(Matrix44<T>& matrix) noexcept
    {
        matrix.transpose();
        return matrix;
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> transpose(const Matrix44<T>& matrix) noexcept
    {
        return matrix.transposed();
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> operator*(
        const Matrix44<T>& lhs,
        const Matrix44<T>& rhs) noexcept
    {
        return Matrix44<T>::multiplied(lhs, rhs);
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
    [[nodiscard]] Matrix44<T> createLookAtMatrix(
        const Vec3<T>& position,
        const Vec3<T>& target,
        const Vec3<T>& up_hint,
        bool orientation_only)
    {
        Vec3<T> forward = position - target;
        if (forward.getSquaredLength() == T{})
            throw std::invalid_argument("Look-at position and target must differ");
        forward.normalize();

        Vec3<T> right = cross(up_hint, forward);
        if (right.getSquaredLength() == T{})
            throw std::invalid_argument("Look-at up vector is parallel to the view direction");
        right.normalize();
        Vec3<T> up = cross(forward, right);
        up.normalize();

        Matrix44<T> result(right, up, forward);
        if (!orientation_only)
            result.setTranslation(position);
        return result;
    }

    template<typename T>
    [[nodiscard]] Matrix44<T> createMatrixFromPolarCoordinates(
        const T& azimuth,
        const T& elevation,
        const T& distance,
        int axis = 2,
        bool orientation_only = false)
    {
        Matrix44<T> matrix = Matrix44<T>::Identity();
        if (axis == 0)
            return matrix;

        if (!orientation_only)
            matrix.translate(Vec3<T>(T{}, distance, T{}));
        matrix.rotateZ(elevation);
        matrix.rotateY(azimuth);

        if (axis == 1)
        {
            return Matrix44<T>(
                -matrix.getDir(),
                -matrix.getRight(),
                matrix.getUp(),
                matrix.getTranslation());
        }
        return matrix;
    }

    template<typename T>
    void basisFromVector(const Vec3<T>& direction, Vec3<T>* tangent, Vec3<T>* bitangent)
    {
        if (!tangent || !bitangent)
            throw std::invalid_argument("basisFromVector requires both output vectors");

        Vec3<T> normal = direction;
        if (normal.getSquaredLength() == T{})
            throw std::invalid_argument("basisFromVector requires a non-zero direction");
        normal.normalize();

        using std::abs;
        using std::sqrt;
        if (abs(normal.x) > abs(normal.y))
        {
            const T inverse_length = T{1}
                / static_cast<T>(sqrt(normal.x * normal.x + normal.z * normal.z));
            *bitangent = Vec3<T>(-normal.z * inverse_length, T{}, normal.x * inverse_length);
        }
        else
        {
            const T inverse_length = T{1}
                / static_cast<T>(sqrt(normal.y * normal.y + normal.z * normal.z));
            *bitangent = Vec3<T>(T{}, normal.z * inverse_length, -normal.y * inverse_length);
        }
        *tangent = cross(*bitangent, normal);
        tangent->normalize();
    }

    template<typename T>
    [[nodiscard]] Matrix44<T> transformFromVector(
        const Vec3<T>& direction,
        const Vec3<T>& translation = Vec3<T>{})
    {
        Vec3<T> tangent;
        Vec3<T> bitangent;
        Vec3<T> normal = direction;
        if (normal.getSquaredLength() == T{})
            throw std::invalid_argument("transformFromVector requires a non-zero direction");
        normal.normalize();
        basisFromVector(normal, &tangent, &bitangent);
        return Matrix44<T>(tangent, bitangent, normal, translation);
    }

    template<typename T>
    [[nodiscard]] constexpr Matrix44<T> orthographicProjectionMatrix(
        T left,
        T right,
        T bottom,
        T top,
        T near_plane,
        T far_plane)
    {
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
    [[nodiscard]] Matrix44<T> projectionMatrix(
        T field_of_view,
        T aspect_ratio,
        T near_plane,
        T far_plane)
    {
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
