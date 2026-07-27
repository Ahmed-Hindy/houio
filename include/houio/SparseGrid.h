#pragma once

#include <houio/math/Matrix44.h>
#include <houio/math/Vec3.h>
#include <houio/types.h>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace houio
{
    enum class SparseGridClass
    {
        unknown,
        fog_volume,
        level_set,
        staggered,
    };

    enum class SparseVectorType
    {
        invariant,
        covariant,
        covariant_normalize,
        contravariant_relative,
        contravariant_absolute,
    };

    struct SparseIndexBounds
    {
        math::V3i minimum{0};
        math::V3i maximum{0};
    };

    template<typename Value>
    struct SparseVoxel
    {
        math::V3i index{0};
        Value value{};
    };

    template<typename Value>
    struct SparseTile
    {
        SparseIndexBounds bounds;
        Value value{};
    };

    using SparseFloatVoxel = SparseVoxel<float>;
    using SparseFloatTile = SparseTile<float>;
    using SparseInt32Voxel = SparseVoxel<sint32>;
    using SparseInt32Tile = SparseTile<sint32>;
    using SparseVec3fVoxel = SparseVoxel<math::V3f>;
    using SparseVec3fTile = SparseTile<math::V3f>;

    namespace detail
    {
    template<typename Value>
    class SparseValueGrid
    {
    public:
        using ValueType = Value;
        using Voxel = SparseVoxel<Value>;
        using Tile = SparseTile<Value>;

        explicit SparseValueGrid(Value background = Value{});

        [[nodiscard]] Value background() const noexcept;
        void setBackground(Value value);

        [[nodiscard]] SparseGridClass gridClass() const noexcept;
        void setGridClass(SparseGridClass value) noexcept;

        [[nodiscard]] const std::string& name() const noexcept;
        void setName(std::string value);

        [[nodiscard]] const math::M44f& indexToWorld() const noexcept;
        void setIndexToWorld(const math::M44f& value);

        void setMetadata(std::string key, std::string value);
        [[nodiscard]] std::optional<std::string> metadata(const std::string& key) const;
        [[nodiscard]] const std::map<std::string, std::string>& metadata() const noexcept;

        void setVoxel(const math::V3i& index, Value value);
        bool eraseVoxel(const math::V3i& index) noexcept;
        void addActiveTile(const SparseIndexBounds& bounds, Value value);
        void clearActiveTiles() noexcept;
        [[nodiscard]] bool isActive(const math::V3i& index) const noexcept;
        [[nodiscard]] Value value(const math::V3i& index) const noexcept;
        [[nodiscard]] std::size_t activeVoxelCount() const noexcept;
        [[nodiscard]] std::size_t activeTileCount() const noexcept;
        [[nodiscard]] std::optional<SparseIndexBounds> activeBounds() const noexcept;
        [[nodiscard]] std::vector<Voxel> activeVoxels() const;
        [[nodiscard]] const std::vector<Tile>& activeTiles() const noexcept;

        template<typename Callable>
        void forEachActiveVoxel(Callable&& callable) const
        {
            for( const auto& [index, storedValue] : voxels_ )
                callable(Voxel{index, storedValue});
        }

    private:
        struct IndexLess
        {
            [[nodiscard]] bool operator()(
                const math::V3i& left,
                const math::V3i& right) const noexcept;
        };

        static void validateValue(Value value, const char* field);

        Value background_{};
        SparseGridClass grid_class_ = SparseGridClass::unknown;
        std::string name_;
        math::M44f index_to_world_ = math::M44f::identity();
        std::map<std::string, std::string> metadata_;
        std::map<math::V3i, Value, IndexLess> voxels_;
        std::vector<Tile> tiles_;
    };
    }

    class SparseFloatGrid final : public detail::SparseValueGrid<float>
    {
    public:
        using detail::SparseValueGrid<float>::SparseValueGrid;
    };

    class SparseInt32Grid final : public detail::SparseValueGrid<sint32>
    {
    public:
        using detail::SparseValueGrid<sint32>::SparseValueGrid;
    };

    class SparseVec3fGrid final : public detail::SparseValueGrid<math::V3f>
    {
    public:
        using detail::SparseValueGrid<math::V3f>::SparseValueGrid;

        [[nodiscard]] SparseVectorType vectorType() const noexcept;
        void setVectorType(SparseVectorType value) noexcept;

    private:
        SparseVectorType vector_type_ = SparseVectorType::invariant;
    };
}
