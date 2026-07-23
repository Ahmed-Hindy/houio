#include <houio/Attribute.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace houio
{
    Attribute::Attribute(int component_count, ComponentType component_type)
        : component_size_(componentSize(component_type)),
          component_type_(component_type),
          component_count_(component_count)
    {
        if (component_count <= 0)
            throw std::invalid_argument("Attribute requires at least one component");
    }

    Attribute::Ptr Attribute::copy() const
    {
        return create(
            component_count_,
            component_type_,
            data_.empty() ? nullptr : data_.data(),
            numElements());
    }

    void Attribute::clear() noexcept
    {
        data_.clear();
        element_count_ = 0;
        dirty_ = true;
    }

    void Attribute::resize(std::size_t element_count)
    {
        const std::size_t bytes_per_element = elementByteSize();
        if (bytes_per_element != 0
            && element_count > std::numeric_limits<std::size_t>::max() / bytes_per_element)
        {
            throw std::length_error("Attribute byte count exceeds addressable storage");
        }
        data_.resize(element_count * bytes_per_element);
        element_count_ = element_count;
        dirty_ = true;
    }

    void Attribute::fillZero() noexcept
    {
        std::fill(data_.begin(), data_.end(), std::byte{0});
        dirty_ = true;
    }

    void Attribute::append(const Attribute& source)
    {
        if (component_type_ != source.component_type_
            || component_count_ != source.component_count_
            || component_size_ != source.component_size_)
        {
            throw std::invalid_argument("Cannot append attributes with different layouts");
        }
        if (source.element_count_ > std::numeric_limits<std::size_t>::max() - element_count_)
            throw std::length_error("Attribute element count overflow");
        if (source.data_.size() > std::numeric_limits<std::size_t>::max() - data_.size())
            throw std::length_error("Attribute byte count overflow");

        data_.insert(data_.end(), source.data_.begin(), source.data_.end());
        element_count_ += source.element_count_;
        dirty_ = true;
    }

    unsigned int Attribute::duplicateElement(unsigned int source_index)
    {
        const auto source = elementBytes(source_index);
        std::vector<std::byte> copied(source.begin(), source.end());
        const unsigned int destination_index = checkedNextElementIndex();
        appendElementBytes(copied);
        return destination_index;
    }

    int Attribute::numElements() const
    {
        if (element_count_ > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::overflow_error("Attribute element count exceeds int range");
        return static_cast<int>(element_count_);
    }

    int Attribute::numComponents() const noexcept
    {
        return component_count_;
    }

    Attribute::ComponentType Attribute::elementComponentType() const noexcept
    {
        return component_type_;
    }

    int Attribute::elementComponentSize() const noexcept
    {
        return component_size_;
    }

    std::size_t Attribute::elementByteSize() const noexcept
    {
        return static_cast<std::size_t>(component_count_)
            * static_cast<std::size_t>(component_size_);
    }

    std::size_t Attribute::byteSize() const noexcept
    {
        return data_.size();
    }

    bool Attribute::isDirty() const noexcept
    {
        return dirty_;
    }

    void Attribute::markClean() noexcept
    {
        dirty_ = false;
    }

    std::span<std::byte> Attribute::bytes() noexcept
    {
        return data_;
    }

    std::span<const std::byte> Attribute::bytes() const noexcept
    {
        return data_;
    }

    std::span<std::byte> Attribute::elementBytes(std::size_t index)
    {
        const std::size_t offset = checkedElementOffset(index);
        return std::span<std::byte>(data_).subspan(offset, elementByteSize());
    }

    std::span<const std::byte> Attribute::elementBytes(std::size_t index) const
    {
        const std::size_t offset = checkedElementOffset(index);
        return std::span<const std::byte>(data_).subspan(offset, elementByteSize());
    }

    Attribute::Ptr Attribute::create(
        int component_count,
        ComponentType component_type,
        const void* raw_data,
        int element_count)
    {
        if (element_count < 0)
            throw std::invalid_argument("Attribute element count cannot be negative");

        auto attribute = std::make_shared<Attribute>(component_count, component_type);
        attribute->resize(static_cast<std::size_t>(element_count));
        if (!attribute->data_.empty())
        {
            if (!raw_data)
                throw std::invalid_argument(
                    "Attribute raw data cannot be null for non-empty storage");
            std::memcpy(attribute->data_.data(), raw_data, attribute->data_.size());
        }
        return attribute;
    }

    Attribute::Ptr Attribute::createM33f()
    {
        return std::make_shared<Attribute>(9, ComponentType::float32);
    }

    Attribute::Ptr Attribute::createM44f()
    {
        return std::make_shared<Attribute>(16, ComponentType::float32);
    }

    Attribute::Ptr Attribute::createV4f(int element_count)
    {
        if (element_count < 0)
            throw std::invalid_argument("Attribute element count cannot be negative");
        auto attribute = std::make_shared<Attribute>(4, ComponentType::float32);
        attribute->resize(static_cast<std::size_t>(element_count));
        return attribute;
    }

    Attribute::Ptr Attribute::createV3f(int element_count)
    {
        if (element_count < 0)
            throw std::invalid_argument("Attribute element count cannot be negative");
        auto attribute = std::make_shared<Attribute>(3, ComponentType::float32);
        attribute->resize(static_cast<std::size_t>(element_count));
        return attribute;
    }

    Attribute::Ptr Attribute::createV2f(int element_count)
    {
        if (element_count < 0)
            throw std::invalid_argument("Attribute element count cannot be negative");
        auto attribute = std::make_shared<Attribute>(2, ComponentType::float32);
        attribute->resize(static_cast<std::size_t>(element_count));
        return attribute;
    }

    Attribute::Ptr Attribute::createFloat(int element_count)
    {
        if (element_count < 0)
            throw std::invalid_argument("Attribute element count cannot be negative");
        auto attribute = std::make_shared<Attribute>(1, ComponentType::float32);
        attribute->resize(static_cast<std::size_t>(element_count));
        return attribute;
    }

    Attribute::Ptr Attribute::createInt(int element_count)
    {
        if (element_count < 0)
            throw std::invalid_argument("Attribute element count cannot be negative");
        auto attribute = std::make_shared<Attribute>(1, ComponentType::int32);
        attribute->resize(static_cast<std::size_t>(element_count));
        return attribute;
    }

    int Attribute::componentSize(ComponentType component_type)
    {
        switch (component_type)
        {
        case ComponentType::int32:
            return static_cast<int>(sizeof(sint32));
        case ComponentType::float32:
            return static_cast<int>(sizeof(real32));
        case ComponentType::int64:
            return static_cast<int>(sizeof(sint64));
        case ComponentType::float16:
            return static_cast<int>(sizeof(uword));
        case ComponentType::invalid:
            throw std::invalid_argument("Attribute component type is invalid");
        }
        throw std::invalid_argument("Attribute component type is unknown");
    }

    Attribute::ComponentType Attribute::componentType(const std::string& storage_name)
    {
        if (storage_name == "fpreal32" || storage_name == "float")
            return ComponentType::float32;
        if (storage_name == "int32" || storage_name == "int")
            return ComponentType::int32;
        if (storage_name == "int64")
            return ComponentType::int64;
        if (storage_name == "fpreal16")
            return ComponentType::float16;
        return ComponentType::invalid;
    }

    std::size_t Attribute::checkedElementOffset(std::size_t index) const
    {
        if (index >= element_count_)
            throw std::out_of_range("Attribute index exceeds stored data");
        return index * elementByteSize();
    }

    unsigned int Attribute::checkedNextElementIndex() const
    {
        if (element_count_ > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max()))
            throw std::overflow_error("Attribute element index exceeds unsigned int range");
        return static_cast<unsigned int>(element_count_);
    }

    void Attribute::validateElementRepresentation(std::size_t representation_bytes) const
    {
        if (representation_bytes != elementByteSize())
        {
            throw std::invalid_argument(
                "Attribute value size does not match the declared tuple layout");
        }
    }

    void Attribute::appendElementBytes(std::span<const std::byte> source)
    {
        validateElementRepresentation(source.size());
        if (data_.size() > std::numeric_limits<std::size_t>::max() - source.size())
            throw std::length_error("Attribute byte count overflow");
        if (element_count_ == std::numeric_limits<std::size_t>::max())
            throw std::length_error("Attribute element count overflow");

        data_.insert(data_.end(), source.begin(), source.end());
        ++element_count_;
        dirty_ = true;
    }

    void Attribute::setElementBytes(std::size_t index, std::span<const std::byte> source)
    {
        validateElementRepresentation(source.size());
        auto destination = elementBytes(index);
        std::memcpy(destination.data(), source.data(), source.size());
        dirty_ = true;
    }
}
