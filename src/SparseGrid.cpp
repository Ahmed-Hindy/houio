#include <houio/SparseGrid.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace houio
{
namespace detail
{
    template<typename Value>
    SparseScalarGrid<Value>::SparseScalarGrid(Value background)
        : background_(background)
    {
        validateValue(background, "background");
    }

    template<typename Value>
    Value SparseScalarGrid<Value>::background() const noexcept
    {
        return background_;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::setBackground(Value value)
    {
        validateValue(value, "background");
        background_ = value;
    }

    template<typename Value>
    SparseGridClass SparseScalarGrid<Value>::gridClass() const noexcept
    {
        return grid_class_;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::setGridClass(SparseGridClass value) noexcept
    {
        grid_class_ = value;
    }

    template<typename Value>
    const std::string& SparseScalarGrid<Value>::name() const noexcept
    {
        return name_;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::setName(std::string value)
    {
        name_ = std::move(value);
    }

    template<typename Value>
    const math::M44f& SparseScalarGrid<Value>::indexToWorld() const noexcept
    {
        return index_to_world_;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::setIndexToWorld(const math::M44f& value)
    {
        if( std::any_of(value.ma.begin(), value.ma.end(),
                [](float component) { return !std::isfinite(component); }) )
        {
            throw std::invalid_argument("Sparse scalar grid transform must be finite");
        }
        index_to_world_ = value;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::setMetadata(std::string key, std::string value)
    {
        if( key.empty() )
            throw std::invalid_argument("Sparse scalar grid metadata key cannot be empty");
        metadata_.insert_or_assign(std::move(key), std::move(value));
    }

    template<typename Value>
    std::optional<std::string> SparseScalarGrid<Value>::metadata(
        const std::string& key) const
    {
        const auto entry = metadata_.find(key);
        if( entry == metadata_.end() )
            return std::nullopt;
        return entry->second;
    }

    template<typename Value>
    const std::map<std::string, std::string>&
    SparseScalarGrid<Value>::metadata() const noexcept
    {
        return metadata_;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::setVoxel(const math::V3i& index, Value value)
    {
        validateValue(value, "voxel value");
        voxels_.insert_or_assign(index, value);
    }

    template<typename Value>
    bool SparseScalarGrid<Value>::eraseVoxel(const math::V3i& index) noexcept
    {
        return voxels_.erase(index) != 0;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::addActiveTile(
        const SparseIndexBounds& bounds,
        Value value)
    {
        if( bounds.minimum.x > bounds.maximum.x
            || bounds.minimum.y > bounds.maximum.y
            || bounds.minimum.z > bounds.maximum.z )
        {
            throw std::invalid_argument("Sparse scalar grid tile bounds must be ordered");
        }
        validateValue(value, "tile value");
        tiles_.push_back(Tile{bounds, value});
    }

    template<typename Value>
    void SparseScalarGrid<Value>::clearActiveTiles() noexcept
    {
        tiles_.clear();
    }

    template<typename Value>
    bool SparseScalarGrid<Value>::isActive(const math::V3i& index) const noexcept
    {
        if( voxels_.contains(index) )
            return true;
        return std::any_of(tiles_.begin(), tiles_.end(),
            [&](const Tile& tile)
            {
                return index.x >= tile.bounds.minimum.x && index.x <= tile.bounds.maximum.x
                    && index.y >= tile.bounds.minimum.y && index.y <= tile.bounds.maximum.y
                    && index.z >= tile.bounds.minimum.z && index.z <= tile.bounds.maximum.z;
            });
    }

    template<typename Value>
    Value SparseScalarGrid<Value>::value(const math::V3i& index) const noexcept
    {
        const auto entry = voxels_.find(index);
        if( entry != voxels_.end() )
            return entry->second;
        for( auto tile = tiles_.rbegin(); tile != tiles_.rend(); ++tile )
        {
            if( index.x >= tile->bounds.minimum.x && index.x <= tile->bounds.maximum.x
                && index.y >= tile->bounds.minimum.y && index.y <= tile->bounds.maximum.y
                && index.z >= tile->bounds.minimum.z && index.z <= tile->bounds.maximum.z )
            {
                return tile->value;
            }
        }
        return background_;
    }

    template<typename Value>
    std::size_t SparseScalarGrid<Value>::activeVoxelCount() const noexcept
    {
        return voxels_.size();
    }

    template<typename Value>
    std::size_t SparseScalarGrid<Value>::activeTileCount() const noexcept
    {
        return tiles_.size();
    }

    template<typename Value>
    std::optional<SparseIndexBounds>
    SparseScalarGrid<Value>::activeBounds() const noexcept
    {
        if( voxels_.empty() && tiles_.empty() )
            return std::nullopt;

        SparseIndexBounds bounds;
        if( !voxels_.empty() )
        {
            const auto first = voxels_.begin();
            bounds = SparseIndexBounds{first->first, first->first};
        }
        else
        {
            bounds = tiles_.front().bounds;
        }

        const auto include = [&](const math::V3i& index)
        {
            bounds.minimum.x = std::min(bounds.minimum.x, index.x);
            bounds.minimum.y = std::min(bounds.minimum.y, index.y);
            bounds.minimum.z = std::min(bounds.minimum.z, index.z);
            bounds.maximum.x = std::max(bounds.maximum.x, index.x);
            bounds.maximum.y = std::max(bounds.maximum.y, index.y);
            bounds.maximum.z = std::max(bounds.maximum.z, index.z);
        };
        for( const auto& [index, storedValue] : voxels_ )
        {
            static_cast<void>(storedValue);
            include(index);
        }
        for( const Tile& tile : tiles_ )
        {
            include(tile.bounds.minimum);
            include(tile.bounds.maximum);
        }
        return bounds;
    }

    template<typename Value>
    std::vector<typename SparseScalarGrid<Value>::Voxel>
    SparseScalarGrid<Value>::activeVoxels() const
    {
        std::vector<Voxel> result;
        result.reserve(voxels_.size());
        for( const auto& [index, storedValue] : voxels_ )
            result.push_back({index, storedValue});
        return result;
    }

    template<typename Value>
    const std::vector<typename SparseScalarGrid<Value>::Tile>&
    SparseScalarGrid<Value>::activeTiles() const noexcept
    {
        return tiles_;
    }

    template<typename Value>
    bool SparseScalarGrid<Value>::IndexLess::operator()(
        const math::V3i& left,
        const math::V3i& right) const noexcept
    {
        if( left.z != right.z )
            return left.z < right.z;
        if( left.y != right.y )
            return left.y < right.y;
        return left.x < right.x;
    }

    template<typename Value>
    void SparseScalarGrid<Value>::validateValue(Value value, const char* field)
    {
        if constexpr( std::is_floating_point_v<Value> )
        {
            if( !std::isfinite(value) )
                throw std::invalid_argument(std::string("Sparse scalar grid ") + field + " must be finite");
        }
        else
        {
            static_cast<void>(value);
            static_cast<void>(field);
        }
    }

    template class SparseScalarGrid<float>;
    template class SparseScalarGrid<sint32>;
}
}
