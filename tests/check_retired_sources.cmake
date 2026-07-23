if(NOT DEFINED HOUIO_SOURCE_DIR OR HOUIO_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "HOUIO_SOURCE_DIR is required")
endif()

set(ttl_root "${HOUIO_SOURCE_DIR}/include/ttl")
set(half_root "${HOUIO_SOURCE_DIR}/include/houio/math/Half")
set(half_source_root "${HOUIO_SOURCE_DIR}/src/math/Half")

file(GLOB_RECURSE ttl_files LIST_DIRECTORIES FALSE "${ttl_root}/*")
foreach(file_path IN LISTS ttl_files)
    get_filename_component(file_name "${file_path}" NAME)
    if(file_name STREQUAL "retired.hpp")
        continue()
    endif()
    file(READ "${file_path}" file_content)
    string(FIND "${file_content}" "#include <ttl/retired.hpp>" marker_position)
    if(marker_position EQUAL -1)
        message(FATAL_ERROR "Retired TTL file contains implementation code: ${file_path}")
    endif()
endforeach()

file(GLOB_RECURSE half_files LIST_DIRECTORIES FALSE
    "${half_root}/*"
    "${half_source_root}/*"
)
foreach(file_path IN LISTS half_files)
    get_filename_component(file_name "${file_path}" NAME)
    if(file_name STREQUAL "retired.h")
        continue()
    endif()
    file(READ "${file_path}" file_content)
    string(FIND "${file_content}" "houio/math/Half/retired.h" marker_position)
    if(marker_position EQUAL -1)
        message(FATAL_ERROR "Retired half file contains implementation code: ${file_path}")
    endif()
endforeach()

file(GLOB_RECURSE active_sources LIST_DIRECTORIES FALSE
    "${HOUIO_SOURCE_DIR}/include/houio/*.h"
    "${HOUIO_SOURCE_DIR}/include/houio/*.hpp"
    "${HOUIO_SOURCE_DIR}/src/*.cpp"
    "${HOUIO_SOURCE_DIR}/tests/*.cpp"
    "${HOUIO_SOURCE_DIR}/tools/*.cpp"
)
foreach(file_path IN LISTS active_sources)
    if(file_path MATCHES "/math/Half/" OR file_path MATCHES "/src/math/Half/")
        continue()
    endif()
    file(READ "${file_path}" file_content)
    string(FIND "${file_content}" "<ttl/" ttl_reference)
    string(FIND "${file_content}" "<houio/math/Half/" half_reference)
    if(NOT ttl_reference EQUAL -1)
        message(FATAL_ERROR "Active source references retired TTL code: ${file_path}")
    endif()
    if(NOT half_reference EQUAL -1)
        message(FATAL_ERROR "Active source references retired half code: ${file_path}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/CMakeLists.txt" root_cmake)
string(FIND "${root_cmake}" "src/math/Half/half.cpp" compiled_half_reference)
if(NOT compiled_half_reference EQUAL -1)
    message(FATAL_ERROR "The retired half implementation is still part of the build")
endif()
