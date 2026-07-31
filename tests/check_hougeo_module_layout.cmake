if(NOT DEFINED HOUIO_SOURCE_DIR)
    message(FATAL_ERROR "HOUIO_SOURCE_DIR is required")
endif()

set(monolith "${HOUIO_SOURCE_DIR}/src/HouGeo.cpp")
set(primitive_loader "${HOUIO_SOURCE_DIR}/src/HouGeoPrimitiveLoad.cpp")
set(project_file "${HOUIO_SOURCE_DIR}/CMakeLists.txt")

foreach(required_file IN ITEMS "${monolith}" "${primitive_loader}" "${project_file}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required HouGeo module file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${monolith}" monolith_source)
file(READ "${primitive_loader}" primitive_loader_source)
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

string(FIND "${project_source}" "src/HouGeoPrimitiveLoad.cpp" cmake_position)
if(cmake_position EQUAL -1)
    message(FATAL_ERROR "HouGeoPrimitiveLoad.cpp is not part of the houio target")
endif()

message(STATUS "HouGeo primitive-loader module boundary is intact")
