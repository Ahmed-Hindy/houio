#pragma once

#include <houio/math/Matrix44.h>
#include <houio/math/Vec3.h>

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
    };

    struct SparseIndexBounds
    {
        math::V3i minimum{0};
        math::V3i maximum{0};
    };

    struct SparseFloatVoxel
    {
        math::V3i index{0};
        float value = 0.0f;
    };

    struct SparseFloatTile
    {
        SparseIndexBounds bounds;
        float value = 0.0f;
    };

    class SparseFloatGrid final
    {
    public:
        explicit SparseFloatGrid(float background = 0.0f);

        [[nodiscard]] float background() const noexcept;
        void setBackground(float value);

        [[nodiscard]] SparseGridClass gridClass() const noexcept;
        void setGridClass(SparseGridClass value) noexcept;

        [[nodiscard]] const std::string& name() const noexcept;
        void setName(std::string value);

        [[nodiscard]] const math::M44f& indexToWorld() const noexcept;
        void setIndexToWorld(const math::M44f& value);

        void setMetadata(std::string key, std::string value);
        [[nodiscard]] std::optional<std::string> metadata(const std::string& key) const;
        [[nodiscard]] const std::map<std::string, std::string>& metadata() const noexcept;

        void setVoxel(const math::V3i& index, float value);
        bool eraseVoxel(const math::V3i& index) noexcept;
        void addActiveTile(const SparseIndexBounds& bounds, float value);
        void clearActiveTiles() noexcept;
        [[nodiscard]] bool isActive(const math::V3i& index) const noexcept;
        [[nodiscard]] float value(const math::V3i& index) const noexcept;
        [[nodiscard]] std::size_t activeVoxelCount() const noexcept;
        [[nodiscard]] std::size_t activeTileCount() const noexcept;
        [[nodiscard]] std::optional<SparseIndexBounds> activeBounds() const noexcept;
        [[nodiscard]] std::vector<SparseFloatVoxel> activeVoxels() const;
        [[nodiscard]] const std::vector<SparseFloatTile>& activeTiles() const noexcept;

        template<typename Callable>
        void forEachActiveVoxel(Callable&& callable) const
        {
            for( const auto& [index, value] : voxels_ )
                callable(SparseFloatVoxel{index, value});
        }

    private:
        struct IndexLess
        {
            [[nodiscard]] bool operator()(const math::V3i& left, const math::V3i& right) const noexcept;
        };

        float background_ = 0.0f;
        SparseGridClass grid_class_ = SparseGridClass::unknown;
        std::string name_;
        math::M44f index_to_world_ = math::M44f::identity();
        std::map<std::string, std::string> metadata_;
        std::map<math::V3i, float, IndexLess> voxels_;
        std::vector<SparseFloatTile> tiles_;
    };
}
