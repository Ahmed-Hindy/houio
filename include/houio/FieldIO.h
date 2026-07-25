#pragma once

#include <fstream>
#include <limits>
#include <string>
#include <type_traits>

#include <houio/Field.h>

namespace houio
{
    namespace detail
    {
        template<typename Value>
        bool readFieldBinaryValue(std::istream& input, Value& value)
        {
            static_assert(std::is_trivially_copyable_v<Value>);
            input.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(Value)));
            return input.good();
        }

        template<typename Value>
        bool writeFieldBinaryValue(std::ostream& output, const Value& value)
        {
            static_assert(std::is_trivially_copyable_v<Value>);
            output.write(
                reinterpret_cast<const char*>(&value),
                static_cast<std::streamsize>(sizeof(Value)));
            return output.good();
        }

        template<typename Value>
        struct FieldStorageCode;

        template<>
        struct FieldStorageCode<float>
        {
            static constexpr int value = 1;
        };

        template<>
        struct FieldStorageCode<math::V3f>
        {
            static constexpr int value = 2;
        };

        template<>
        struct FieldStorageCode<double>
        {
            static constexpr int value = 3;
        };

        template<>
        struct FieldStorageCode<math::V3d>
        {
            static constexpr int value = 4;
        };

        template<typename Value>
        inline constexpr int fieldStorageCode = FieldStorageCode<Value>::value;

        template<typename Value>
        bool writeFieldPayload(std::ostream& output, std::span<const Value> values)
        {
            static_assert(std::is_trivially_copyable_v<Value>);
            if (values.size() > std::numeric_limits<std::size_t>::max() / sizeof(Value))
                throw std::length_error("Field storage byte count overflow");
            const std::size_t byte_count = values.size() * sizeof(Value);
            if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
                throw std::length_error("Field storage exceeds streamsize range");
            if (byte_count > 0)
            {
                output.write(
                    reinterpret_cast<const char*>(values.data()),
                    static_cast<std::streamsize>(byte_count));
            }
            return output.good();
        }
    }

    template<typename T>
    [[nodiscard]] typename Field<T>::Ptr loadField(const std::string& filename)
    {
        static_assert(std::is_trivially_copyable_v<T>);
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

        if (!detail::readFieldBinaryValue(input, resolution_x)
            || !detail::readFieldBinaryValue(input, resolution_y)
            || !detail::readFieldBinaryValue(input, resolution_z)
            || !detail::readFieldBinaryValue(input, minimum_x)
            || !detail::readFieldBinaryValue(input, minimum_y)
            || !detail::readFieldBinaryValue(input, minimum_z)
            || !detail::readFieldBinaryValue(input, maximum_x)
            || !detail::readFieldBinaryValue(input, maximum_y)
            || !detail::readFieldBinaryValue(input, maximum_z)
            || !detail::readFieldBinaryValue(input, stored_data_type)
            || stored_data_type != detail::fieldStorageCode<T>)
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

        std::span<T> values = field->values();
        if (values.size() > std::numeric_limits<std::size_t>::max() / sizeof(T))
            return nullptr;
        const std::size_t byte_count = values.size() * sizeof(T);
        if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
            return nullptr;
        if (byte_count > 0)
        {
            input.read(
                reinterpret_cast<char*>(values.data()),
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
    [[nodiscard]] bool storeField(const Field<T>& field, const std::string& filename)
    {
        std::ofstream output(filename, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;

        const math::V3i resolution = field.resolution();
        const math::Box3f bound = field.bound();
        return detail::writeFieldBinaryValue(output, resolution.x)
            && detail::writeFieldBinaryValue(output, resolution.y)
            && detail::writeFieldBinaryValue(output, resolution.z)
            && detail::writeFieldBinaryValue(output, bound.minPoint.x)
            && detail::writeFieldBinaryValue(output, bound.minPoint.y)
            && detail::writeFieldBinaryValue(output, bound.minPoint.z)
            && detail::writeFieldBinaryValue(output, bound.maxPoint.x)
            && detail::writeFieldBinaryValue(output, bound.maxPoint.y)
            && detail::writeFieldBinaryValue(output, bound.maxPoint.z)
            && detail::writeFieldBinaryValue(output, detail::fieldStorageCode<T>)
            && detail::writeFieldPayload(output, field.values());
    }

    template<typename T>
    [[nodiscard]] bool storeFieldWithoutBoundingBox(const Field<T>& field, const std::string& filename)
    {
        std::ofstream output(filename, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;

        const math::V3i resolution = field.resolution();
        return detail::writeFieldBinaryValue(output, resolution.x)
            && detail::writeFieldBinaryValue(output, resolution.y)
            && detail::writeFieldBinaryValue(output, resolution.z)
            && detail::writeFieldBinaryValue(output, detail::fieldStorageCode<T>)
            && detail::writeFieldPayload(output, field.values());
    }
}
