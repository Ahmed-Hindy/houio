if(NOT DEFINED HOUIO_SOURCE_DIR)
    message(FATAL_ERROR "HOUIO_SOURCE_DIR is required")
endif()

set(dispatch_source "${HOUIO_SOURCE_DIR}/src/HomManifest.cpp")
set(attribute_source "${HOUIO_SOURCE_DIR}/src/HomManifestAttributes.cpp")
set(primitive_source "${HOUIO_SOURCE_DIR}/src/HomManifestPrimitives.cpp")
set(vdb_source "${HOUIO_SOURCE_DIR}/src/HomManifestVdb.cpp")
set(internal_header "${HOUIO_SOURCE_DIR}/src/HomManifestInternal.h")
set(project_file "${HOUIO_SOURCE_DIR}/CMakeLists.txt")

foreach(required_file IN ITEMS
    "${dispatch_source}"
    "${attribute_source}"
    "${primitive_source}"
    "${vdb_source}"
    "${internal_header}"
    "${project_file}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required HOM manifest module file is missing: ${required_file}")
    endif()
endforeach()

file(READ "${dispatch_source}" dispatch_text)
file(READ "${attribute_source}" attribute_text)
file(READ "${primitive_source}" primitive_text)
file(READ "${vdb_source}" vdb_text)
file(READ "${project_file}" project_text)

string(FIND "${attribute_text}" "HouGeo::HouAttribute::Ptr parseAttribute" attribute_definition)
if(attribute_definition EQUAL -1)
    message(FATAL_ERROR "HOM attribute module is missing parseAttribute")
endif()
string(FIND "${attribute_text}" "void parseAttributeDomain" domain_definition)
if(domain_definition EQUAL -1)
    message(FATAL_ERROR "HOM attribute module is missing parseAttributeDomain")
endif()
string(FIND "${attribute_text}" "void parseGroups" group_definition)
if(group_definition EQUAL -1)
    message(FATAL_ERROR "HOM attribute module is missing parseGroups")
endif()
string(FIND "${primitive_text}" "void parsePrimitives" primitive_definition)
if(primitive_definition EQUAL -1)
    message(FATAL_ERROR "HOM primitive module is missing parsePrimitives")
endif()
string(FIND "${vdb_text}" "bool parseSparseVdbPrimitive" vdb_definition)
if(vdb_definition EQUAL -1)
    message(FATAL_ERROR "HOM VDB module is missing parseSparseVdbPrimitive")
endif()

set(extracted_definitions
    "HouGeo::HouAttribute::Ptr parseAttribute"
    "void parseAttributeDomain"
    "void parseGroups"
    "void parsePrimitives"
    "bool parseSparseVdbPrimitive"
)
foreach(definition IN LISTS extracted_definitions)
    string(FIND "${dispatch_text}" "${definition}" dispatch_position)
    if(NOT dispatch_position EQUAL -1)
        message(FATAL_ERROR "Extracted HOM manifest decoder returned to HomManifest.cpp: ${definition}")
    endif()
endforeach()

foreach(module IN ITEMS
    "src/HomManifestAttributes.cpp"
    "src/HomManifestPrimitives.cpp"
    "src/HomManifestVdb.cpp")
    string(FIND "${project_text}" "${module}" cmake_position)
    if(cmake_position EQUAL -1)
        message(FATAL_ERROR "${module} is not part of the houio target")
    endif()
endforeach()

message(STATUS "HOM manifest module boundary is intact")
