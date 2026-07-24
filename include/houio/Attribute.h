#pragma once

#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <houio/math/Math.h>
#include <houio/types.h>

namespace houio
{
    namespace detail
    {
        template<typename T>
        struct AttributeValueLayout
        {
            using Scalar = std::remove_cv_t<T>;
            static constexpr std::size_t component_count = 1;
        };

        template<typename T>
        struct AttributeValueLayout<math::Vec2<T>>
        {
            using Scalar = T;
            static constexpr std::size_t component_count = 2;
        };

        template<typename T>
        struct AttributeValueLayout<math::Vec3<T>>
        {
            using Scalar = T;
            static constexpr std::size_t component_count = 3;
        };

        template<typename T>
        struct AttributeValueLayout<math::Vec4<T>>
        {
            using Scalar = T;
            static constexpr std::size_t component_count = 4;
        };

        template<typename T>
        struct AttributeValueLayout<math::Matrix22<T>>
        {
            using Scalar = T;
            static constexpr std::size_t component_count = 4;
        };

        template<typename T>
        struct AttributeValueLayout<math::Matrix33<T>>
        {
            using Scalar = T;
            static constexpr std::size_t component_count = 9;
        };

        template<typename T>
        struct AttributeValueLayout<math::Matrix44<T>>
        {
            using Scalar = T;
            static constexpr std::size_t component_count = 16;
        };
    }

    class Attribute
    {
    public:
        using Ptr = std::shared_ptr<Attribute>;
        using CPtr = std::shared_ptr<const Attribute>;

        enum class ComponentType
        {
            invalid,
            int32,
            float32,
            int64,
            float16,
        };

        explicit Attribute(
            int component_count = 3,
            ComponentType component_type = ComponentType::float32);
        ~Attribute() = default;

        [[nodiscard]] Ptr copy() const;

        template<typename T>
        unsigned int appendElement(const T& value);
        template<typename T>
        unsigned int appendElement(const T& value0, const T& value1);
        template<typename T>
        unsigned int appendElement(const T& value0, const T& value1, const T& value2);
        template<typename T>
        unsigned int appendElement(
            const T& value0,
            const T& value1,
            const T& value2,
            const T& value3);

        template<typename T>
        [[nodiscard]] T get(unsigned int index) const;

        template<typename T>
        void set(unsigned int index, const T& value);
        template<typename T>
        void set(unsigned int index, const T& value0, const T& value1);
        template<typename T>
        void set(unsigned int index, const T& value0, const T& value1, const T& value2);
        template<typename T>
        void set(
            unsigned int index,
            const T& value0,
            const T& value1,
            const T& value2,
            const T& value3);

        void clear() noexcept;
        void resize(std::size_t element_count);
        void fillZero() noexcept;
        void append(const Attribute& source);
        unsigned int duplicateElement(unsigned int source_index);

        [[nodiscard]] int numElements() const;
        [[nodiscard]] int numComponents() const noexcept;
        [[nodiscard]] ComponentType elementComponentType() const noexcept;
        [[nodiscard]] int elementComponentSize() const noexcept;
        [[nodiscard]] std::size_t elementByteSize() const noexcept;
        [[nodiscard]] std::size_t byteSize() const noexcept;
        [[nodiscard]] bool isDirty() const noexcept;
        void markClean() noexcept;

        [[nodiscard]] std::span<std::byte> mutableBytes() noexcept;
        [[nodiscard]] std::span<const std::byte> bytes() const noexcept;
        [[nodiscard]] std::span<std::byte> mutableElementBytes(std::size_t index);
        [[nodiscard]] std::span<const std::byte> elementBytes(std::size_t index) const;

        [[nodiscard]] static Ptr create(
            int component_count,
            ComponentType component_type,
            std::span<const std::byte> raw_data,
            int element_count);
        [[nodiscard]] static Ptr createM33f();
        [[nodiscard]] static Ptr createM44f();
        [[nodiscard]] static Ptr createV4f(int element_count = 0);
        [[nodiscard]] static Ptr createV3f(int element_count = 0);
        [[nodiscard]] static Ptr createV2f(int element_count = 0);
        [[nodiscard]] static Ptr createFloat(int element_count = 0);
        [[nodiscard]] static Ptr createInt(int element_count = 0);

        [[nodiscard]] static int componentSize(ComponentType component_type);
        [[nodiscard]] static ComponentType componentType(const std::string& storage_name);

    private:
        template<typename T>
        static constexpr bool isByteCopyable = std::is_trivially_copyable_v<T>;

        template<typename T>
        [[nodiscard]] static constexpr ComponentType representationComponentType() noexcept;
        template<typename T>
        void validateValueRepresentation() const;
        template<typename T>
        void validateScalarRepresentation(std::size_t component_count) const;

        [[nodiscard]] std::size_t checkedElementOffset(std::size_t index) const;
        [[nodiscard]] unsigned int checkedNextElementIndex() const;
        void validateElementRepresentation(std::size_t representation_bytes) const;
        void appendElementBytes(std::span<const std::byte> source);
        void setElementBytes(std::size_t index, std::span<const std::byte> source);

        std::vector<std::byte> data_;
        int component_size_ = 0;
        ComponentType component_type_ = ComponentType::invalid;
        int component_count_ = 0;
        std::size_t element_count_ = 0;
        bool dirty_ = true;
    };

    template<typename T>
    constexpr Attribute::ComponentType Attribute::representationComponentType() noexcept
    {
        using Value = std::remove_cv_t<T>;
        using Scalar = std::remove_cv_t<typename detail::AttributeValueLayout<Value>::Scalar>;
        if constexpr (std::is_same_v<Scalar, sint32>)
            return ComponentType::int32;
        else if constexpr (std::is_same_v<Scalar, real32>)
            return ComponentType::float32;
        else if constexpr (std::is_same_v<Scalar, sint64>)
            return ComponentType::int64;
        else if constexpr (std::is_same_v<Scalar, uword>)
            return ComponentType::float16;
        else
            return ComponentType::invalid;
    }

    template<typename T>
    void Attribute::validateValueRepresentation() const
    {
        using Value = std::remove_cv_t<T>;
        using Layout = detail::AttributeValueLayout<Value>;
        using Scalar = std::remove_cv_t<typename Layout::Scalar>;
        static_assert(isByteCopyable<Value>, "Attribute values must be trivially copyable");
        static_assert(
            sizeof(Value) == sizeof(Scalar) * Layout::component_count,
            "Attribute composite values must not contain padding");

        constexpr ComponentType representation_type = representationComponentType<Value>();
        if constexpr (representation_type == ComponentType::invalid)
        {
            throw std::invalid_argument("Attribute value uses an unsupported scalar type");
        }
        else if (representation_type != component_type_
            || Layout::component_count != static_cast<std::size_t>(component_count_))
        {
            throw std::invalid_argument(
                "Attribute value type does not match the declared component layout");
        }
        validateElementRepresentation(sizeof(Value));
    }

    template<typename T>
    void Attribute::validateScalarRepresentation(std::size_t component_count) const
    {
        using Value = std::remove_cv_t<T>;
        using Layout = detail::AttributeValueLayout<Value>;
        static_assert(isByteCopyable<Value>, "Attribute values must be trivially copyable");
        static_assert(
            Layout::component_count == 1,
            "Multi-value Attribute access requires scalar arguments");

        constexpr ComponentType representation_type = representationComponentType<Value>();
        if constexpr (representation_type == ComponentType::invalid)
        {
            throw std::invalid_argument("Attribute value uses an unsupported scalar type");
        }
        else if (representation_type != component_type_
            || component_count != static_cast<std::size_t>(component_count_))
        {
            throw std::invalid_argument(
                "Attribute scalar values do not match the declared component layout");
        }
        validateElementRepresentation(sizeof(Value) * component_count);
    }

    template<typename T>
    unsigned int Attribute::appendElement(const T& value)
    {
        validateValueRepresentation<T>();
        const unsigned int index = checkedNextElementIndex();
        const auto source = std::as_bytes(std::span<const T>(&value, 1));
        appendElementBytes(source);
        return index;
    }

    template<typename T>
    unsigned int Attribute::appendElement(const T& value0, const T& value1)
    {
        validateScalarRepresentation<T>(2);
        const T values[] = {value0, value1};
        const unsigned int index = checkedNextElementIndex();
        appendElementBytes(std::as_bytes(std::span<const T>(values)));
        return index;
    }

    template<typename T>
    unsigned int Attribute::appendElement(const T& value0, const T& value1, const T& value2)
    {
        validateScalarRepresentation<T>(3);
        const T values[] = {value0, value1, value2};
        const unsigned int index = checkedNextElementIndex();
        appendElementBytes(std::as_bytes(std::span<const T>(values)));
        return index;
    }

    template<typename T>
    unsigned int Attribute::appendElement(
        const T& value0,
        const T& value1,
        const T& value2,
        const T& value3)
    {
        validateScalarRepresentation<T>(4);
        const T values[] = {value0, value1, value2, value3};
        const unsigned int index = checkedNextElementIndex();
        appendElementBytes(std::as_bytes(std::span<const T>(values)));
        return index;
    }

    template<typename T>
    T Attribute::get(unsigned int index) const
    {
        validateValueRepresentation<T>();
        T value{};
        const auto source = elementBytes(index);
        std::memcpy(&value, source.data(), sizeof(T));
        return value;
    }

    template<typename T>
    void Attribute::set(unsigned int index, const T& value)
    {
        validateValueRepresentation<T>();
        setElementBytes(index, std::as_bytes(std::span<const T>(&value, 1)));
    }

    template<typename T>
    void Attribute::set(unsigned int index, const T& value0, const T& value1)
    {
        validateScalarRepresentation<T>(2);
        const T values[] = {value0, value1};
        setElementBytes(index, std::as_bytes(std::span<const T>(values)));
    }

    template<typename T>
    void Attribute::set(unsigned int index, const T& value0, const T& value1, const T& value2)
    {
        validateScalarRepresentation<T>(3);
        const T values[] = {value0, value1, value2};
        setElementBytes(index, std::as_bytes(std::span<const T>(values)));
    }

    template<typename T>
    void Attribute::set(
        unsigned int index,
        const T& value0,
        const T& value1,
        const T& value2,
        const T& value3)
    {
        validateScalarRepresentation<T>(4);
        const T values[] = {value0, value1, value2, value3};
        setElementBytes(index, std::as_bytes(std::span<const T>(values)));
    }
}
