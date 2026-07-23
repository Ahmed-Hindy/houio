#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

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
            const T& value00,
            const T& value01,
            const T& value02,
            const T& value03,
            const T& value10,
            const T& value11,
            const T& value12,
            const T& value13,
            const T& value20,
            const T& value21,
            const T& value22,
            const T& value23,
            const T& value30,
            const T& value31,
            const T& value32,
            const T& value33) noexcept
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
            const Vec3<T>& translation = Vec3<T>{}) noexcept
            : Matrix44(
                  right.x, right.y, right.z, T{},
                  up.x, up.y, up.z, T{},
                  forward.x, forward.y, forward.z, T{},
                  translation.x, translation.y, translation.z, T{1})
        {
        }

        template<typename S>
        constexpr Matrix44(const Matrix44<S>& other) noexcept
        {
            for (std::size_t index = 0; index < ma.size(); ++index)
                ma[index] = static_cast<T>(other.ma[index]);
        }

        [[nodiscard]] static constexpr Matrix44 Zero() noexcept
        {
            return Matrix44(
                T{}, T{}, T{}, T{},
                T{}, T{}, T{}, T{},
                T{}, T{}, T{}, T{},
                T{}, T{}, T{}, T{});
        }

        [[nodiscard]] static constexpr Matrix44 Identity() noexcept
        {
            return Matrix44{};
        }

        [[nodiscard]] static Matrix44 RotationMatrixX(const T& angle)
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

        [[nodiscard]] static Matrix44 RotationMatrixY(const T& angle)
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

        [[nodiscard]] static Matrix44 RotationMatrixZ(const T& angle)
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

        [[nodiscard]] static Matrix44 RotationMatrix(const Vec3<T>& axis, const T& angle)
        {
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
                xy + sz,
                xz - sy,
                T{},
                xy - sz,
                one_minus_cosine * normalized_axis.y * normalized_axis.y + cosine,
                yz + sx,
                T{},
                xz + sy,
                yz - sx,
                one_minus_cosine * normalized_axis.z * normalized_axis.z + cosine,
                T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] static Matrix44 RotationMatrix(const Vec3<T>& from, const Vec3<T>& to)
        {
            const Vec3<T> normalized_from = from.normalized();
            const Vec3<T> normalized_to = to.normalized();
            T cosine = normalized_from.x * normalized_to.x
                + normalized_from.y * normalized_to.y
                + normalized_from.z * normalized_to.z;
            cosine = std::clamp(cosine, T{-1}, T{1});

            const Vec3<T> axis(
                normalized_from.y * normalized_to.z - normalized_from.z * normalized_to.y,
                normalized_from.z * normalized_to.x - normalized_from.x * normalized_to.z,
                normalized_from.x * normalized_to.y - normalized_from.y * normalized_to.x);
            const T axis_length = axis.getLength();
            using std::acos;
            if (axis_length > static_cast<T>(1.0e-8))
            {
                Vec3<T> normalized_axis = axis;
                normalized_axis.normalize();
                return RotationMatrix(normalized_axis, static_cast<T>(acos(cosine)));
            }
            if (cosine > T{})
                return Identity();

            Vec3<T> perpendicular = std::abs(normalized_from.x) < std::abs(normalized_from.y)
                ? Vec3<T>(T{}, -normalized_from.z, normalized_from.y)
                : Vec3<T>(-normalized_from.z, T{}, normalized_from.x);
            perpendicular.normalize();
            return RotationMatrix(perpendicular, static_cast<T>(acos(T{-1})));
        }

        [[nodiscard]] static constexpr Matrix44 TranslationMatrix(
            const Vec3<T>& translation) noexcept
        {
            return TranslationMatrix(translation.x, translation.y, translation.z);
        }

        [[nodiscard]] static constexpr Matrix44 TranslationMatrix(
            const T& x,
            const T& y,
            const T& z) noexcept
        {
            return Matrix44(
                T{1}, T{}, T{}, T{},
                T{}, T{1}, T{}, T{},
                T{}, T{}, T{1}, T{},
                x, y, z, T{1});
        }

        [[nodiscard]] static constexpr Matrix44 ScaleMatrix(const T& uniform_scale) noexcept
        {
            return ScaleMatrix(uniform_scale, uniform_scale, uniform_scale);
        }

        [[nodiscard]] static constexpr Matrix44 ScaleMatrix(
            const T& x,
            const T& y,
            const T& z) noexcept
        {
            return Matrix44(
                x, T{}, T{}, T{},
                T{}, y, T{}, T{},
                T{}, T{}, z, T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] static constexpr Matrix44 ScaleMatrix(const Vec3<T>& scale) noexcept
        {
            return ScaleMatrix(scale.x, scale.y, scale.z);
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

        void rotateX(const T& angle)
        {
            *this = multiplied(*this, RotationMatrixX(angle));
        }

        void rotateY(const T& angle)
        {
            *this = multiplied(*this, RotationMatrixY(angle));
        }

        void rotateZ(const T& angle)
        {
            *this = multiplied(*this, RotationMatrixZ(angle));
        }

        Matrix44& translate(const Vec3<T>& translation)
        {
            *this = multiplied(*this, TranslationMatrix(translation));
            return *this;
        }

        void translate(const T& x, const T& y, const T& z)
        {
            translate(Vec3<T>(x, y, z));
        }

        void scale(const T& uniform_scale)
        {
            *this = multiplied(*this, ScaleMatrix(uniform_scale));
        }

        void scale(const T& x, const T& y, const T& z)
        {
            *this = multiplied(*this, ScaleMatrix(x, y, z));
        }

        Matrix44& scale(const Vec3<T>& scale_value)
        {
            *this = multiplied(*this, ScaleMatrix(scale_value));
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

        void invert()
        {
            std::array<std::array<T, dimension * 2>, dimension> augmented{};
            for (std::size_t row = 0; row < dimension; ++row)
            {
                for (std::size_t column = 0; column < dimension; ++column)
                    augmented[row][column] = (*this)(row, column);
                augmented[row][dimension + row] = T{1};
            }

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
                if (abs(augmented[pivot_row][pivot_column]) <= static_cast<T>(1.0e-12))
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
        {
            Matrix44 result = *this;
            result.invert();
            return result;
        }

        [[nodiscard]] Matrix44 inverse() const
        {
            return inverted();
        }

        [[nodiscard]] Vec3<T> getRight(const bool& normalized = true) const
        {
            Vec3<T> result(ma[0], ma[1], ma[2]);
            if (normalized)
                result.normalize();
            return result;
        }

        [[nodiscard]] Vec3<T> getUp(const bool& normalized = true) const
        {
            Vec3<T> result(ma[4], ma[5], ma[6]);
            if (normalized)
                result.normalize();
            return result;
        }

        [[nodiscard]] Vec3<T> getDir(const bool& normalized = true) const
        {
            Vec3<T> result(ma[8], ma[9], ma[10]);
            if (normalized)
                result.normalize();
            return result;
        }

        [[nodiscard]] constexpr Vec3<T> getTranslation() const noexcept
        {
            return Vec3<T>(ma[12], ma[13], ma[14]);
        }

        [[nodiscard]] constexpr Matrix44 getOrientation() const noexcept
        {
            return Matrix44(
                ma[0], ma[1], ma[2], T{},
                ma[4], ma[5], ma[6], T{},
                ma[8], ma[9], ma[10], T{},
                T{}, T{}, T{}, T{1});
        }

        [[nodiscard]] Matrix44 getNormalizedOrientation() const
        {
            return Matrix44(getRight(), getUp(), getDir());
        }

        [[nodiscard]] constexpr Matrix44 getTransposed() const noexcept
        {
            return transposed();
        }

        constexpr void setRight(const Vec3<T>& right) noexcept
        {
            ma[0] = right.x;
            ma[1] = right.y;
            ma[2] = right.z;
        }

        constexpr void setUp(const Vec3<T>& up) noexcept
        {
            ma[4] = up.x;
            ma[5] = up.y;
            ma[6] = up.z;
        }

        constexpr void setDir(const Vec3<T>& direction) noexcept
        {
            ma[8] = direction.x;
            ma[9] = direction.y;
            ma[10] = direction.z;
        }

        constexpr void setTranslation(const Vec3<T>& translation) noexcept
        {
            ma[12] = translation.x;
            ma[13] = translation.y;
            ma[14] = translation.z;
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

        constexpr Matrix44& operator+=(const T& rhs) noexcept
        {
            for (T& value : ma)
                value += rhs;
            return *this;
        }

        constexpr Matrix44& operator-=(const T& rhs) noexcept
        {
            for (T& value : ma)
                value -= rhs;
            return *this;
        }

        constexpr Matrix44& operator*=(const T& rhs) noexcept
        {
            for (T& value : ma)
                value *= rhs;
            return *this;
        }

        constexpr Matrix44& operator/=(const T& rhs)
        {
            for (T& value : ma)
                value /= rhs;
            return *this;
        }

        [[nodiscard]] static constexpr Matrix44 multiplied(
            const Matrix44& lhs,
            const Matrix44& rhs) noexcept
        {
            Matrix44 result = Zero();
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
    };

    using Matrix44f = Matrix44<float>;
    using Matrix44d = Matrix44<double>;
    using M44f = Matrix44<float>;
    using M44d = Matrix44<double>;
}
