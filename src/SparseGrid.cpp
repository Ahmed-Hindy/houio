#include <houio/SparseGrid.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace houio
{
    SparseFloatGrid::SparseFloatGrid(float background)
        : background_(background)
    {
        if( !std::isfinite(background) )
            throw std::invalid_argument("SparseFloatGrid background must be finite");
    }

    float SparseFloatGrid::background() const noexcept
    {
        return background_;
    }

    void SparseFloatGrid::setBackground(float value)
    {
        if( !std::isfinite(value) )
            throw std::invalid_argument("SparseFloatGrid background must be finite");
        background_ = value;
    }

    SparseGridClass SparseFloatGrid::gridClass() const noexcept
    {
        return grid_class_;
    }

    void SparseFloatGrid::setGridClass(SparseGridClass value) noexcept
    {
        grid_class_ = value;
    }

    const std::string& SparseFloatGrid::name() const noexcept
    {
        return name_;
    }

    void SparseFloatGrid::setName(std::string value)
    {
        name_ = std::move(value);
    }

    const math::M44f& SparseFloatGrid::indexToWorld() const noexcept
    {
        return index_to_world_;
    }

    void SparseFloatGrid::setIndexToWorld(const math::M44f& value)
    {
        if( std::any_of(value.ma.begin(), value.ma.end(),
                [](float component) { return !std::isfinite(component); }) )
        {
            throw std::invalid_argument("SparseFloatGrid transform must be finite");
        }
        index_to_world_ = value;
    }

    void SparseFloatGrid::setMetadata(std::string key, std::string value)
    {
        if( key.empty() )
            throw std::invalid_argument("SparseFloatGrid metadata key cannot be empty");
        metadata_.insert_or_assign(std::move(key), std::move(value));
    }

    std::optional<std::string> SparseFloatGrid::metadata(const std::string& key) const
    {
        const auto entry = metadata_.find(key);
        if( entry == metadata_.end() )
            return std::nullopt;
        return entry->second;
    }

    const std::map<std::string, std::string>& SparseFloatGrid::metadata() const noexcept
    {
        return metadata_;
    }

    void SparseFloatGrid::setVoxel(const math::V3i& index, float value)
    {
        if( !std::isfinite(value) )
            throw std::invalid_argument("SparseFloatGrid voxel value must be finite");
        voxels_.insert_or_assign(index, value);
    }

    bool SparseFloatGrid::eraseVoxel(const math::V3i& index) noexcept
    {
        return voxels_.erase(index) != 0;
    }

    bool SparseFloatGrid::isActive(const math::V3i& index) const noexcept
    {
        return voxels_.contains(index);
    }

    float SparseFloatGrid::value(const math::V3i& index) const noexcept
    {
        const auto entry = voxels_.find(index);
        return entry == voxels_.end() ? background_ : entry->second;
    }

    std::size_t SparseFloatGrid::activeVoxelCount() const noexcept
    {
        return voxels_.size();
    }

    std::optional<SparseIndexBounds> SparseFloatGrid::activeBounds() const noexcept
    {
        if( voxels_.empty() )
            return std::nullopt;
        auto entry = voxels_.begin();
        SparseIndexBounds bounds{entry->first, entry->first};
        for( ++entry; entry != voxels_.end(); ++entry )
        {
            const math::V3i& index = entry->first;
            bounds.minimum.x = std::min(bounds.minimum.x, index.x);
            bounds.minimum.y = std::min(bounds.minimum.y, index.y);
            bounds.minimum.z = std::min(bounds.minimum.z, index.z);
            bounds.maximum.x = std::max(bounds.maximum.x, index.x);
            bounds.maximum.y = std::max(bounds.maximum.y, index.y);
            bounds.maximum.z = std::max(bounds.maximum.z, index.z);
        }
        return bounds;
    }

    std::vector<SparseFloatVoxel> SparseFloatGrid::activeVoxels() const
    {
        std::vector<SparseFloatVoxel> result;
        result.reserve(voxels_.size());
        for( const auto& [index, value] : voxels_ )
            result.push_back({index, value});
        return result;
    }

    bool SparseFloatGrid::IndexLess::operator()(
        const math::V3i& left,
        const math::V3i& right) const noexcept
    {
        if( left.z != right.z )
            return left.z < right.z;
        if( left.y != right.y )
            return left.y < right.y;
        return left.x < right.x;
    }
}
