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

            [[nodiscard]] std::string getName() const override;
            [[nodiscard]] Type getType() const override;
            [[nodiscard]] int getTupleSize() const override;
            [[nodiscard]] Storage getStorage() const override;
            void getPacking(std::vector<int>& packing) const override;
            [[nodiscard]] int getNumElements() const override;
            [[nodiscard]] std::string getString(int index) const override;
            [[nodiscard]] std::shared_ptr<json::Object> getDictionary(int index) const override;
            [[nodiscard]] RawPointer::Ptr getRawPointer() override;

            int addString(const std::string& value);

            void setName(std::string name)
            {
                m_name = std::move(name);
            }

            void setTupleSize(int tuple_size)
            {
                if (tuple_size <= 0)
                    throw std::invalid_argument("HouAttribute tuple size must be positive");
                tupleSize = tuple_size;
            }

            void setStorage(Storage storage) noexcept
            {
                m_storage = storage;
            }

            void setType(Type type) noexcept
            {
                m_type = type;
            }

            void setElementCount(int element_count)
            {
                if (element_count < 0)
                    throw std::invalid_argument("HouAttribute element count cannot be negative");
                numElements = element_count;
            }

            void setNumericAttribute(Attribute::Ptr attribute)
            {
                if (!attribute)
                    throw std::invalid_argument("HouAttribute numeric storage cannot be null");
                m_attr = std::move(attribute);
                tupleSize = m_attr->numComponents();
                numElements = m_attr->numElements();
                m_type = ATTR_TYPE_NUMERIC;
            }

            [[nodiscard]] Attribute::Ptr numericAttribute() noexcept
            {
                return m_attr;
            }

            [[nodiscard]] Attribute::CPtr numericAttribute() const noexcept
            {
                return m_attr;
            }

            void setStringValues(std::vector<std::string> values)
            {
                strings = std::move(values);
                dictionaries.clear();
                m_attr.reset();
                m_type = ATTR_TYPE_STRING;
                m_storage = ATTR_STORAGE_INT32;
                tupleSize = 1;
                numElements = static_cast<int>(strings.size());
            }

            [[nodiscard]] const std::vector<std::string>& stringValues() const noexcept
            {
                return strings;
            }

            void setDictionaryValues(std::vector<json::ObjectPtr> values)
            {
                dictionaries = std::move(values);
                strings.clear();
                m_attr.reset();
                m_type = ATTR_TYPE_DICT;
                m_storage = ATTR_STORAGE_INT32;
                tupleSize = 1;
                numElements = static_cast<int>(dictionaries.size());
            }

            [[nodiscard]] const std::vector<json::ObjectPtr>& dictionaryValues() const noexcept
            {
                return dictionaries;
            }

        private:
            std::string m_name;
            int tupleSize = 1;
            Storage m_storage = ATTR_STORAGE_INVALID;
            Type m_type = ATTR_TYPE_NUMERIC;
            std::vector<std::string> strings;
            std::vector<json::ObjectPtr> dictionaries;
            int numElements = 0;
            Attribute::Ptr m_attr;

            friend class HouGeo;
        };

        class HouTopology final : public Topology
        {
        public:
            using Ptr = std::shared_ptr<HouTopology>;
            using ConstPtr = std::shared_ptr<const HouTopology>;

            HouTopology();

            void getIndices(std::vector<int>& indices) const override;
            void addIndices(const std::vector<int>& indices) override;
            [[nodiscard]] sint64 getNumIndices() const override;

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

            [[nodiscard]] math::M44f getTransform() const override;
            [[nodiscard]] int getVertex() const override;
            [[nodiscard]] math::Vec3i getResolution() const override;
            [[nodiscard]] RawPointer::Ptr getRawPointer() override;
            [[nodiscard]] real32 getVoxel(int x, int y, int z) const override;
            [[nodiscard]] std::string getVisualizationMode() const override;
            [[nodiscard]] real32 getVisualizationIso() const override;
            [[nodiscard]] real32 getVisualizationDensity() const override;

            void setField(ScalarField::Ptr scalar_field)
            {
                if (!scalar_field)
                    throw std::invalid_argument("HouVolume field cannot be null");
                field = std::move(scalar_field);
            }

            [[nodiscard]] ScalarField::Ptr scalarField() noexcept
            {
                return field;
            }

            [[nodiscard]] std::shared_ptr<const ScalarField> scalarField() const noexcept
            {
                return field;
            }

            void setTopologyVertex(int topology_vertex) noexcept
            {
                vertex = topology_vertex;
            }

            void setVisualization(
                std::string mode,
                real32 iso_value,
                real32 density)
            {
                visualizationMode = mode.empty() ? "smoke" : std::move(mode);
                visualizationIso = iso_value;
                visualizationDensity = density;
            }

        private:
            ScalarField::Ptr field;
            int vertex = -1;
            std::string visualizationMode = "smoke";
            real32 visualizationIso = 0.0f;
            real32 visualizationDensity = 1.0f;

            friend class HouGeo;
        };

        class HouPoly final : public PolyPrimitive
        {
        public:
            using Ptr = std::shared_ptr<HouPoly>;
            using ConstPtr = std::shared_ptr<const HouPoly>;

            [[nodiscard]] int numPolys() const override;
            [[nodiscard]] int numVertices(int polygon_index) const override;
            [[nodiscard]] const int* vertices(int polygon_index = 0) const override;
            [[nodiscard]] bool closed() const override;

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

        [[nodiscard]] sint64 pointcount() const override;
        [[nodiscard]] sint64 vertexcount() const override;
        [[nodiscard]] sint64 primitivecount() const override;
        void getPointAttributeNames(std::vector<std::string>& names) const override;
        [[nodiscard]] AttributeAdapter::Ptr getPointAttribute(const std::string& name) override;
        void getVertexAttributeNames(std::vector<std::string>& names) const override;
        [[nodiscard]] AttributeAdapter::Ptr getVertexAttribute(const std::string& name) override;
        [[nodiscard]] bool hasPrimitiveAttribute(const std::string& name) const override;
        void getPrimitiveAttributeNames(std::vector<std::string>& names) const override;
        [[nodiscard]] AttributeAdapter::Ptr getPrimitiveAttribute(const std::string& name) override;
        void getPrimitives(std::vector<Primitive::Ptr>& primitives) override;
        void getGlobalAttributeNames(std::vector<std::string>& names) const override;
        [[nodiscard]] AttributeAdapter::Ptr getGlobalAttribute(const std::string& name) override;
        void getPointGroupNames(std::vector<std::string>& names) const override;
        [[nodiscard]] bool getPointGroupMembership(
            const std::string& name,
            std::vector<bool>& membership) const override;
        void getVertexGroupNames(std::vector<std::string>& names) const override;
        [[nodiscard]] bool getVertexGroupMembership(
            const std::string& name,
            std::vector<bool>& membership) const override;
        void getPrimitiveGroupNames(std::vector<std::string>& names) const override;
        [[nodiscard]] bool getPrimitiveGroupMembership(
            const std::string& name,
            std::vector<bool>& membership) const override;
        [[nodiscard]] Topology::Ptr getTopology() override;

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
            float* voxel_data);

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
