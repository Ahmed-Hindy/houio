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

        // Compatibility names retained while callers migrate to scoped values.
        static constexpr ComponentType INVALID = ComponentType::invalid;
        static constexpr ComponentType INT = ComponentType::int32;
        static constexpr ComponentType FLOAT = ComponentType::float32;
        static constexpr ComponentType INT64 = ComponentType::int64;
        static constexpr ComponentType HALF = ComponentType::float16;

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

        [[nodiscard]] std::span<std::byte> bytes() noexcept;
        [[nodiscard]] std::span<const std::byte> bytes() const noexcept;
        [[nodiscard]] std::span<std::byte> elementBytes(std::size_t index);
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
    unsigned int Attribute::appendElement(const T& value)
    {
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        validateElementRepresentation(sizeof(T));
        const unsigned int index = checkedNextElementIndex();
        const auto source = std::as_bytes(std::span<const T>(&value, 1));
        appendElementBytes(source);
        return index;
    }

    template<typename T>
    unsigned int Attribute::appendElement(const T& value0, const T& value1)
    {
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        const T values[] = {value0, value1};
        validateElementRepresentation(sizeof(values));
        const unsigned int index = checkedNextElementIndex();
        appendElementBytes(std::as_bytes(std::span<const T>(values)));
        return index;
    }

    template<typename T>
    unsigned int Attribute::appendElement(const T& value0, const T& value1, const T& value2)
    {
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        const T values[] = {value0, value1, value2};
        validateElementRepresentation(sizeof(values));
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
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        const T values[] = {value0, value1, value2, value3};
        validateElementRepresentation(sizeof(values));
        const unsigned int index = checkedNextElementIndex();
        appendElementBytes(std::as_bytes(std::span<const T>(values)));
        return index;
    }

    template<typename T>
    T Attribute::get(unsigned int index) const
    {
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        validateElementRepresentation(sizeof(T));
        T value{};
        const auto source = elementBytes(index);
        std::memcpy(&value, source.data(), sizeof(T));
        return value;
    }

    template<typename T>
    void Attribute::set(unsigned int index, const T& value)
    {
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        validateElementRepresentation(sizeof(T));
        setElementBytes(index, std::as_bytes(std::span<const T>(&value, 1)));
    }

    template<typename T>
    void Attribute::set(unsigned int index, const T& value0, const T& value1)
    {
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        const T values[] = {value0, value1};
        validateElementRepresentation(sizeof(values));
        setElementBytes(index, std::as_bytes(std::span<const T>(values)));
    }

    template<typename T>
    void Attribute::set(unsigned int index, const T& value0, const T& value1, const T& value2)
    {
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        const T values[] = {value0, value1, value2};
        validateElementRepresentation(sizeof(values));
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
        static_assert(isByteCopyable<T>, "Attribute values must be trivially copyable");
        const T values[] = {value0, value1, value2, value3};
        validateElementRepresentation(sizeof(values));
        setElementBytes(index, std::as_bytes(std::span<const T>(values)));
    }
}
