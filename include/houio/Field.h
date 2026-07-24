#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <houio/math/Math.h>

namespace houio
{
    namespace detail
    {
        template<typename Value>
        bool readBinaryValue(std::istream& input, Value& value)
        {
            static_assert(std::is_trivially_copyable_v<Value>);
            input.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(Value)));
            return input.good();
        }

        template<typename Value>
        void writeBinaryValue(std::ostream& output, const Value& value)
        {
            static_assert(std::is_trivially_copyable_v<Value>);
            output.write(
                reinterpret_cast<const char*>(&value),
                static_cast<std::streamsize>(sizeof(Value)));
        }
    }

    template<typename T>
    class Field
    {
    public:
        using ValueType = T;
        using Ptr = std::shared_ptr<Field<T>>;
        using CPtr = std::shared_ptr<const Field<T>>;

        [[nodiscard]] static Ptr create(
            math::V3i resolution = math::V3i(10),
            math::M44f local_to_world = math::M44f());
        [[nodiscard]] static Ptr create(
            math::V3i resolution,
            math::Box3f bound,
            T initial_value = T());
        template<typename Source>
        [[nodiscard]] static Ptr create(const typename Field<Source>::Ptr& source);
        [[nodiscard]] static Ptr load(const std::string& filename);

        Field();

        [[nodiscard]] T evaluate(const math::V3f& voxel_position) const;
        [[nodiscard]] const T& voxel(int x, int y, int z) const;
        [[nodiscard]] T& voxel(int x, int y, int z);

        [[nodiscard]] math::V3i resolution() const noexcept;
        void resize(int x, int y, int z);
        void resize(math::V3i resolution);

        void setLocalToWorld(const math::M44f& local_to_world);
        void setBound(const math::Box3f& bound);

        [[nodiscard]] math::Box3f bound() const noexcept;
        [[nodiscard]] math::V3f voxelSize() const;
        [[nodiscard]] const math::M44f& localToWorldMatrix() const noexcept;

        [[nodiscard]] math::V3f worldToLocal(const math::V3f& world_position) const;
        [[nodiscard]] math::V3f localToWorld(const math::V3f& local_position) const;
        [[nodiscard]] math::V3f worldToVoxel(const math::V3f& world_position) const;
        [[nodiscard]] math::V3f voxelToWorld(const math::V3f& voxel_position) const;
        [[nodiscard]] math::V3f localToVoxel(const math::V3f& local_position) const;
        [[nodiscard]] math::V3f voxelToLocal(const math::V3f& voxel_position) const;

        [[nodiscard]] std::span<T> values() noexcept;
        [[nodiscard]] std::span<const T> values() const noexcept;

        void fill(const T& value);
        void fill(const T& value, const math::Box3f& world_bound);
        void multiply(const T& value);
        void store(const std::string& filename) const;
        void storeWithoutBoundingBox(const std::string& filename) const;

    private:
        [[nodiscard]] std::size_t linearIndex(int x, int y, int z) const;
        void updateVoxelTransforms();

        math::M44f local_to_world_ = math::M44f::identity();
        math::M44f world_to_local_ = math::M44f::identity();
        math::M44f world_to_voxel_ = math::M44f::identity();
        math::M44f voxel_to_world_ = math::M44f::identity();
        math::V3f sample_location_ = math::V3f(0.5f);
        math::V3i resolution_ = math::V3i(1);
        math::Box3f bound_;
        std::vector<T> data_;

        static const int data_type_;
    };

    template<typename T>
    typename Field<T>::Ptr Field<T>::create(math::V3i resolution, math::M44f local_to_world)
    {
        auto field = std::make_shared<Field<T>>();
        field->resize(resolution);
        field->setLocalToWorld(local_to_world);
        return field;
    }

    template<typename T>
    typename Field<T>::Ptr Field<T>::create(
        math::V3i resolution,
        math::Box3f bound,
        T initial_value)
    {
        auto field = std::make_shared<Field<T>>();
        field->resize(resolution);
        field->setBound(bound);
        field->fill(initial_value);
        return field;
    }

    template<typename T>
    template<typename Source>
    typename Field<T>::Ptr Field<T>::create(const typename Field<Source>::Ptr& source)
    {
        if (!source)
            throw std::invalid_argument("Field::create received a null source");

        auto destination = Field<T>::create(source->resolution(), source->bound());
        const std::span<const Source> source_values = source->values();
        std::span<T> destination_values = destination->values();
        if (source_values.size() != destination_values.size())
            throw std::runtime_error("Field conversion produced inconsistent storage sizes");

        std::transform(
            source_values.begin(),
            source_values.end(),
            destination_values.begin(),
            [](const Source& value) { return static_cast<T>(value); });
        return destination;
    }

    template<typename T>
    Field<T>::Field()
    {
        resize(math::V3i(1));
        setLocalToWorld(math::M44f::identity());
    }

    template<typename T>
    typename Field<T>::Ptr Field<T>::load(const std::string& filename)
    {
        std::ifstream input(filename, std::ios::binary);
        if (!input)
            return nullptr;

        int resolution_x = 0;
        int resolution_y = 0;
        int resolution_z = 0;
        float minimum_x = 0.0f;
        float minimum_y = 0.0f;
        float minimum_z = 0.0f;
        float maximum_x = 0.0f;
        float maximum_y = 0.0f;
        float maximum_z = 0.0f;
        int stored_data_type = 0;

        if (!detail::readBinaryValue(input, resolution_x)
            || !detail::readBinaryValue(input, resolution_y)
            || !detail::readBinaryValue(input, resolution_z)
            || !detail::readBinaryValue(input, minimum_x)
            || !detail::readBinaryValue(input, minimum_y)
            || !detail::readBinaryValue(input, minimum_z)
            || !detail::readBinaryValue(input, maximum_x)
            || !detail::readBinaryValue(input, maximum_y)
            || !detail::readBinaryValue(input, maximum_z)
            || !detail::readBinaryValue(input, stored_data_type)
            || stored_data_type != data_type_)
        {
            return nullptr;
        }

        auto field = std::make_shared<Field<T>>();
        try
        {
            field->resize(math::V3i(resolution_x, resolution_y, resolution_z));
        }
        catch (const std::exception&)
        {
            return nullptr;
        }

        const std::size_t byte_count = field->data_.size() * sizeof(T);
        if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
            return nullptr;
        if (byte_count > 0)
        {
            input.read(
                reinterpret_cast<char*>(field->data_.data()),
                static_cast<std::streamsize>(byte_count));
            if (input.gcount() != static_cast<std::streamsize>(byte_count))
                return nullptr;
        }

        field->setBound(math::Box3f(
            math::V3f(minimum_x, minimum_y, minimum_z),
            math::V3f(maximum_x, maximum_y, maximum_z)));
        return field;
    }

    template<typename T>
    void Field<T>::store(const std::string& filename) const
    {
        std::ofstream output(filename, std::ios::binary | std::ios::trunc);
        if (!output)
            return;

        detail::writeBinaryValue(output, resolution_.x);
        detail::writeBinaryValue(output, resolution_.y);
        detail::writeBinaryValue(output, resolution_.z);
        detail::writeBinaryValue(output, bound_.minPoint.x);
        detail::writeBinaryValue(output, bound_.minPoint.y);
        detail::writeBinaryValue(output, bound_.minPoint.z);
        detail::writeBinaryValue(output, bound_.maxPoint.x);
        detail::writeBinaryValue(output, bound_.maxPoint.y);
        detail::writeBinaryValue(output, bound_.maxPoint.z);
        detail::writeBinaryValue(output, data_type_);

        const std::size_t byte_count = data_.size() * sizeof(T);
        if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
            throw std::length_error("Field storage exceeds streamsize range");
        if (byte_count > 0)
            output.write(
                reinterpret_cast<const char*>(data_.data()),
                static_cast<std::streamsize>(byte_count));
    }

    template<typename T>
    void Field<T>::storeWithoutBoundingBox(const std::string& filename) const
    {
        std::ofstream output(filename, std::ios::binary | std::ios::trunc);
        if (!output)
            return;

        detail::writeBinaryValue(output, resolution_.x);
        detail::writeBinaryValue(output, resolution_.y);
        detail::writeBinaryValue(output, resolution_.z);
        detail::writeBinaryValue(output, data_type_);

        const std::size_t byte_count = data_.size() * sizeof(T);
        if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
            throw std::length_error("Field storage exceeds streamsize range");
        if (byte_count > 0)
            output.write(
                reinterpret_cast<const char*>(data_.data()),
                static_cast<std::streamsize>(byte_count));
    }

    template<typename T>
    void Field<T>::resize(int x, int y, int z)
    {
        resize(math::V3i(x, y, z));
    }

    template<typename T>
    void Field<T>::resize(math::V3i resolution)
    {
        if (resolution.x < 0 || resolution.y < 0 || resolution.z < 0)
            throw std::invalid_argument("Field resolution cannot be negative");

        const std::size_t x = static_cast<std::size_t>(resolution.x);
        const std::size_t y = static_cast<std::size_t>(resolution.y);
        const std::size_t z = static_cast<std::size_t>(resolution.z);
        if (x != 0 && y > std::numeric_limits<std::size_t>::max() / x)
            throw std::length_error("Field resolution exceeds addressable storage");
        const std::size_t xy = x * y;
        if (xy != 0 && z > std::numeric_limits<std::size_t>::max() / xy)
            throw std::length_error("Field resolution exceeds addressable storage");

        resolution_ = resolution;
        data_.assign(xy * z, T());
        updateVoxelTransforms();
    }

    template<typename T>
    std::size_t Field<T>::linearIndex(int x, int y, int z) const
    {
        if (x < 0 || y < 0 || z < 0
            || x >= resolution_.x || y >= resolution_.y || z >= resolution_.z)
        {
            throw std::out_of_range("Field voxel coordinate is out of range");
        }
        return static_cast<std::size_t>(z) * static_cast<std::size_t>(resolution_.x)
                * static_cast<std::size_t>(resolution_.y)
            + static_cast<std::size_t>(y) * static_cast<std::size_t>(resolution_.x)
            + static_cast<std::size_t>(x);
    }

    template<typename T>
    const T& Field<T>::voxel(int x, int y, int z) const
    {
        return data_.at(linearIndex(x, y, z));
    }

    template<typename T>
    T& Field<T>::voxel(int x, int y, int z)
    {
        return data_.at(linearIndex(x, y, z));
    }

    template<typename T>
    T Field<T>::evaluate(const math::V3f& voxel_position) const
    {
        if (data_.empty())
            throw std::runtime_error("Field::evaluate requires non-empty storage");

        using Real = float;
        math::Vec3<Real> position = voxel_position - sample_location_;
        const Real fraction_x = position.x - std::floor(position.x);
        const Real fraction_y = position.y - std::floor(position.y);
        const Real fraction_z = position.z - std::floor(position.z);

        math::V3i lower(
            static_cast<int>(std::floor(position.x)),
            static_cast<int>(std::floor(position.y)),
            static_cast<int>(std::floor(position.z)));
        math::V3i upper = lower + math::V3i(1);

        lower.x = std::clamp(lower.x, 0, resolution_.x - 1);
        lower.y = std::clamp(lower.y, 0, resolution_.y - 1);
        lower.z = std::clamp(lower.z, 0, resolution_.z - 1);
        upper.x = std::clamp(upper.x, 0, resolution_.x - 1);
        upper.y = std::clamp(upper.y, 0, resolution_.y - 1);
        upper.z = std::clamp(upper.z, 0, resolution_.z - 1);

        const T lower_plane = math::lerp(
            math::lerp(
                voxel(lower.x, lower.y, lower.z),
                voxel(upper.x, lower.y, lower.z),
                fraction_x),
            math::lerp(
                voxel(lower.x, upper.y, lower.z),
                voxel(upper.x, upper.y, lower.z),
                fraction_x),
            fraction_y);
        const T upper_plane = math::lerp(
            math::lerp(
                voxel(lower.x, lower.y, upper.z),
                voxel(upper.x, lower.y, upper.z),
                fraction_x),
            math::lerp(
                voxel(lower.x, upper.y, upper.z),
                voxel(upper.x, upper.y, upper.z),
                fraction_x),
            fraction_y);
        return math::lerp(lower_plane, upper_plane, fraction_z);
    }

    template<typename T>
    math::V3i Field<T>::resolution() const noexcept
    {
        return resolution_;
    }

    template<typename T>
    math::V3f Field<T>::voxelSize() const
    {
        if (resolution_.x == 0 || resolution_.y == 0 || resolution_.z == 0)
            return math::V3f(0.0f);
        const math::V3f minimum = voxelToWorld(math::V3f(0.0f));
        const math::V3f maximum = voxelToWorld(math::V3f(1.0f));
        return maximum - minimum;
    }

    template<typename T>
    const math::M44f& Field<T>::localToWorldMatrix() const noexcept
    {
        return local_to_world_;
    }

    template<typename T>
    std::span<T> Field<T>::values() noexcept
    {
        return data_;
    }

    template<typename T>
    std::span<const T> Field<T>::values() const noexcept
    {
        return data_;
    }

    template<typename T>
    void Field<T>::updateVoxelTransforms()
    {
        if (resolution_.x == 0 || resolution_.y == 0 || resolution_.z == 0)
        {
            world_to_voxel_ = math::M44f::identity();
            voxel_to_world_ = math::M44f::identity();
            return;
        }
        world_to_voxel_ = world_to_local_ * math::M44f().scale(math::V3f(resolution_));
        voxel_to_world_ = world_to_voxel_.inverted();
    }

    template<typename T>
    void Field<T>::setLocalToWorld(const math::M44f& local_to_world)
    {
        local_to_world_ = local_to_world;
        world_to_local_ = local_to_world_.inverted();
        updateVoxelTransforms();

        bound_.reset();
        for (int z = 0; z <= 1; ++z)
            for (int y = 0; y <= 1; ++y)
                for (int x = 0; x <= 1; ++x)
                    bound_.extend(math::V3f(float(x), float(y), float(z)) * local_to_world_);
    }

    template<typename T>
    void Field<T>::setBound(const math::Box3f& bound)
    {
        bound_ = bound;
        const math::V3f dimensions = bound_.size();
        local_to_world_ = math::M44f().scale(dimensions)
            * math::M44f().translate(bound_.minPoint);
        world_to_local_ = local_to_world_.inverted();
        updateVoxelTransforms();
    }

    template<typename T>
    math::V3f Field<T>::worldToVoxel(const math::V3f& world_position) const
    {
        return world_position * world_to_voxel_;
    }

    template<typename T>
    math::V3f Field<T>::voxelToWorld(const math::V3f& voxel_position) const
    {
        return voxel_position * voxel_to_world_;
    }

    template<typename T>
    math::V3f Field<T>::worldToLocal(const math::V3f& world_position) const
    {
        return world_position * world_to_local_;
    }

    template<typename T>
    math::V3f Field<T>::localToWorld(const math::V3f& local_position) const
    {
        return local_position * local_to_world_;
    }

    template<typename T>
    math::V3f Field<T>::voxelToLocal(const math::V3f& voxel_position) const
    {
        if (resolution_.x == 0 || resolution_.y == 0 || resolution_.z == 0)
            throw std::runtime_error("Field::voxelToLocal requires non-zero resolution");
        return voxel_position * math::M44f::scaleMatrix(
            1.0f / float(resolution_.x),
            1.0f / float(resolution_.y),
            1.0f / float(resolution_.z));
    }

    template<typename T>
    math::V3f Field<T>::localToVoxel(const math::V3f& local_position) const
    {
        return local_position * math::M44f::scaleMatrix(
            float(resolution_.x),
            float(resolution_.y),
            float(resolution_.z));
    }

    template<typename T>
    math::Box3f Field<T>::bound() const noexcept
    {
        return bound_;
    }

    template<typename T>
    void Field<T>::fill(const T& value)
    {
        std::fill(data_.begin(), data_.end(), value);
    }

    template<typename T>
    void Field<T>::fill(const T& value, const math::Box3f& world_bound)
    {
        for (int z = 0; z < resolution_.z; ++z)
            for (int y = 0; y < resolution_.y; ++y)
                for (int x = 0; x < resolution_.x; ++x)
                {
                    const math::V3f voxel_position(
                        float(x) + 0.5f,
                        float(y) + 0.5f,
                        float(z) + 0.5f);
                    if (world_bound.encloses(voxelToWorld(voxel_position)))
                        voxel(x, y, z) = value;
                }
    }

    template<typename T>
    void Field<T>::multiply(const T& value)
    {
        for (T& voxel : data_)
            voxel *= value;
    }

    template<typename T>
    T field_maximum(const Field<T>& field)
    {
        const std::span<const T> field_values = field.values();
        if (field_values.empty())
            throw std::invalid_argument("field_maximum requires a non-empty field");
        return *std::max_element(field_values.begin(), field_values.end());
    }

    template<typename T>
    void field_range(const Field<T>& field, T& minimum, T& maximum)
    {
        const std::span<const T> field_values = field.values();
        if (field_values.empty())
            throw std::invalid_argument("field_range requires a non-empty field");
        const auto [minimum_iterator, maximum_iterator] =
            std::minmax_element(field_values.begin(), field_values.end());
        minimum = *minimum_iterator;
        maximum = *maximum_iterator;
    }

    template<typename T>
    void field_writeplot(
        const Field<T>& field,
        math::V3f world_start,
        math::V3f world_end,
        int sample_count,
        std::ofstream& output)
    {
        if (sample_count <= 0)
            throw std::invalid_argument("field_writeplot requires a positive sample count");
        const math::Ray3f ray(
            world_start,
            (world_end - world_start).normalized());
        const float step = (world_end - world_start).length() / float(sample_count);
        for (int sample_index = 0; sample_index < sample_count; ++sample_index)
        {
            const float distance = step * float(sample_index);
            const T value = field.evaluate(field.worldToVoxel(ray(distance)));
            output << distance << ' ' << value << '\n';
        }
    }

    template<typename T>
    void field_writeplot(
        const Field<math::Vec3<T>>& field,
        math::V3f world_start,
        math::V3f world_end,
        int sample_count,
        std::ofstream& output,
        int component)
    {
        if (component < 0 || component >= 3)
            throw std::out_of_range("field_writeplot vector component is out of range");
        if (sample_count <= 0)
            throw std::invalid_argument("field_writeplot requires a positive sample count");
        const math::Ray3f ray(
            world_start,
            (world_end - world_start).normalized());
        const float step = (world_end - world_start).length() / float(sample_count);
        for (int sample_index = 0; sample_index < sample_count; ++sample_index)
        {
            const float distance = step * float(sample_index);
            const math::Vec3<T> value = field.evaluate(field.worldToVoxel(ray(distance)));
            output << distance << ' ' << value[component] << '\n';
        }
    }

    template<typename T, typename Real = float>
    struct LinearFieldInterp
    {
        [[nodiscard]] static T eval(const Field<T>& field, const math::V3f& voxel_position)
        {
            return field.evaluate(voxel_position);
        }
    };

    using Fieldf = Field<float>;
    using ScalarField = Field<float>;
    using Field3f = Field<math::V3f>;
    using VectorField = Field<math::V3f>;
    using Fieldd = Field<double>;
    using Field3d = Field<math::V3d>;
}
