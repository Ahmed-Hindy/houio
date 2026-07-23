#pragma once

#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <houio/math/Math.h>
#include <houio/types.h>

namespace houio
{
    class HouGeoIO;

    namespace json
    {
        struct Object;
    }

    class HouGeoAdapter
    {
    public:
        using Ptr = std::shared_ptr<HouGeoAdapter>;
        using ConstPtr = std::shared_ptr<const HouGeoAdapter>;

        class RawDataView final
        {
        public:
            RawDataView() = default;

            explicit RawDataView(std::span<const std::byte> bytes) noexcept
                : bytes_(bytes), available_(true)
            {
            }

            template<typename T>
            [[nodiscard]] static RawDataView from(std::span<const T> values) noexcept
            {
                static_assert(std::is_trivially_copyable_v<T>);
                return RawDataView(std::as_bytes(values));
            }

            [[nodiscard]] bool available() const noexcept { return available_; }
            [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }
            [[nodiscard]] std::size_t sizeBytes() const noexcept { return bytes_.size(); }
            [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return bytes_; }

            template<typename T>
            [[nodiscard]] T read(std::size_t scalar_index) const
            {
                static_assert(std::is_trivially_copyable_v<T>);
                if (!available_)
                    throw std::logic_error("RawDataView is unavailable");
                if (scalar_index > std::numeric_limits<std::size_t>::max() / sizeof(T))
                    throw std::out_of_range("RawDataView scalar index overflow");
                const std::size_t byte_offset = scalar_index * sizeof(T);
                if (byte_offset > bytes_.size() || sizeof(T) > bytes_.size() - byte_offset)
                    throw std::out_of_range("RawDataView scalar index is outside the byte range");
                T value{};
                std::memcpy(&value, bytes_.data() + byte_offset, sizeof(T));
                return value;
            }

        private:
            std::span<const std::byte> bytes_;
            bool available_ = false;
        };

        class AttributeAdapter
        {
        public:
            using Ptr = std::shared_ptr<AttributeAdapter>;
            using ConstPtr = std::shared_ptr<const AttributeAdapter>;

            enum class Type
            {
                invalid,
                numeric,
                string,
                dictionary,
            };

            enum class Storage
            {
                invalid,
                float32,
                float64,
                int32,
                int64,
                float16,
            };

            static constexpr Type ATTR_TYPE_INVALID = Type::invalid;
            static constexpr Type ATTR_TYPE_NUMERIC = Type::numeric;
            static constexpr Type ATTR_TYPE_STRING = Type::string;
            static constexpr Type ATTR_TYPE_DICT = Type::dictionary;
            static constexpr Storage ATTR_STORAGE_INVALID = Storage::invalid;
            static constexpr Storage ATTR_STORAGE_FPREAL32 = Storage::float32;
            static constexpr Storage ATTR_STORAGE_FPREAL64 = Storage::float64;
            static constexpr Storage ATTR_STORAGE_INT32 = Storage::int32;
            static constexpr Storage ATTR_STORAGE_INT64 = Storage::int64;
            static constexpr Storage ATTR_STORAGE_FPREAL16 = Storage::float16;

            virtual ~AttributeAdapter();

            [[nodiscard]] std::string name() const;
            [[nodiscard]] Type type() const;
            [[nodiscard]] int tupleSize() const;
            [[nodiscard]] Storage storage() const;
            [[nodiscard]] std::vector<int> packing() const;
            [[nodiscard]] int elementCount() const;
            [[nodiscard]] std::string stringValue(int index) const;
            [[nodiscard]] std::shared_ptr<json::Object> dictionaryValue(int index) const;
            [[nodiscard]] virtual RawDataView rawData() const;

            [[nodiscard]] static Type parseType(const std::string& type_name);
            [[nodiscard]] static Storage parseStorage(const std::string& storage_name);
            [[nodiscard]] static int storageSize(Storage storage_type);

        protected:
            friend class ::houio::HouGeoIO;

            [[nodiscard]] virtual std::string getName() const;
            [[nodiscard]] virtual Type getType() const;
            [[nodiscard]] virtual int getTupleSize() const;
            [[nodiscard]] virtual Storage getStorage() const;
            virtual void getPacking(std::vector<int>& packing) const;
            [[nodiscard]] virtual int getNumElements() const;
            [[nodiscard]] virtual std::string getString(int index) const = 0;
            [[nodiscard]] virtual std::shared_ptr<json::Object> getDictionary(int index) const;
        };

        class Topology
        {
        public:
            using Ptr = std::shared_ptr<Topology>;
            using ConstPtr = std::shared_ptr<const Topology>;

            virtual ~Topology();
            virtual void getIndices(std::vector<int>& indices) const = 0;
            virtual void addIndices(const std::vector<int>& indices) = 0;
            [[nodiscard]] virtual sint64 getNumIndices() const = 0;
        };

        class Primitive
        {
        public:
            using Ptr = std::shared_ptr<Primitive>;
            using ConstPtr = std::shared_ptr<const Primitive>;

            enum class Type
            {
                volume,
                polygon,
            };

            static constexpr Type PRIM_VOLUME = Type::volume;
            static constexpr Type PRIM_POLY = Type::polygon;

            virtual ~Primitive() = default;
            [[nodiscard]] virtual int numPrimitives() const { return 1; }
        };

        class VolumePrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<VolumePrimitive>;

            [[nodiscard]] virtual math::M44f getTransform() const = 0;
            [[nodiscard]] virtual int getVertex() const = 0;
            [[nodiscard]] virtual math::Vec3i getResolution() const;
            [[nodiscard]] virtual real32 getVoxel(int x, int y, int z) const = 0;
            [[nodiscard]] virtual std::string getVisualizationMode() const;
            [[nodiscard]] virtual real32 getVisualizationIso() const;
            [[nodiscard]] virtual real32 getVisualizationDensity() const;
            [[nodiscard]] virtual RawDataView rawData() const;
        };

        class PolyPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<PolyPrimitive>;

            [[nodiscard]] virtual int numPolys() const;
            [[nodiscard]] virtual int numVertices(int polygon_index) const;
            [[nodiscard]] virtual const int* vertices(int polygon_index = 0) const;
            [[nodiscard]] int numPrimitives() const override { return numPolys(); }
            [[nodiscard]] virtual bool closed() const;
        };

        virtual ~HouGeoAdapter() = default;

        [[nodiscard]] sint64 pointCount() const;
        [[nodiscard]] sint64 vertexCount() const;
        [[nodiscard]] sint64 primitiveCount() const;
        [[nodiscard]] std::vector<std::string> pointAttributeNames() const;
        [[nodiscard]] AttributeAdapter::Ptr pointAttribute(const std::string& name);
        [[nodiscard]] std::vector<std::string> vertexAttributeNames() const;
        [[nodiscard]] AttributeAdapter::Ptr vertexAttribute(const std::string& name);
        [[nodiscard]] std::vector<std::string> globalAttributeNames() const;
        [[nodiscard]] AttributeAdapter::Ptr globalAttribute(const std::string& name);
        [[nodiscard]] std::vector<std::string> primitiveAttributeNames() const;
        [[nodiscard]] AttributeAdapter::Ptr primitiveAttribute(const std::string& name);
        [[nodiscard]] std::vector<std::string> pointGroupNames() const;
        [[nodiscard]] std::optional<std::vector<bool>> pointGroupMembership(
            const std::string& name) const;
        [[nodiscard]] std::vector<std::string> vertexGroupNames() const;
        [[nodiscard]] std::optional<std::vector<bool>> vertexGroupMembership(
            const std::string& name) const;
        [[nodiscard]] std::vector<std::string> primitiveGroupNames() const;
        [[nodiscard]] std::optional<std::vector<bool>> primitiveGroupMembership(
            const std::string& name) const;
        [[nodiscard]] bool hasPrimitiveAttribute(const std::string& name) const;
        [[nodiscard]] std::vector<Primitive::Ptr> primitives();
        [[nodiscard]] Topology::Ptr topology();

    protected:
        [[nodiscard]] virtual sint64 pointcount() const;
        [[nodiscard]] virtual sint64 vertexcount() const;
        [[nodiscard]] virtual sint64 primitivecount() const;
        virtual void getPointAttributeNames(std::vector<std::string>& names) const;
        [[nodiscard]] virtual AttributeAdapter::Ptr getPointAttribute(const std::string& name);
        virtual void getVertexAttributeNames(std::vector<std::string>& names) const;
        [[nodiscard]] virtual AttributeAdapter::Ptr getVertexAttribute(const std::string& name);
        virtual void getGlobalAttributeNames(std::vector<std::string>& names) const;
        [[nodiscard]] virtual AttributeAdapter::Ptr getGlobalAttribute(const std::string& name);
        virtual void getPointGroupNames(std::vector<std::string>& names) const;
        [[nodiscard]] virtual bool getPointGroupMembership(
            const std::string& name,
            std::vector<bool>& membership) const;
        virtual void getVertexGroupNames(std::vector<std::string>& names) const;
        [[nodiscard]] virtual bool getVertexGroupMembership(
            const std::string& name,
            std::vector<bool>& membership) const;
        virtual void getPrimitiveGroupNames(std::vector<std::string>& names) const;
        [[nodiscard]] virtual bool getPrimitiveGroupMembership(
            const std::string& name,
            std::vector<bool>& membership) const;
        [[nodiscard]] virtual bool hasPrimitiveAttributeLegacy(const std::string& name) const;
        virtual void getPrimitiveAttributeNames(std::vector<std::string>& names) const = 0;
        [[nodiscard]] virtual AttributeAdapter::Ptr getPrimitiveAttribute(
            const std::string& name) = 0;
        virtual void getPrimitives(std::vector<Primitive::Ptr>& primitives);
        [[nodiscard]] virtual Topology::Ptr getTopology();
    };
}
