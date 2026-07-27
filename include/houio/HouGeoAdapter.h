#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <houio/SparseGrid.h>
#include <houio/math/Math.h>
#include <houio/types.h>

namespace houio
{
    namespace json
    {
        struct Array;
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

            class TupleSize final
            {
            public:
                explicit TupleSize(int value)
                    : value_(value)
                {
                    if (value <= 0)
                        throw std::invalid_argument("Attribute tuple size must be positive");
                }

                [[nodiscard]] int value() const noexcept { return value_; }
                [[nodiscard]] std::size_t asSize() const noexcept
                {
                    return static_cast<std::size_t>(value_);
                }

                friend bool operator==(TupleSize, TupleSize) = default;

            private:
                int value_;
            };

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

            virtual ~AttributeAdapter() = default;

            [[nodiscard]] virtual std::string name() const;
            [[nodiscard]] virtual std::string scope() const;
            [[nodiscard]] virtual std::shared_ptr<json::Object> options() const;
            [[nodiscard]] virtual Type type() const;
            [[nodiscard]] virtual TupleSize tupleSize() const = 0;
            [[nodiscard]] virtual Storage storage() const;
            [[nodiscard]] virtual std::vector<int> packing() const;
            [[nodiscard]] virtual int elementCount() const;
            [[nodiscard]] virtual std::string stringValue(int index) const = 0;
            [[nodiscard]] virtual std::string stringValue(int element_index, int component_index) const;
            [[nodiscard]] virtual std::shared_ptr<json::Object> dictionaryValue(int index) const;
            [[nodiscard]] virtual RawDataView rawData() const;

            [[nodiscard]] static Type parseType(std::string_view type_name) noexcept;
            [[nodiscard]] static std::optional<std::string_view> typeName(Type type) noexcept;
            [[nodiscard]] static Storage parseStorage(std::string_view storage_name) noexcept;
            [[nodiscard]] static std::optional<std::string_view> storageName(Storage storage) noexcept;
            [[nodiscard]] static std::optional<std::size_t> storageByteWidth(Storage storage) noexcept;
        };

        class Topology
        {
        public:
            using Ptr = std::shared_ptr<Topology>;
            using ConstPtr = std::shared_ptr<const Topology>;

            virtual ~Topology() = default;

            [[nodiscard]] virtual std::vector<int> indexValues() const = 0;
            /// Optional immutable view used to avoid copying topology during export.
            /// The default empty view preserves compatibility with existing adapters.
            [[nodiscard]] virtual std::span<const int> indexView() const noexcept;
            virtual void appendIndices(std::span<const int> indices) = 0;
            [[nodiscard]] virtual sint64 indexCount() const = 0;
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
                packed_geometry,
                packed_fragment,
                packed_disk,
                packed_disk_sequence,
                native_vdb,
            };

            virtual ~Primitive() = default;
            [[nodiscard]] virtual int primitiveCount() const { return 1; }
        };

        class VolumePrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<VolumePrimitive>;
            using ConstPtr = std::shared_ptr<const VolumePrimitive>;

            [[nodiscard]] virtual math::M44f transform() const = 0;
            [[nodiscard]] virtual int topologyVertex() const = 0;
            [[nodiscard]] virtual math::Vec3i resolution() const;
            [[nodiscard]] virtual real32 voxelValue(int x, int y, int z) const = 0;
            [[nodiscard]] virtual std::string visualizationMode() const;
            [[nodiscard]] virtual real32 visualizationIso() const;
            [[nodiscard]] virtual real32 visualizationDensity() const;
            [[nodiscard]] virtual RawDataView rawData() const;
        };

        class PackedGeometryPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<PackedGeometryPrimitive>;
            using ConstPtr = std::shared_ptr<const PackedGeometryPrimitive>;

            [[nodiscard]] virtual HouGeoAdapter::ConstPtr embeddedGeometry() const = 0;
            [[nodiscard]] virtual int topologyVertex() const = 0;
            [[nodiscard]] virtual math::V3f pivot() const;
            [[nodiscard]] virtual math::M33f transform() const;
            [[nodiscard]] virtual std::string viewportLod() const;
            [[nodiscard]] virtual bool pointInstanceTransform() const;
            [[nodiscard]] virtual bool treatAsFolder() const;
        };

        class PackedFragmentPrimitive : public PackedGeometryPrimitive
        {
        public:
            using Ptr = std::shared_ptr<PackedFragmentPrimitive>;
            using ConstPtr = std::shared_ptr<const PackedFragmentPrimitive>;
            using Bounds = std::array<real32, 6>;

            [[nodiscard]] virtual std::string fragmentAttribute() const = 0;
            [[nodiscard]] virtual std::string fragmentName() const = 0;
            [[nodiscard]] virtual Bounds bounds() const;
            [[nodiscard]] virtual Bounds cachedBounds() const;
        };

        class PackedDiskPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<PackedDiskPrimitive>;
            using ConstPtr = std::shared_ptr<const PackedDiskPrimitive>;

            [[nodiscard]] virtual int topologyVertex() const = 0;
            [[nodiscard]] virtual std::string filename() const = 0;
            [[nodiscard]] virtual real32 expandFrame() const;
            [[nodiscard]] virtual bool expandFilename() const;
            [[nodiscard]] virtual math::V3f pivot() const;
            [[nodiscard]] virtual math::M33f transform() const;
            [[nodiscard]] virtual std::string viewportLod() const;
            [[nodiscard]] virtual bool pointInstanceTransform() const;
            [[nodiscard]] virtual bool treatAsFolder() const;
        };

        class PackedDiskSequencePrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<PackedDiskSequencePrimitive>;
            using ConstPtr = std::shared_ptr<const PackedDiskSequencePrimitive>;

            enum class WrapMode
            {
                cycle,
                clamp,
                strict,
                mirror,
            };

            [[nodiscard]] virtual int topologyVertex() const = 0;
            [[nodiscard]] virtual std::vector<std::string> filenames() const = 0;
            [[nodiscard]] virtual real32 index() const;
            [[nodiscard]] virtual WrapMode wrapMode() const;
            [[nodiscard]] virtual math::V3f pivot() const;
            [[nodiscard]] virtual math::M33f transform() const;
            [[nodiscard]] virtual std::string viewportLod() const;
            [[nodiscard]] virtual bool pointInstanceTransform() const;
        };

        class SparseVdbPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<SparseVdbPrimitive>;
            using ConstPtr = std::shared_ptr<const SparseVdbPrimitive>;

            [[nodiscard]] virtual int topologyVertex() const = 0;
            [[nodiscard]] virtual const SparseFloatGrid& sparseGrid() const = 0;
        };

        class SparseInt32VdbPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<SparseInt32VdbPrimitive>;
            using ConstPtr = std::shared_ptr<const SparseInt32VdbPrimitive>;

            [[nodiscard]] virtual int topologyVertex() const = 0;
            [[nodiscard]] virtual const SparseInt32Grid& sparseGrid() const = 0;
        };

        class SparseVec3fVdbPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<SparseVec3fVdbPrimitive>;
            using ConstPtr = std::shared_ptr<const SparseVec3fVdbPrimitive>;

            [[nodiscard]] virtual int topologyVertex() const = 0;
            [[nodiscard]] virtual const SparseVec3fGrid& sparseGrid() const = 0;
        };

        class NativeVdbPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<NativeVdbPrimitive>;
            using ConstPtr = std::shared_ptr<const NativeVdbPrimitive>;

            [[nodiscard]] virtual int topologyVertex() const = 0;
            /// Opaque Houdini VDB payload retained for lossless file round trips.
            /// Constructing or editing sparse trees requires an optional OpenVDB backend.
            [[nodiscard]] virtual std::shared_ptr<json::Array> serializedPayload() const = 0;
        };

        class PolyPrimitive : public Primitive
        {
        public:
            using Ptr = std::shared_ptr<PolyPrimitive>;
            using ConstPtr = std::shared_ptr<const PolyPrimitive>;

            [[nodiscard]] virtual int polygonCount() const;
            [[nodiscard]] virtual int polygonVertexCount(int polygon_index) const;
            [[nodiscard]] virtual std::span<const int> polygonVertexIndices(
                int polygon_index = 0) const;
            [[nodiscard]] virtual bool isClosed() const;
            [[nodiscard]] int primitiveCount() const override { return polygonCount(); }
        };

        virtual ~HouGeoAdapter() = default;

        [[nodiscard]] virtual sint64 pointCount() const;
        [[nodiscard]] virtual sint64 vertexCount() const;
        [[nodiscard]] virtual sint64 primitiveCount() const;
        [[nodiscard]] virtual std::vector<std::string> pointAttributeNames() const;
        [[nodiscard]] virtual AttributeAdapter::Ptr pointAttribute(const std::string& name);
        [[nodiscard]] virtual AttributeAdapter::ConstPtr pointAttribute(
            const std::string& name) const;
        [[nodiscard]] virtual std::vector<std::string> vertexAttributeNames() const;
        [[nodiscard]] virtual AttributeAdapter::Ptr vertexAttribute(const std::string& name);
        [[nodiscard]] virtual AttributeAdapter::ConstPtr vertexAttribute(
            const std::string& name) const;
        [[nodiscard]] virtual std::vector<std::string> globalAttributeNames() const;
        [[nodiscard]] virtual AttributeAdapter::Ptr globalAttribute(const std::string& name);
        [[nodiscard]] virtual AttributeAdapter::ConstPtr globalAttribute(
            const std::string& name) const;
        [[nodiscard]] virtual std::vector<std::string> primitiveAttributeNames() const = 0;
        [[nodiscard]] virtual AttributeAdapter::Ptr primitiveAttribute(const std::string& name) = 0;
        [[nodiscard]] virtual AttributeAdapter::ConstPtr primitiveAttribute(
            const std::string& name) const = 0;
        [[nodiscard]] virtual std::vector<std::string> pointGroupNames() const;
        [[nodiscard]] virtual std::optional<std::vector<bool>> pointGroupMembership(
            const std::string& name) const;
        [[nodiscard]] virtual std::vector<std::string> vertexGroupNames() const;
        [[nodiscard]] virtual std::optional<std::vector<bool>> vertexGroupMembership(
            const std::string& name) const;
        [[nodiscard]] virtual std::vector<std::string> primitiveGroupNames() const;
        [[nodiscard]] virtual std::optional<std::vector<bool>> primitiveGroupMembership(
            const std::string& name) const;
        [[nodiscard]] virtual bool hasPrimitiveAttribute(const std::string& name) const;
        [[nodiscard]] virtual std::vector<Primitive::Ptr> primitives();
        [[nodiscard]] virtual std::vector<Primitive::ConstPtr> primitives() const;
        /// Optional immutable view used to avoid copying the primitive pointer list during export.
        /// The default empty view preserves compatibility with existing adapters.
        [[nodiscard]] virtual std::span<const Primitive::Ptr> primitiveView() const noexcept;
        [[nodiscard]] virtual Topology::Ptr topology();
        [[nodiscard]] virtual Topology::ConstPtr topology() const;
    };
}
