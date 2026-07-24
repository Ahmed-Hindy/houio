#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

#include <houio/math/Vec3.h>

namespace houio::math
{
    template<typename T>
    class Matrix44
    {
    public:
        static constexpr std::size_t dimension = 4;
        std::array<T, dimension * dimension> ma{
            T{1}, T{}, T{}, T{},
            T{}, T{1}, T{}, T{},
            T{}, T{}, T{1}, T{},
            T{}, T{}, T{}, T{1}};

        constexpr Matrix44() noexcept = default;

        constexpr Matrix44(
            T value00,
            T value01,
            T value02,
            T value03,
            T value10,
            T value11,
            T value12,
            T value13,
            T value20,
            T value21,
            T value22,
            T value23,
            T value30,
            T value31,
            T value32,
            T value33) noexcept
            : ma{
                  value00, value01, value02, value03,
                  value10, value11, value12, value13,
                  value20, value21, value22, value23,
                  value30, value31, value32, value33}
        {
        }

        constexpr Matrix44(
            const Vec3<T>& right,
            const Vec3<T>& up,
            const Vec3<T>& forward,
            const Vec3<T>& translation_value = Vec3<T>{}) noexcept
            : Matrix44(
                  right.x, right.y, right.z, T{},
                  up.x, up.y, up.z, T{},
                  forward.x, forward.y, forward.z, T{},
                  translation_value.x, translation_value.y, translation_value.z, T{1})
        {
        }

        template<typename S>
        constexpr Matrix44(const Matrix44<S>& other) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] = static_cast<T>(other.ma[index]);
        }

        [[nodiscard]] static constexpr Matrix44 zero() noexcept
        {
            return Matrix44(
                T{}, T{}, T{}, T{},
                T{}, T{}, T{}, T{},
                T{}, T{}, T{}, T{},
                T{}, T{}, T{}, T{});
        }

        [[nodiscard]] static constexpr Matrix44 identity() noexcept
        {
            return Matrix44{};
        }

        [[nodiscard]] static Matrix44 rotationX(T angle)
            requires std::floating_point<T>
        {
            using std::cos;
            using std::sin;
            const T sine = static_cast<T>(sin(angle));
            const T cosine = static_cast<T>(cos(angle));
            return Matrix44(
                T{1}, T{}, T{}, T{},
                T{}, cosine, -sine, T{},
                T{}, sine, cosine, T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] static Matrix44 rotationY(T angle)
            requires std::floating_point<T>
        {
            using std::cos;
            using std::sin;
            const T sine = static_cast<T>(sin(angle));
            const T cosine = static_cast<T>(cos(angle));
            return Matrix44(
                cosine, T{}, sine, T{},
                T{}, T{1}, T{}, T{},
                -sine, T{}, cosine, T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] static Matrix44 rotationZ(T angle)
            requires std::floating_point<T>
        {
            using std::cos;
            using std::sin;
            const T sine = static_cast<T>(sin(angle));
            const T cosine = static_cast<T>(cos(angle));
            return Matrix44(
                cosine, -sine, T{}, T{},
                sine, cosine, T{}, T{},
                T{}, T{}, T{1}, T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] static Matrix44 axisRotation(const Vec3<T>& axis, T angle)
            requires std::floating_point<T>
        {
            if (axis.squaredLength() <= std::numeric_limits<T>::epsilon())
                throw std::invalid_argument("Matrix44 axis rotation requires a non-zero axis");

            const Vec3<T> normalized_axis = axis.normalized();
            using std::cos;
            using std::sin;
            const T cosine = static_cast<T>(cos(angle));
            const T sine = static_cast<T>(sin(angle));
            const T one_minus_cosine = T{1} - cosine;
            const T xy = one_minus_cosine * normalized_axis.x * normalized_axis.y;
            const T xz = one_minus_cosine * normalized_axis.x * normalized_axis.z;
            const T yz = one_minus_cosine * normalized_axis.y * normalized_axis.z;
            const T sx = sine * normalized_axis.x;
            const T sy = sine * normalized_axis.y;
            const T sz = sine * normalized_axis.z;
            return Matrix44(
                one_minus_cosine * normalized_axis.x * normalized_axis.x + cosine,
                xy - sz,
                xz + sy,
                T{},
                xy + sz,
                one_minus_cosine * normalized_axis.y * normalized_axis.y + cosine,
                yz - sx,
                T{},
                xz - sy,
                yz + sx,
                one_minus_cosine * normalized_axis.z * normalized_axis.z + cosine,
                T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] static Matrix44 rotationBetween(
            const Vec3<T>& from,
            const Vec3<T>& to)
            requires std::floating_point<T>
        {
            if (from.squaredLength() <= std::numeric_limits<T>::epsilon()
                || to.squaredLength() <= std::numeric_limits<T>::epsilon())
            {
                throw std::invalid_argument("Matrix44 rotationBetween requires non-zero vectors");
            }

            const Vec3<T> normalized_from = from.normalized();
            const Vec3<T> normalized_to = to.normalized();
            T cosine = normalized_from.x * normalized_to.x
                + normalized_from.y * normalized_to.y
                + normalized_from.z * normalized_to.z;
            cosine = std::clamp(cosine, T{-1}, T{1});

            const Vec3<T> axis(
                normalized_to.y * normalized_from.z - normalized_to.z * normalized_from.y,
                normalized_to.z * normalized_from.x - normalized_to.x * normalized_from.z,
                normalized_to.x * normalized_from.y - normalized_to.y * normalized_from.x);
            if (axis.squaredLength() > std::numeric_limits<T>::epsilon())
            {
                using std::acos;
                return axisRotation(axis, static_cast<T>(acos(cosine)));
            }
            if (cosine > T{})
                return identity();

            Vec3<T> perpendicular = std::abs(normalized_from.x) < std::abs(normalized_from.y)
                ? Vec3<T>(T{}, -normalized_from.z, normalized_from.y)
                : Vec3<T>(-normalized_from.z, T{}, normalized_from.x);
            if (perpendicular.squaredLength() <= std::numeric_limits<T>::epsilon())
                perpendicular = Vec3<T>(-normalized_from.y, normalized_from.x, T{});
            return axisRotation(perpendicular, std::numbers::pi_v<T>);
        }

        [[nodiscard]] static constexpr Matrix44 translationMatrix(
            const Vec3<T>& translation_value) noexcept
        {
            return translationMatrix(
                translation_value.x,
                translation_value.y,
                translation_value.z);
        }

        [[nodiscard]] static constexpr Matrix44 translationMatrix(T x, T y, T z) noexcept
        {
            return Matrix44(
                T{1}, T{}, T{}, T{},
                T{}, T{1}, T{}, T{},
                T{}, T{}, T{1}, T{},
                x, y, z, T{1});
        }

        [[nodiscard]] static constexpr Matrix44 scaleMatrix(T uniform_scale) noexcept
        {
            return scaleMatrix(uniform_scale, uniform_scale, uniform_scale);
        }

        [[nodiscard]] static constexpr Matrix44 scaleMatrix(T x, T y, T z) noexcept
        {
            return Matrix44(
                x, T{}, T{}, T{},
                T{}, y, T{}, T{},
                T{}, T{}, z, T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] static constexpr Matrix44 scaleMatrix(const Vec3<T>& scale_value) noexcept
        {
            return scaleMatrix(scale_value.x, scale_value.y, scale_value.z);
        }

        [[nodiscard]] constexpr T& operator()(std::size_t row, std::size_t column)
        {
            return ma.at(row * dimension + column);
        }

        [[nodiscard]] constexpr const T& operator()(
            std::size_t row,
            std::size_t column) const
        {
            return ma.at(row * dimension + column);
        }

        Matrix44& rotateX(T angle)
            requires std::floating_point<T>
        {
            *this = multiply(*this, rotationX(angle));
            return *this;
        }

        Matrix44& rotateY(T angle)
            requires std::floating_point<T>
        {
            *this = multiply(*this, rotationY(angle));
            return *this;
        }

        Matrix44& rotateZ(T angle)
            requires std::floating_point<T>
        {
            *this = multiply(*this, rotationZ(angle));
            return *this;
        }

        Matrix44& translate(const Vec3<T>& translation_value)
        {
            *this = multiply(*this, translationMatrix(translation_value));
            return *this;
        }

        Matrix44& translate(T x, T y, T z)
        {
            return translate(Vec3<T>(x, y, z));
        }

        Matrix44& scale(T uniform_scale)
        {
            *this = multiply(*this, scaleMatrix(uniform_scale));
            return *this;
        }

        Matrix44& scale(T x, T y, T z)
        {
            *this = multiply(*this, scaleMatrix(x, y, z));
            return *this;
        }

        Matrix44& scale(const Vec3<T>& scale_value)
        {
            *this = multiply(*this, scaleMatrix(scale_value));
            return *this;
        }

        constexpr void transpose() noexcept
        {
            for (std::size_t row = 0; row < dimension; ++row)
            {
                for (std::size_t column = row + 1; column < dimension; ++column)
                    std::swap((*this)(row, column), (*this)(column, row));
            }
        }

        [[nodiscard]] constexpr Matrix44 transposed() const noexcept
        {
            Matrix44 result = *this;
            result.transpose();
            return result;
        }

        [[nodiscard]] T determinant() const
            requires std::floating_point<T>
        {
            Matrix44 working = *this;
            T determinant_value = T{1};
            const T tolerance = pivotTolerance();
            using std::abs;

            for (std::size_t pivot_column = 0; pivot_column < dimension; ++pivot_column)
            {
                std::size_t pivot_row = pivot_column;
                for (std::size_t candidate = pivot_column + 1; candidate < dimension; ++candidate)
                {
                    if (abs(working(candidate, pivot_column))
                        > abs(working(pivot_row, pivot_column)))
                    {
                        pivot_row = candidate;
                    }
                }
                if (abs(working(pivot_row, pivot_column)) <= tolerance)
                    return T{};
                if (pivot_row != pivot_column)
                {
                    for (std::size_t column = 0; column < dimension; ++column)
                        std::swap(working(pivot_row, column), working(pivot_column, column));
                    determinant_value = -determinant_value;
                }

                const T pivot = working(pivot_column, pivot_column);
                determinant_value *= pivot;
                for (std::size_t row = pivot_column + 1; row < dimension; ++row)
                {
                    const T factor = working(row, pivot_column) / pivot;
                    for (std::size_t column = pivot_column + 1; column < dimension; ++column)
                    {
                        working(row, column)
                            -= factor * working(pivot_column, column);
                    }
                }
            }
            return determinant_value;
        }

        void invert()
            requires std::floating_point<T>
        {
            std::array<std::array<T, dimension * 2>, dimension> augmented{};
            for (std::size_t row = 0; row < dimension; ++row)
            {
                for (std::size_t column = 0; column < dimension; ++column)
                    augmented[row][column] = (*this)(row, column);
                augmented[row][dimension + row] = T{1};
            }

            const T tolerance = pivotTolerance();
            using std::abs;
            for (std::size_t pivot_column = 0; pivot_column < dimension; ++pivot_column)
            {
                std::size_t pivot_row = pivot_column;
                for (std::size_t candidate = pivot_column + 1; candidate < dimension; ++candidate)
                {
                    if (abs(augmented[candidate][pivot_column])
                        > abs(augmented[pivot_row][pivot_column]))
                    {
                        pivot_row = candidate;
                    }
                }
                if (abs(augmented[pivot_row][pivot_column]) <= tolerance)
                    throw std::domain_error("Matrix44 is singular");
                if (pivot_row != pivot_column)
                    std::swap(augmented[pivot_row], augmented[pivot_column]);

                const T pivot = augmented[pivot_column][pivot_column];
                for (T& value : augmented[pivot_column])
                    value /= pivot;

                for (std::size_t row = 0; row < dimension; ++row)
                {
                    if (row == pivot_column)
                        continue;
                    const T factor = augmented[row][pivot_column];
                    for (std::size_t column = 0; column < dimension * 2; ++column)
                    {
                        augmented[row][column]
                            -= factor * augmented[pivot_column][column];
                    }
                }
            }

            for (std::size_t row = 0; row < dimension; ++row)
            {
                for (std::size_t column = 0; column < dimension; ++column)
                    (*this)(row, column) = augmented[row][dimension + column];
            }
        }

        [[nodiscard]] Matrix44 inverted() const
            requires std::floating_point<T>
        {
            Matrix44 result = *this;
            result.invert();
            return result;
        }

        [[nodiscard]] Vec3<T> right(bool normalize_result = true) const
        {
            Vec3<T> result(ma[0], ma[1], ma[2]);
            if (normalize_result)
                result.normalize();
            return result;
        }

        [[nodiscard]] Vec3<T> up(bool normalize_result = true) const
        {
            Vec3<T> result(ma[4], ma[5], ma[6]);
            if (normalize_result)
                result.normalize();
            return result;
        }

        [[nodiscard]] Vec3<T> direction(bool normalize_result = true) const
        {
            Vec3<T> result(ma[8], ma[9], ma[10]);
            if (normalize_result)
                result.normalize();
            return result;
        }

        [[nodiscard]] constexpr Vec3<T> translation() const noexcept
        {
            return Vec3<T>(ma[12], ma[13], ma[14]);
        }

        [[nodiscard]] constexpr Matrix44 orientation() const noexcept
        {
            return Matrix44(
                ma[0], ma[1], ma[2], T{},
                ma[4], ma[5], ma[6], T{},
                ma[8], ma[9], ma[10], T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] Matrix44 normalizedOrientation() const
        {
            return Matrix44(right(), up(), direction());
        }

        constexpr void setRight(const Vec3<T>& right_value) noexcept
        {
            ma[0] = right_value.x;
            ma[1] = right_value.y;
            ma[2] = right_value.z;
        }

        constexpr void setUp(const Vec3<T>& up_value) noexcept
        {
            ma[4] = up_value.x;
            ma[5] = up_value.y;
            ma[6] = up_value.z;
        }

        constexpr void setDirection(const Vec3<T>& direction_value) noexcept
        {
            ma[8] = direction_value.x;
            ma[9] = direction_value.y;
            ma[10] = direction_value.z;
        }

        constexpr void setTranslation(const Vec3<T>& translation_value) noexcept
        {
            ma[12] = translation_value.x;
            ma[13] = translation_value.y;
            ma[14] = translation_value.z;
        }

        [[nodiscard]] constexpr bool operator==(const Matrix44& rhs) const noexcept
        {
            return ma == rhs.ma;
        }

        [[nodiscard]] constexpr bool operator!=(const Matrix44& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        constexpr Matrix44& operator+=(const Matrix44& rhs) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] += rhs.ma[index];
            return *this;
        }

        constexpr Matrix44& operator-=(const Matrix44& rhs) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] -= rhs.ma[index];
            return *this;
        }

        constexpr Matrix44& operator+=(T rhs) noexcept
        {
            for (T& value : ma)
                value += rhs;
            return *this;
        }

        constexpr Matrix44& operator-=(T rhs) noexcept
        {
            for (T& value : ma)
                value -= rhs;
            return *this;
        }

        constexpr Matrix44& operator*=(T rhs) noexcept
        {
            for (T& value : ma)
                value *= rhs;
            return *this;
        }

        constexpr Matrix44& operator/=(T rhs)
        {
            for (T& value : ma)
                value /= rhs;
            return *this;
        }

    private:
        [[nodiscard]] constexpr Matrix44 multiply(
            const Matrix44& lhs,
            const Matrix44& rhs) const noexcept
        {
            Matrix44 result = zero();
            for (std::size_t row = 0; row < dimension; ++row)
            {
                for (std::size_t column = 0; column < dimension; ++column)
                {
                    for (std::size_t inner = 0; inner < dimension; ++inner)
                        result(row, column) += lhs(row, inner) * rhs(inner, column);
                }
            }
            return result;
        }

        [[nodiscard]] T pivotTolerance() const
            requires std::floating_point<T>
        {
            using std::abs;
            T scale = T{1};
            for (const T value : ma)
                scale = std::max(scale, static_cast<T>(abs(value)));
            return std::numeric_limits<T>::epsilon() * scale * T{64};
        }
    };

    using Matrix44f = Matrix44<float>;
    using Matrix44d = Matrix44<double>;
    using M44f = Matrix44<float>;
    using M44d = Matrix44<double>;
}
