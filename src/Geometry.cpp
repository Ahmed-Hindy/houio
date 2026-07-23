#include <houio/Geometry.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <tuple>

namespace houio
{
    namespace
    {
        [[nodiscard]] Geometry::Index checkedIndex(int value, const char* description)
        {
            if (value < 0)
                throw std::out_of_range(std::string(description) + " cannot be negative");
            return static_cast<Geometry::Index>(value);
        }

        void validateGridResolution(int resolution, const char* axis)
        {
            if (resolution < 2)
                throw std::invalid_argument(
                    std::string("Geometry grid ") + axis + " resolution must be at least 2");
        }

        [[nodiscard]] math::Vec3f faceNormal(
            const math::Vec3f& point0,
            const math::Vec3f& point1,
            const math::Vec3f& point2)
        {
            const math::Vec3f edge0 = point1 - point0;
            const math::Vec3f edge1 = point2 - point0;
            const math::Vec3f normal = math::cross(edge0, edge1);
            if (normal.getLength() == 0.0f)
                return math::Vec3f(0.0f);
            return math::normalize(normal);
        }
    }

    Geometry::Geometry(PrimitiveType primitive_type)
        : primitive_type_(primitive_type),
          vertices_per_primitive_(fixedVertexCount(primitive_type))
    {
    }

    unsigned int Geometry::fixedVertexCount(PrimitiveType primitive_type)
    {
        switch (primitive_type)
        {
        case PrimitiveType::point:
            return 1;
        case PrimitiveType::line:
            return 2;
        case PrimitiveType::triangle:
            return 3;
        case PrimitiveType::quad:
            return 4;
        case PrimitiveType::polygon:
            return 0;
        }
        throw std::invalid_argument("Unknown Geometry primitive type");
    }

    Attribute::Ptr Geometry::getAttr(const std::string& name)
    {
        const auto attribute = attributes_.find(name);
        return attribute == attributes_.end() ? nullptr : attribute->second;
    }

    Attribute::CPtr Geometry::getAttr(const std::string& name) const
    {
        const auto attribute = attributes_.find(name);
        return attribute == attributes_.end() ? nullptr : attribute->second;
    }

    void Geometry::setAttr(const std::string& name, Attribute::Ptr attribute)
    {
        if (name.empty())
            throw std::invalid_argument("Geometry::setAttr requires a non-empty name");
        if (!attribute)
            throw std::invalid_argument("Geometry::setAttr received a null attribute");
        attributes_[name] = std::move(attribute);
    }

    bool Geometry::hasAttr(const std::string& name) const
    {
        return attributes_.contains(name);
    }

    void Geometry::getAttrNames(std::vector<std::string>& names) const
    {
        names.clear();
        names.reserve(attributes_.size());
        for (const auto& [name, attribute] : attributes_)
        {
            if (!attribute)
                throw std::runtime_error("Geometry contains a null attribute named " + name);
            names.push_back(name);
        }
    }

    void Geometry::removeAttr(const std::string& name)
    {
        attributes_.erase(name);
    }

    void Geometry::appendFixedPrimitive(std::span<const Index> point_indices)
    {
        if (primitive_type_ == PrimitiveType::polygon)
            throw std::logic_error("Fixed primitive insertion is unavailable for polygon geometry");
        if (point_indices.size() != vertices_per_primitive_)
            throw std::invalid_argument("Primitive vertex count does not match Geometry type");
        if (primitive_count_ == std::numeric_limits<unsigned int>::max())
            throw std::overflow_error("Geometry primitive count exceeds unsigned int range");
        indices_.insert(indices_.end(), point_indices.begin(), point_indices.end());
        ++primitive_count_;
    }

    unsigned int Geometry::addPoint(Index point_index)
    {
        const unsigned int primitive_index = primitive_count_;
        const std::array<Index, 1> indices = {point_index};
        appendFixedPrimitive(indices);
        return primitive_index;
    }

    unsigned int Geometry::addLine(Index point_index0, Index point_index1)
    {
        const unsigned int primitive_index = primitive_count_;
        const std::array<Index, 2> indices = {point_index0, point_index1};
        appendFixedPrimitive(indices);
        return primitive_index;
    }

    unsigned int Geometry::addTriangle(
        Index point_index0,
        Index point_index1,
        Index point_index2)
    {
        const unsigned int primitive_index = primitive_count_;
        const std::array<Index, 3> indices = {
            point_index0,
            point_index1,
            point_index2,
        };
        appendFixedPrimitive(indices);
        return primitive_index;
    }

    unsigned int Geometry::addQuad(
        Index point_index0,
        Index point_index1,
        Index point_index2,
        Index point_index3)
    {
        const unsigned int primitive_index = primitive_count_;
        const std::array<Index, 4> indices = {
            point_index0,
            point_index1,
            point_index2,
            point_index3,
        };
        appendFixedPrimitive(indices);
        return primitive_index;
    }

    unsigned int Geometry::addPolygonVertex(Index point_index)
    {
        if (primitive_type_ != PrimitiveType::polygon)
            throw std::logic_error("Polygon vertices require polygon Geometry");
        if (vertices_per_primitive_ == std::numeric_limits<unsigned int>::max())
            throw std::overflow_error("Polygon vertex count exceeds unsigned int range");
        indices_.push_back(point_index);
        ++vertices_per_primitive_;
        primitive_count_ = 1;
        return 0;
    }

    void Geometry::reverse()
    {
        if (primitive_count_ == 0)
            return;
        if (vertices_per_primitive_ == 0)
            throw std::runtime_error("Geometry has primitives without vertices");
        const std::size_t expected_index_count =
            static_cast<std::size_t>(primitive_count_) * vertices_per_primitive_;
        if (indices_.size() != expected_index_count)
            throw std::runtime_error("Geometry topology does not match its primitive counts");

        for (unsigned int primitive_index = 0; primitive_index < primitive_count_; ++primitive_index)
        {
            const auto first = indices_.begin()
                + static_cast<std::ptrdiff_t>(primitive_index * vertices_per_primitive_);
            std::reverse(first, first + vertices_per_primitive_);
        }
    }

    Geometry::Index Geometry::duplicatePoint(Index point_index)
    {
        const Attribute::Ptr positions = getAttr("P");
        if (!positions)
            throw std::runtime_error("Geometry::duplicatePoint requires a P attribute");

        const int point_count = positions->numElements();
        if (point_count < 0 || point_index >= static_cast<Index>(point_count))
            throw std::out_of_range("Geometry::duplicatePoint index is out of range");
        if (point_count == std::numeric_limits<int>::max())
            throw std::overflow_error("Geometry::duplicatePoint exceeds the supported point range");

        const Index duplicate_index = static_cast<Index>(point_count);
        for (const auto& [name, attribute] : attributes_)
        {
            if (!attribute)
                throw std::runtime_error("Geometry contains a null attribute named " + name);
            if (attribute->numElements() != point_count)
                throw std::runtime_error(
                    "Geometry attribute counts are inconsistent while duplicating a point");
            if (attribute->duplicateElement(point_index) != duplicate_index)
                throw std::runtime_error("Geometry attributes produced different duplicate indices");
        }
        return duplicate_index;
    }

    void Geometry::transform(const math::M44f& transform_matrix)
    {
        const Attribute::Ptr positions = getAttr("P");
        if (!positions)
            throw std::runtime_error("Geometry::transform requires a P attribute");
        if (positions->numComponents() != 3
            || positions->elementComponentType() != Attribute::ComponentType::float32)
        {
            throw std::runtime_error("Geometry::transform requires a Float32 P attribute with three components");
        }

        const int point_count = positions->numElements();
        for (int point_index = 0; point_index < point_count; ++point_index)
        {
            const math::Vec3f transformed = math::transform(
                positions->get<math::Vec3f>(static_cast<unsigned int>(point_index)),
                transform_matrix);
            positions->set<math::Vec3f>(static_cast<unsigned int>(point_index), transformed);
        }
    }

    void Geometry::addNormals()
    {
        if (primitive_type_ != PrimitiveType::triangle && primitive_type_ != PrimitiveType::quad)
            throw std::logic_error("Geometry::addNormals supports only triangle and quad geometry");

        const Attribute::Ptr positions = getAttr("P");
        if (!positions)
            throw std::runtime_error("Geometry::addNormals requires a P attribute");
        if (positions->numComponents() != 3
            || positions->elementComponentType() != Attribute::ComponentType::float32)
        {
            throw std::runtime_error("Geometry::addNormals requires a Float32 P attribute with three components");
        }

        const int point_count = positions->numElements();
        if (point_count < 0)
            throw std::runtime_error("Geometry has a negative point count");
        if (indices_.size() != static_cast<std::size_t>(primitive_count_) * vertices_per_primitive_)
            throw std::runtime_error("Geometry topology does not match its primitive counts");

        Attribute::Ptr normals = getAttr("N");
        if (!normals)
            normals = Attribute::createV3f(point_count);
        else
        {
            if (normals->numComponents() != 3
                || normals->elementComponentType() != Attribute::ComponentType::float32)
            {
                throw std::runtime_error("Geometry N attribute must contain Float32 vectors");
            }
            normals->resize(static_cast<std::size_t>(point_count));
        }
        for (int point_index = 0; point_index < point_count; ++point_index)
        {
            normals->set<math::Vec3f>(
                static_cast<unsigned int>(point_index),
                math::Vec3f(0.0f));
        }

        for (unsigned int primitive_index = 0; primitive_index < primitive_count_; ++primitive_index)
        {
            const std::size_t primitive_offset =
                static_cast<std::size_t>(primitive_index) * vertices_per_primitive_;
            const Index point0_index = indices_[primitive_offset];
            const Index point1_index = indices_[primitive_offset + 1];
            const Index point2_index = indices_[primitive_offset + 2];
            if (point0_index >= static_cast<Index>(point_count)
                || point1_index >= static_cast<Index>(point_count)
                || point2_index >= static_cast<Index>(point_count))
            {
                throw std::out_of_range("Geometry primitive references a point outside P");
            }

            const math::Vec3f normal0 = faceNormal(
                positions->get<math::Vec3f>(point0_index),
                positions->get<math::Vec3f>(point1_index),
                positions->get<math::Vec3f>(point2_index));
            for (unsigned int local_index = 0; local_index < 3; ++local_index)
            {
                const Index point_index = indices_[primitive_offset + local_index];
                normals->set<math::Vec3f>(
                    point_index,
                    normals->get<math::Vec3f>(point_index) + normal0);
            }

            if (primitive_type_ == PrimitiveType::quad)
            {
                const Index point3_index = indices_[primitive_offset + 3];
                if (point3_index >= static_cast<Index>(point_count))
                    throw std::out_of_range("Geometry quad references a point outside P");
                const math::Vec3f normal1 = faceNormal(
                    positions->get<math::Vec3f>(point0_index),
                    positions->get<math::Vec3f>(point2_index),
                    positions->get<math::Vec3f>(point3_index));
                for (const Index point_index : {point0_index, point2_index, point3_index})
                {
                    normals->set<math::Vec3f>(
                        point_index,
                        normals->get<math::Vec3f>(point_index) + normal1);
                }
            }
        }

        for (int point_index = 0; point_index < point_count; ++point_index)
        {
            const Index index = static_cast<Index>(point_index);
            const math::Vec3f accumulated = normals->get<math::Vec3f>(index);
            normals->set<math::Vec3f>(
                index,
                accumulated.getLength() == 0.0f ? accumulated : math::normalize(accumulated));
        }
        setAttr("N", std::move(normals));
    }

    math::BoundingBox3f Geometry::getBound() const
    {
        math::BoundingBox3f bound;
        const Attribute::CPtr positions = getAttr("P");
        if (!positions)
            return bound;
        if (positions->numComponents() != 3
            || positions->elementComponentType() != Attribute::ComponentType::float32)
        {
            throw std::runtime_error("Geometry::getBound requires a Float32 P attribute with three components");
        }
        for (int point_index = 0; point_index < positions->numElements(); ++point_index)
            bound.extend(positions->get<math::V3f>(static_cast<unsigned int>(point_index)));
        return bound;
    }

    void Geometry::clear()
    {
        attributes_.clear();
        indices_.clear();
        primitive_count_ = 0;
        vertices_per_primitive_ = fixedVertexCount(primitive_type_);
    }

    Geometry::Ptr Geometry::createPointGeometry()
    {
        auto geometry = std::make_shared<Geometry>(PrimitiveType::point);
        geometry->setAttr("P", Attribute::createV3f());
        return geometry;
    }

    Geometry::Ptr Geometry::createLineGeometry()
    {
        auto geometry = std::make_shared<Geometry>(PrimitiveType::line);
        geometry->setAttr("P", Attribute::createV3f());
        return geometry;
    }

    Geometry::Ptr Geometry::createTriangleGeometry()
    {
        auto geometry = std::make_shared<Geometry>(PrimitiveType::triangle);
        geometry->setAttr("P", Attribute::createV3f());
        return geometry;
    }

    Geometry::Ptr Geometry::createQuadGeometry()
    {
        auto geometry = std::make_shared<Geometry>(PrimitiveType::quad);
        geometry->setAttr("P", Attribute::createV3f());
        return geometry;
    }

    Geometry::Ptr Geometry::createPolyGeometry()
    {
        auto geometry = std::make_shared<Geometry>(PrimitiveType::polygon);
        geometry->setAttr("P", Attribute::createV3f());
        return geometry;
    }

    Geometry::Ptr Geometry::createQuad(PrimitiveType primitive_type)
    {
        if (primitive_type != PrimitiveType::triangle && primitive_type != PrimitiveType::quad)
            throw std::invalid_argument("Geometry::createQuad supports triangle or quad output");

        auto geometry = std::make_shared<Geometry>(primitive_type);
        auto positions = Attribute::createV3f();
        auto texture_coordinates = Attribute::createV2f();

        positions->appendElement(math::Vec3f(-1.0f, -1.0f, 0.0f));
        texture_coordinates->appendElement(0.0f, 0.0f);
        positions->appendElement(math::Vec3f(-1.0f, 1.0f, 0.0f));
        texture_coordinates->appendElement(0.0f, 1.0f);
        positions->appendElement(math::Vec3f(1.0f, 1.0f, 0.0f));
        texture_coordinates->appendElement(1.0f, 1.0f);
        positions->appendElement(math::Vec3f(1.0f, -1.0f, 0.0f));
        texture_coordinates->appendElement(1.0f, 0.0f);

        geometry->setAttr("P", positions);
        geometry->setAttr("UV", texture_coordinates);
        if (primitive_type == PrimitiveType::quad)
            geometry->addQuad(3, 2, 1, 0);
        else
        {
            geometry->addTriangle(3, 2, 1);
            geometry->addTriangle(3, 1, 0);
        }
        return geometry;
    }

    Geometry::Ptr Geometry::createGrid(
        int x_resolution,
        int z_resolution,
        PrimitiveType primitive_type)
    {
        validateGridResolution(x_resolution, "X");
        validateGridResolution(z_resolution, "Z");
        if (primitive_type != PrimitiveType::point
            && primitive_type != PrimitiveType::line
            && primitive_type != PrimitiveType::triangle)
        {
            throw std::invalid_argument("Geometry::createGrid supports point, line, or triangle output");
        }

        auto geometry = std::make_shared<Geometry>(primitive_type);
        auto positions = Attribute::createV3f();
        auto texture_coordinates = Attribute::createV2f();
        geometry->setAttr("P", positions);
        geometry->setAttr("UV", texture_coordinates);

        for (int z_index = 0; z_index < z_resolution; ++z_index)
        {
            for (int x_index = 0; x_index < x_resolution; ++x_index)
            {
                const float u = static_cast<float>(x_index) / static_cast<float>(x_resolution - 1);
                const float v = static_cast<float>(z_index) / static_cast<float>(z_resolution - 1);
                positions->appendElement(math::V3f(u - 0.5f, 0.0f, v - 0.5f));
                texture_coordinates->appendElement(u, 1.0f - v);
            }
        }

        if (primitive_type == PrimitiveType::point)
        {
            const int point_count = x_resolution * z_resolution;
            for (int point_index = 0; point_index < point_count; ++point_index)
                geometry->addPoint(checkedIndex(point_index, "Grid point index"));
        }
        else if (primitive_type == PrimitiveType::line)
        {
            for (int z_index = 0; z_index < z_resolution; ++z_index)
            {
                const int row_offset = z_index * x_resolution;
                for (int x_index = 0; x_index < x_resolution - 1; ++x_index)
                {
                    geometry->addLine(
                        checkedIndex(row_offset + x_index, "Grid line index"),
                        checkedIndex(row_offset + x_index + 1, "Grid line index"));
                }
            }
            for (int x_index = 0; x_index < x_resolution; ++x_index)
            {
                for (int z_index = 0; z_index < z_resolution - 1; ++z_index)
                {
                    geometry->addLine(
                        checkedIndex(z_index * x_resolution + x_index, "Grid line index"),
                        checkedIndex((z_index + 1) * x_resolution + x_index, "Grid line index"));
                }
            }
        }
        else
        {
            for (int z_index = 0; z_index < z_resolution - 1; ++z_index)
            {
                const int row_offset = z_index * x_resolution;
                for (int x_index = 0; x_index < x_resolution - 1; ++x_index)
                {
                    const Index lower_left = checkedIndex(row_offset + x_index, "Grid triangle index");
                    const Index lower_right = checkedIndex(row_offset + x_index + 1, "Grid triangle index");
                    const Index upper_left = checkedIndex(
                        row_offset + x_resolution + x_index,
                        "Grid triangle index");
                    const Index upper_right = checkedIndex(
                        row_offset + x_resolution + x_index + 1,
                        "Grid triangle index");
                    geometry->addTriangle(upper_right, lower_right, lower_left);
                    geometry->addTriangle(upper_left, upper_right, lower_left);
                }
            }
        }
        return geometry;
    }

    Geometry::Ptr Geometry::createGrid(
        int x_resolution,
        int y_resolution,
        int z_resolution,
        PrimitiveType primitive_type)
    {
        if (x_resolution < 1 || y_resolution < 1 || z_resolution < 1)
            throw std::invalid_argument("Geometry 3D grid resolutions must be positive");
        if (primitive_type != PrimitiveType::point && primitive_type != PrimitiveType::line)
            throw std::invalid_argument("Geometry 3D grid supports point or line output");

        auto geometry = std::make_shared<Geometry>(primitive_type);
        auto positions = Attribute::createV3f();
        geometry->setAttr("P", positions);

        const int point_count_x = x_resolution + 1;
        const int point_count_y = y_resolution + 1;
        const int point_count_z = z_resolution + 1;
        const auto linear_index = [=](int x, int y, int z)
        {
            return z * point_count_x * point_count_y + y * point_count_x + x;
        };

        for (int z_index = 0; z_index < point_count_z; ++z_index)
        {
            for (int y_index = 0; y_index < point_count_y; ++y_index)
            {
                for (int x_index = 0; x_index < point_count_x; ++x_index)
                {
                    const float u = static_cast<float>(x_index) / static_cast<float>(x_resolution);
                    const float v = static_cast<float>(y_index) / static_cast<float>(y_resolution);
                    const float w = static_cast<float>(z_index) / static_cast<float>(z_resolution);
                    positions->appendElement(math::V3f(u - 0.5f, v - 0.5f, w - 0.5f));
                }
            }
        }

        if (primitive_type == PrimitiveType::point)
        {
            for (int point_index = 0; point_index < positions->numElements(); ++point_index)
                geometry->addPoint(checkedIndex(point_index, "Grid point index"));
            return geometry;
        }

        for (int z_index = 0; z_index < point_count_z; ++z_index)
        {
            for (int y_index = 0; y_index < point_count_y; ++y_index)
            {
                for (int x_index = 0; x_index < point_count_x; ++x_index)
                {
                    const Index current = checkedIndex(
                        linear_index(x_index, y_index, z_index),
                        "Grid line index");
                    if (x_index + 1 < point_count_x)
                    {
                        geometry->addLine(
                            current,
                            checkedIndex(linear_index(x_index + 1, y_index, z_index), "Grid line index"));
                    }
                    if (y_index + 1 < point_count_y)
                    {
                        geometry->addLine(
                            current,
                            checkedIndex(linear_index(x_index, y_index + 1, z_index), "Grid line index"));
                    }
                    if (z_index + 1 < point_count_z)
                    {
                        geometry->addLine(
                            current,
                            checkedIndex(linear_index(x_index, y_index, z_index + 1), "Grid line index"));
                    }
                }
            }
        }
        return geometry;
    }

    Geometry::Ptr Geometry::createSphere(
        int u_subdivisions,
        int v_subdivisions,
        float radius,
        math::Vec3f center,
        PrimitiveType primitive_type)
    {
        if (u_subdivisions < 3 || v_subdivisions < 2)
            throw std::invalid_argument("Geometry sphere requires at least 3x2 subdivisions");
        if (radius < 0.0f)
            throw std::invalid_argument("Geometry sphere radius cannot be negative");
        if (primitive_type != PrimitiveType::point
            && primitive_type != PrimitiveType::line
            && primitive_type != PrimitiveType::triangle)
        {
            throw std::invalid_argument("Geometry::createSphere supports point, line, or triangle output");
        }

        auto geometry = std::make_shared<Geometry>(primitive_type);
        auto positions = Attribute::createV3f();
        geometry->setAttr("P", positions);

        const Index north_pole = positions->appendElement(math::V3f(0.0f, radius, 0.0f) + center);
        for (int latitude = 1; latitude < v_subdivisions; ++latitude)
        {
            const float theta = std::numbers::pi_v<float>
                * static_cast<float>(latitude) / static_cast<float>(v_subdivisions);
            const float radial = std::sin(theta);
            const float y = std::cos(theta);
            for (int longitude = 0; longitude < u_subdivisions; ++longitude)
            {
                const float phi = 2.0f * std::numbers::pi_v<float>
                    * static_cast<float>(longitude) / static_cast<float>(u_subdivisions);
                const math::V3f unit(
                    radial * std::cos(phi),
                    y,
                    radial * std::sin(phi));
                positions->appendElement(unit * radius + center);
            }
        }
        const Index south_pole = positions->appendElement(math::V3f(0.0f, -radius, 0.0f) + center);

        const auto ring_index = [=](int latitude, int longitude)
        {
            const int wrapped_longitude = (longitude + u_subdivisions) % u_subdivisions;
            return static_cast<Index>(1 + (latitude - 1) * u_subdivisions + wrapped_longitude);
        };

        if (primitive_type == PrimitiveType::point)
        {
            for (int point_index = 0; point_index < positions->numElements(); ++point_index)
                geometry->addPoint(checkedIndex(point_index, "Sphere point index"));
            return geometry;
        }

        if (primitive_type == PrimitiveType::line)
        {
            for (int longitude = 0; longitude < u_subdivisions; ++longitude)
            {
                geometry->addLine(north_pole, ring_index(1, longitude));
                for (int latitude = 1; latitude < v_subdivisions - 1; ++latitude)
                {
                    geometry->addLine(
                        ring_index(latitude, longitude),
                        ring_index(latitude + 1, longitude));
                }
                geometry->addLine(ring_index(v_subdivisions - 1, longitude), south_pole);
            }
            for (int latitude = 1; latitude < v_subdivisions; ++latitude)
            {
                for (int longitude = 0; longitude < u_subdivisions; ++longitude)
                {
                    geometry->addLine(
                        ring_index(latitude, longitude),
                        ring_index(latitude, longitude + 1));
                }
            }
            return geometry;
        }

        for (int longitude = 0; longitude < u_subdivisions; ++longitude)
        {
            geometry->addTriangle(
                ring_index(1, longitude + 1),
                ring_index(1, longitude),
                north_pole);
        }
        for (int latitude = 1; latitude < v_subdivisions - 1; ++latitude)
        {
            for (int longitude = 0; longitude < u_subdivisions; ++longitude)
            {
                const Index lower_left = ring_index(latitude, longitude);
                const Index lower_right = ring_index(latitude, longitude + 1);
                const Index upper_left = ring_index(latitude + 1, longitude);
                const Index upper_right = ring_index(latitude + 1, longitude + 1);
                geometry->addTriangle(lower_right, upper_left, lower_left);
                geometry->addTriangle(lower_right, upper_right, upper_left);
            }
        }
        for (int longitude = 0; longitude < u_subdivisions; ++longitude)
        {
            geometry->addTriangle(
                ring_index(v_subdivisions - 1, longitude),
                ring_index(v_subdivisions - 1, longitude + 1),
                south_pole);
        }
        return geometry;
    }

    Geometry::Ptr Geometry::createBox(
        const math::BoundingBox3f& bound,
        PrimitiveType primitive_type)
    {
        if (primitive_type != PrimitiveType::point
            && primitive_type != PrimitiveType::line
            && primitive_type != PrimitiveType::triangle
            && primitive_type != PrimitiveType::quad)
        {
            throw std::invalid_argument("Geometry::createBox does not support polygon output");
        }

        const std::array<math::Vec3f, 8> corners = {
            math::Vec3f(bound.minPoint.x, bound.minPoint.y, bound.maxPoint.z),
            math::Vec3f(bound.minPoint.x, bound.maxPoint.y, bound.maxPoint.z),
            math::Vec3f(bound.maxPoint.x, bound.maxPoint.y, bound.maxPoint.z),
            math::Vec3f(bound.maxPoint.x, bound.minPoint.y, bound.maxPoint.z),
            math::Vec3f(bound.minPoint.x, bound.minPoint.y, bound.minPoint.z),
            math::Vec3f(bound.minPoint.x, bound.maxPoint.y, bound.minPoint.z),
            math::Vec3f(bound.maxPoint.x, bound.maxPoint.y, bound.minPoint.z),
            math::Vec3f(bound.maxPoint.x, bound.minPoint.y, bound.minPoint.z),
        };
        const std::array<std::array<int, 4>, 6> faces = {{
            {3, 2, 1, 0},
            {4, 5, 6, 7},
            {7, 6, 2, 3},
            {1, 5, 4, 0},
            {6, 5, 1, 2},
            {4, 7, 3, 0},
        }};

        auto geometry = std::make_shared<Geometry>(primitive_type);
        auto positions = Attribute::createV3f();

        if (primitive_type == PrimitiveType::point || primitive_type == PrimitiveType::line)
        {
            for (const math::Vec3f& corner : corners)
                positions->appendElement(corner);
            geometry->setAttr("P", positions);
            if (primitive_type == PrimitiveType::point)
            {
                for (Index point_index = 0; point_index < corners.size(); ++point_index)
                    geometry->addPoint(point_index);
                return geometry;
            }
            const std::array<std::array<Index, 2>, 12> edges = {{
                {0, 1}, {1, 2}, {2, 3}, {3, 0},
                {4, 5}, {5, 6}, {6, 7}, {7, 4},
                {0, 4}, {1, 5}, {2, 6}, {3, 7},
            }};
            for (const auto& edge : edges)
                geometry->addLine(edge[0], edge[1]);
            return geometry;
        }

        auto texture_coordinates = Attribute::createV2f();
        for (const auto& face : faces)
        {
            const Index point0 = positions->appendElement(corners[static_cast<std::size_t>(face[0])]);
            texture_coordinates->appendElement(math::Vec2f(0.0f, 0.0f));
            const Index point1 = positions->appendElement(corners[static_cast<std::size_t>(face[1])]);
            texture_coordinates->appendElement(math::Vec2f(1.0f, 0.0f));
            const Index point2 = positions->appendElement(corners[static_cast<std::size_t>(face[2])]);
            texture_coordinates->appendElement(math::Vec2f(1.0f, 1.0f));
            const Index point3 = positions->appendElement(corners[static_cast<std::size_t>(face[3])]);
            texture_coordinates->appendElement(math::Vec2f(0.0f, 1.0f));
            if (primitive_type == PrimitiveType::quad)
                geometry->addQuad(point0, point1, point2, point3);
            else
            {
                geometry->addTriangle(point0, point1, point2);
                geometry->addTriangle(point0, point2, point3);
            }
        }
        geometry->setAttr("P", positions);
        geometry->setAttr("UV", texture_coordinates);
        return geometry;
    }

    Geometry::Ptr Geometry::createLine(
        const math::V3f& point0,
        const math::V3f& point1)
    {
        auto geometry = createLineGeometry();
        const Attribute::Ptr positions = geometry->getAttr("P");
        const Index index0 = positions->appendElement(point0);
        const Index index1 = positions->appendElement(point1);
        geometry->addLine(index0, index1);
        return geometry;
    }

    Geometry::Ptr Geometry::merge(const std::vector<Ptr>& geometries)
    {
        if (geometries.empty())
            return nullptr;
        const Ptr& reference = geometries.front();
        if (!reference)
            throw std::invalid_argument("Geometry::merge received a null geometry");

        auto result = std::make_shared<Geometry>(reference->primitiveType());
        std::vector<std::string> attribute_names;
        reference->getAttrNames(attribute_names);
        for (const std::string& name : attribute_names)
        {
            const Attribute::Ptr source = reference->getAttr(name);
            if (!source)
                throw std::runtime_error("Geometry::merge reference contains a null attribute");
            result->setAttr(
                name,
                std::make_shared<Attribute>(
                    source->numComponents(),
                    source->elementComponentType()));
        }

        for (const Ptr& geometry : geometries)
        {
            if (!geometry)
                throw std::invalid_argument("Geometry::merge received a null geometry");
            if (geometry->primitiveType() != reference->primitiveType())
                throw std::invalid_argument("Geometry::merge requires a common primitive type");

            std::vector<std::string> source_attribute_names;
            geometry->getAttrNames(source_attribute_names);
            if (source_attribute_names != attribute_names)
                throw std::invalid_argument("Geometry::merge requires identical attribute names");

            const Attribute::Ptr result_positions = result->getAttr("P");
            if (!result_positions)
                throw std::runtime_error("Geometry::merge requires a P attribute");
            const int point_offset_value = result_positions->numElements();
            if (point_offset_value < 0)
                throw std::runtime_error("Geometry::merge encountered a negative point count");
            const Index point_offset = static_cast<Index>(point_offset_value);

            for (const std::string& name : attribute_names)
            {
                const Attribute::Ptr source = geometry->getAttr(name);
                const Attribute::Ptr destination = result->getAttr(name);
                if (!source || !destination)
                    throw std::runtime_error("Geometry::merge encountered a missing attribute");
                if (source->numComponents() != destination->numComponents()
                    || source->elementComponentType() != destination->elementComponentType())
                {
                    throw std::invalid_argument("Geometry::merge attribute layouts do not match");
                }
                destination->append(*source);
            }

            if (geometry->primitiveType() == PrimitiveType::polygon)
            {
                if (result->primitive_count_ != 0 && geometry->primitive_count_ != 0)
                    throw std::invalid_argument("Geometry::merge cannot combine multiple polygon records");
                for (const Index index : geometry->indices_)
                    result->addPolygonVertex(index + point_offset);
            }
            else
            {
                for (const Index index : geometry->indices_)
                {
                    if (index > std::numeric_limits<Index>::max() - point_offset)
                        throw std::overflow_error("Geometry::merge index exceeds unsigned int range");
                    result->indices_.push_back(index + point_offset);
                }
                if (geometry->primitive_count_
                    > std::numeric_limits<unsigned int>::max() - result->primitive_count_)
                {
                    throw std::overflow_error("Geometry::merge primitive count exceeds unsigned int range");
                }
                result->primitive_count_ += geometry->primitive_count_;
            }
        }
        return result;
    }
}
