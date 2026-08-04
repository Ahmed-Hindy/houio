if(NOT DEFINED HOUIO_SOURCE_DIR)
    message(FATAL_ERROR "HOUIO_SOURCE_DIR is required")
endif()

set(monolith "${HOUIO_SOURCE_DIR}/src/HouGeo.cpp")
set(attribute_loader "${HOUIO_SOURCE_DIR}/src/HouGeoAttributeLoad.cpp")
set(attribute_header "${HOUIO_SOURCE_DIR}/src/HouGeoAttributeLoad.h")
set(attribute_schema "${HOUIO_SOURCE_DIR}/src/HouGeoAttributeSchema.cpp")
set(packed_loader "${HOUIO_SOURCE_DIR}/src/HouGeoPackedLoad.cpp")
set(polygon_loader "${HOUIO_SOURCE_DIR}/src/HouGeoPolygonLoad.cpp")
set(primitive_loader "${HOUIO_SOURCE_DIR}/src/HouGeoPrimitiveLoad.cpp")
set(volume_loader "${HOUIO_SOURCE_DIR}/src/HouGeoVolumeLoad.cpp")
set(project_file "${HOUIO_SOURCE_DIR}/CMakeLists.txt")

foreach(required_file IN ITEMS
    "${monolith}"
    "${attribute_loader}"
    "${attribute_header}"
    "${attribute_schema}"
    "${packed_loader}"
    "${polygon_loader}"
    "${primitive_loader}"
    "${volume_loader}"
    "${project_file}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required HouGeo module file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${monolith}" monolith_source)
file(READ "${attribute_loader}" attribute_loader_source)
file(READ "${attribute_schema}" attribute_schema_source)
file(READ "${packed_loader}" packed_loader_source)
file(READ "${polygon_loader}" polygon_loader_source)
file(READ "${primitive_loader}" primitive_loader_source)
file(READ "${volume_loader}" volume_loader_source)
file(READ "${project_file}" project_source)

set(extracted_definitions
    "void HouGeo::loadNativeVdbPrimitive"
    "void HouGeo::loadSpherePrimitive"
    "void HouGeo::loadTubePrimitive"
    "void HouGeo::loadCurvePrimitive"
)

foreach(definition IN LISTS extracted_definitions)
    string(FIND "${primitive_loader_source}" "${definition}" loader_position)
    if(loader_position EQUAL -1)
        message(FATAL_ERROR "HouGeo primitive loader is missing: ${definition}")
    endif()

    string(FIND "${monolith_source}" "${definition}" monolith_position)
    if(NOT monolith_position EQUAL -1)
        message(FATAL_ERROR "Extracted loader returned to HouGeo.cpp: ${definition}")
    endif()
endforeach()

foreach(attribute_definition IN ITEMS
    "std::vector<int> expandPagedIntValues"
    "Attribute::ComponentType componentTypeForStorage"
    "void storeNumericComponent")
    string(FIND "${attribute_loader_source}" "${attribute_definition}" loader_position)
    if(loader_position EQUAL -1)
        message(FATAL_ERROR "HouGeo attribute payload module is missing: ${attribute_definition}")
    endif()

    string(FIND "${monolith_source}" "${attribute_definition}" monolith_position)
    if(NOT monolith_position EQUAL -1)
        message(FATAL_ERROR "Extracted attribute payload helper returned to HouGeo.cpp: ${attribute_definition}")
    endif()
endforeach()

foreach(attribute_schema_definition IN ITEMS
    "void HouGeo::loadGroups"
    "HouGeo::HouAttribute::Ptr HouGeo::loadAttribute")
    string(FIND "${attribute_schema_source}" "${attribute_schema_definition}" schema_position)
    if(schema_position EQUAL -1)
        message(FATAL_ERROR "HouGeo attribute schema module is missing: ${attribute_schema_definition}")
    endif()

    string(FIND "${monolith_source}" "${attribute_schema_definition}" monolith_position)
    if(NOT monolith_position EQUAL -1)
        message(FATAL_ERROR "Extracted attribute schema loader returned to HouGeo.cpp: ${attribute_schema_definition}")
    endif()
endforeach()

foreach(packed_definition IN ITEMS
    "void HouGeo::loadPackedGeometryPrimitive"
    "void HouGeo::loadPackedFragmentPrimitive"
    "void HouGeo::loadPackedDiskPrimitive"
    "void HouGeo::loadPackedDiskSequencePrimitive")
    string(FIND "${packed_loader_source}" "${packed_definition}" loader_position)
    if(loader_position EQUAL -1)
        message(FATAL_ERROR "HouGeo packed loader is missing: ${packed_definition}")
    endif()

    string(FIND "${monolith_source}" "${packed_definition}" monolith_position)
    if(NOT monolith_position EQUAL -1)
        message(FATAL_ERROR "Extracted packed loader returned to HouGeo.cpp: ${packed_definition}")
    endif()
endforeach()

foreach(polygon_definition IN ITEMS
    "void HouGeo::loadPolyPrimitive"
    "void HouGeo::loadPolyPrimitiveRun"
    "void HouGeo::loadPolygonRun")
    string(FIND "${polygon_loader_source}" "${polygon_definition}" loader_position)
    if(loader_position EQUAL -1)
        message(FATAL_ERROR "HouGeo polygon loader is missing: ${polygon_definition}")
    endif()

    string(FIND "${monolith_source}" "${polygon_definition}" monolith_position)
    if(NOT monolith_position EQUAL -1)
        message(FATAL_ERROR "Extracted polygon loader returned to HouGeo.cpp: ${polygon_definition}")
    endif()
endforeach()

foreach(volume_definition IN ITEMS
    "void HouGeo::loadVolumePrimitive"
    "std::vector<float> HouGeo::loadVoxelData")
    string(FIND "${volume_loader_source}" "${volume_definition}" loader_position)
    if(loader_position EQUAL -1)
        message(FATAL_ERROR "HouGeo volume loader is missing: ${volume_definition}")
    endif()

    string(FIND "${monolith_source}" "${volume_definition}" monolith_position)
    if(NOT monolith_position EQUAL -1)
        message(FATAL_ERROR "Extracted volume loader returned to HouGeo.cpp: ${volume_definition}")
    endif()
endforeach()

foreach(module IN ITEMS
    "src/HouGeoAttributeLoad.cpp"
    "src/HouGeoAttributeSchema.cpp"
    "src/HouGeoPackedLoad.cpp"
    "src/HouGeoPolygonLoad.cpp"
    "src/HouGeoPrimitiveLoad.cpp"
    "src/HouGeoVolumeLoad.cpp")
    string(FIND "${project_source}" "${module}" cmake_position)
    if(cmake_position EQUAL -1)
        message(FATAL_ERROR "${module} is not part of the houio target")
    endif()
endforeach()

message(STATUS "HouGeo schema-loader module boundaries are intact")
