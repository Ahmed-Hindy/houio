#pragma once

#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <houio/Attribute.h>
#include <houio/Field.h>
#include <houio/HouGeoAdapter.h>
#include <houio/json.h>

namespace houio
{
    class HouGeo : public HouGeoAdapter
    {
    public:
        using Ptr = std::shared_ptr<HouGeo>;
        using ConstPtr = std::shared_ptr<const HouGeo>;

        class HouAttribute final : public AttributeAdapter
        {
        public:
            using Ptr = std::shared_ptr<HouAttribute>;
            using ConstPtr = std::shared_ptr<const HouAttribute>;

            HouAttribute();
            HouAttribute(const std::string& name, Attribute::Ptr attribute);

            [[nodiscard]] std::string name() const override;
            [[nodiscard]] Type type() const override;
            [[nodiscard]] int tupleSize() const override;
            [[nodiscard]] Storage storage() const override;
            [[nodiscard]] std::vector<int> packing() const override;
            [[nodiscard]] int elementCount() const override;
            [[nodiscard]] std::string stringValue(int index) const override;
            [[nodiscard]] std::shared_ptr<json::Object> dictionaryValue(int index) const override;
            [[nodiscard]] RawDataView rawData() const override;

            int addString(const std::string& value);

            void setName(std::string name)
            {
                name_ = std::move(name);
            }

            void setTupleSize(int tuple_size)
            {
                if (tuple_size <= 0)
                    throw std::invalid_argument("HouAttribute tuple size must be positive");
                tuple_size_ = tuple_size;
            }

            void setStorage(Storage storage) noexcept
            {
                storage_ = storage;
            }

            void setType(Type type) noexcept
            {
                type_ = type;
            }

            void setElementCount(int element_count)
            {
                if (element_count < 0)
                    throw std::invalid_argument("HouAttribute element count cannot be negative");
                element_count_ = element_count;
            }

            void setNumericAttribute(Attribute::Ptr attribute)
            {
                if (!attribute)
                    throw std::invalid_argument("HouAttribute numeric storage cannot be null");
                numeric_attribute_ = std::move(attribute);
                tuple_size_ = numeric_attribute_->numComponents();
                element_count_ = numeric_attribute_->numElements();
                type_ = Type::numeric;
            }

            [[nodiscard]] Attribute::Ptr numericAttribute() noexcept
            {
                return numeric_attribute_;
            }

            [[nodiscard]] Attribute::CPtr numericAttribute() const noexcept
            {
                return numeric_attribute_;
            }

            void setStringValues(std::vector<std::string> values)
            {
                string_values_ = std::move(values);
                dictionary_values_.clear();
                numeric_attribute_.reset();
                type_ = Type::string;
                storage_ = Storage::int32;
                tuple_size_ = 1;
                element_count_ = static_cast<int>(string_values_.size());
            }

            [[nodiscard]] const std::vector<std::string>& stringValues() const noexcept
            {
                return string_values_;
            }

            void setDictionaryValues(std::vector<json::ObjectPtr> values)
            {
                dictionary_values_ = std::move(values);
                string_values_.clear();
                numeric_attribute_.reset();
                type_ = Type::dictionary;
                storage_ = Storage::int32;
                tuple_size_ = 1;
                element_count_ = static_cast<int>(dictionary_values_.size());
            }

            [[nodiscard]] const std::vector<json::ObjectPtr>& dictionaryValues() const noexcept
            {
                return dictionary_values_;
            }

        private:
            std::string name_;
            int tuple_size_ = 1;
            Storage storage_ = Storage::invalid;
            Type type_ = Type::numeric;
            std::vector<std::string> string_values_;
            std::vector<json::ObjectPtr> dictionary_values_;
            int element_count_ = 0;
            Attribute::Ptr numeric_attribute_;

            friend class HouGeo;
        };

        class HouTopology final : public Topology
        {
        public:
            using Ptr = std::shared_ptr<HouTopology>;
            using ConstPtr = std::shared_ptr<const HouTopology>;

            HouTopology();

            [[nodiscard]] std::vector<int> indexValues() const override;
            void appendIndices(std::span<const int> indices) override;
            [[nodiscard]] sint64 indexCount() const override;

            void reserve(std::size_t index_count)
            {
                indexBuffer.reserve(index_count);
            }

            void appendIndex(int point_index)
            {
                indexBuffer.push_back(point_index);
            }

            void setIndices(std::vector<int> indices)
            {
                indexBuffer = std::move(indices);
            }

            [[nodiscard]] std::span<const int> indices() const noexcept
            {
                return indexBuffer;
            }

        private:
            std::vector<int> indexBuffer;

            friend class HouGeo;
        };

        class HouVolume final : public VolumePrimitive
        {
        public:
            using Ptr = std::shared_ptr<HouVolume>;
            using ConstPtr = std::shared_ptr<const HouVolume>;

            [[nodiscard]] math::M44f transform() const override;
            [[nodiscard]] int topologyVertex() const override;
            [[nodiscard]] math::Vec3i resolution() const override;
            [[nodiscard]] RawDataView rawData() const override;
            [[nodiscard]] real32 voxelValue(int x, int y, int z) const override;
            [[nodiscard]] std::string visualizationMode() const override;
            [[nodiscard]] real32 visualizationIso() const override;
            [[nodiscard]] real32 visualizationDensity() const override;

            void setField(ScalarField::Ptr scalar_field)
            {
                if (!scalar_field)
                    throw std::invalid_argument("HouVolume field cannot be null");
                scalar_field_ = std::move(scalar_field);
            }

            [[nodiscard]] ScalarField::Ptr scalarField() noexcept
            {
                return scalar_field_;
            }

            [[nodiscard]] std::shared_ptr<const ScalarField> scalarField() const noexcept
            {
                return scalar_field_;
            }

            void setTopologyVertex(int topology_vertex) noexcept
            {
                topology_vertex_ = topology_vertex;
            }

            void setVisualization(
                std::string mode,
                real32 iso_value,
                real32 density)
            {
                visualization_mode_ = mode.empty() ? "smoke" : std::move(mode);
                visualization_iso_ = iso_value;
                visualization_density_ = density;
            }

        private:
            ScalarField::Ptr scalar_field_;
            int topology_vertex_ = -1;
            std::string visualization_mode_ = "smoke";
            real32 visualization_iso_ = 0.0f;
            real32 visualization_density_ = 1.0f;

            friend class HouGeo;
        };

        class HouPoly final : public PolyPrimitive
        {
        public:
            using Ptr = std::shared_ptr<HouPoly>;
            using ConstPtr = std::shared_ptr<const HouPoly>;

            [[nodiscard]] int polygonCount() const override;
            [[nodiscard]] int polygonVertexCount(int polygon_index) const override;
            [[nodiscard]] std::span<const int> polygonVertexIndices(
                int polygon_index = 0) const override;
            [[nodiscard]] bool isClosed() const override;

            void setPolygonData(
                int polygon_count,
                std::vector<int> vertex_counts,
                std::vector<int> vertex_offsets,
                std::vector<int> point_indices,
                bool is_closed)
            {
                if (polygon_count < 0)
                    throw std::invalid_argument("HouPoly polygon count cannot be negative");
                m_numPolys = polygon_count;
                m_perPolyVertexCount = std::move(vertex_counts);
                m_perPolyVertexListOffset = std::move(vertex_offsets);
                m_vertices = std::move(point_indices);
                m_closed = is_closed;
            }

            [[nodiscard]] std::span<const int> vertexCounts() const noexcept
            {
                return m_perPolyVertexCount;
            }

            [[nodiscard]] std::span<const int> vertexOffsets() const noexcept
            {
                return m_perPolyVertexListOffset;
            }

            [[nodiscard]] std::span<const int> pointIndices() const noexcept
            {
                return m_vertices;
            }

        private:
            int m_numPolys = 0;
            std::vector<int> m_perPolyVertexCount;
            std::vector<int> m_perPolyVertexListOffset;
            std::vector<int> m_vertices;
            bool m_closed = true;

            friend class HouGeo;
        };

        HouGeo();

        [[nodiscard]] static Ptr create();

        void setPointAttribute(HouAttribute::Ptr attribute);
        void setPrimitiveAttribute(const std::string& name, HouAttribute::Ptr attribute);
        void setPointGroup(const std::string& name, const std::vector<bool>& membership);
        void setVertexGroup(const std::string& name, const std::vector<bool>& membership);
        void setPrimitiveGroup(const std::string& name, const std::vector<bool>& membership);
        void addPrimitive(ScalarField::Ptr field);
        void addPrimitive(PolyPrimitive::Ptr polygon);
        void setTopology(HouTopology::Ptr topology);

        [[nodiscard]] sint64 pointCount() const override;
        [[nodiscard]] sint64 vertexCount() const override;
        [[nodiscard]] sint64 primitiveCount() const override;
        [[nodiscard]] std::vector<std::string> pointAttributeNames() const override;
        [[nodiscard]] AttributeAdapter::Ptr pointAttribute(const std::string& name) override;
        [[nodiscard]] std::vector<std::string> vertexAttributeNames() const override;
        [[nodiscard]] AttributeAdapter::Ptr vertexAttribute(const std::string& name) override;
        [[nodiscard]] bool hasPrimitiveAttribute(const std::string& name) const override;
        [[nodiscard]] std::vector<std::string> primitiveAttributeNames() const override;
        [[nodiscard]] AttributeAdapter::Ptr primitiveAttribute(const std::string& name) override;
        [[nodiscard]] std::vector<Primitive::Ptr> primitives() override;
        [[nodiscard]] std::vector<std::string> globalAttributeNames() const override;
        [[nodiscard]] AttributeAdapter::Ptr globalAttribute(const std::string& name) override;
        [[nodiscard]] std::vector<std::string> pointGroupNames() const override;
        [[nodiscard]] std::optional<std::vector<bool>> pointGroupMembership(
            const std::string& name) const override;
        [[nodiscard]] std::vector<std::string> vertexGroupNames() const override;
        [[nodiscard]] std::optional<std::vector<bool>> vertexGroupMembership(
            const std::string& name) const override;
        [[nodiscard]] std::vector<std::string> primitiveGroupNames() const override;
        [[nodiscard]] std::optional<std::vector<bool>> primitiveGroupMembership(
            const std::string& name) const override;
        [[nodiscard]] Topology::Ptr topology() override;

        struct SharedPrimitiveData
        {
            std::map<std::string, json::ObjectPtr> sharedVoxelData;
        };

        void load(json::ObjectPtr root_object);
        [[nodiscard]] static json::ObjectPtr toObject(json::ArrayPtr flattened_array);

    private:
        [[nodiscard]] HouAttribute::Ptr loadAttribute(
            json::ArrayPtr attribute,
            sint64 element_count);
        void loadTopology(json::ObjectPtr topology_object, sint64 point_count);
        void loadPrimitive(
            json::ArrayPtr primitive,
            SharedPrimitiveData& shared_primitive_data);
        void loadVolumePrimitive(
            json::ObjectPtr volume,
            SharedPrimitiveData& shared_primitive_data);
        void loadPolyPrimitive(json::ObjectPtr polygon_object);
        void loadPolyPrimitiveRun(
            json::ObjectPtr definition,
            json::ArrayPtr run_entries);
        void loadPolygonRun(json::ObjectPtr polygon_run, bool closed = true);
        void loadGroups(
            json::ArrayPtr groups,
            sint64 element_count,
            std::map<std::string, std::vector<bool>>& destination);
        void loadVoxelData(
            json::ObjectPtr voxel_object,
            const math::V3i& resolution,
            std::span<float> voxel_data);

        std::vector<Primitive::Ptr> m_primitives;
        std::map<std::string, HouAttribute::Ptr> m_pointAttributes;
        std::map<std::string, HouAttribute::Ptr> m_vertexAttributes;
        std::map<std::string, HouAttribute::Ptr> m_primitiveAttributes;
        std::map<std::string, HouAttribute::Ptr> m_globalAttributes;
        std::map<std::string, std::vector<bool>> m_pointGroups;
        std::map<std::string, std::vector<bool>> m_vertexGroups;
        std::map<std::string, std::vector<bool>> m_primitiveGroups;
        HouTopology::Ptr m_topology;
        sint64 m_pointCount = -1;
    };
}
