cmake_minimum_required(VERSION 3.24)

project(
    houio
    VERSION 0.2.0
    DESCRIPTION "Standalone reader and writer for Houdini GEO and BGEO geometry files"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(HOUIO_BUILD_TESTS "Build HouIO tests" ${PROJECT_IS_TOP_LEVEL})
option(HOUIO_BUILD_EXAMPLES "Build HouIO examples" ${PROJECT_IS_TOP_LEVEL})
option(HOUIO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(HOUIO_ENABLE_ADDRESS_SANITIZER "Enable AddressSanitizer instrumentation" OFF)
set(HOUIO_HYTHON_EXECUTABLE "" CACHE FILEPATH "Optional hython executable for Houdini integration tests")

add_library(
    houio
    STATIC
    src/Attribute.cpp
    src/Field.cpp
    src/Geometry.cpp
    src/HouGeo.cpp
    src/HouGeoAdapter.cpp
    src/HouGeoIO.cpp
    src/json.cpp
    src/math/Color.cpp
    src/math/Math.cpp
)
add_library(houio::houio ALIAS houio)

target_compile_features(houio PUBLIC cxx_std_20)
target_include_directories(
    houio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(
    houio
    PROPERTIES
        CXX_EXTENSIONS OFF
        DEBUG_POSTFIX d
        EXPORT_NAME houio
)

if(MSVC)
    target_compile_options(houio PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    target_compile_definitions(houio PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(HOUIO_WARNINGS_AS_ERRORS)
        target_compile_options(houio PRIVATE /WX)
    endif()
    if(HOUIO_ENABLE_ADDRESS_SANITIZER)
        target_compile_options(houio PUBLIC /fsanitize=address)
        target_link_options(houio PUBLIC /fsanitize=address /INCREMENTAL:NO)
    endif()
else()
    target_compile_options(houio PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    if(HOUIO_WARNINGS_AS_ERRORS)
        target_compile_options(houio PRIVATE -Werror)
    endif()
endif()

if(HOUIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(
    TARGETS houio
    EXPORT HouIOTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    cmake/houio-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/houio-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/houio
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/houio-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    EXPORT HouIOTargets
    FILE houio-targets.cmake
    NAMESPACE houio::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/houio
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/houio-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/houio-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/houio
)
