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
option(HOUIO_BUILD_TOOLS "Build HouIO command-line tools" ${PROJECT_IS_TOP_LEVEL})
option(HOUIO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(HOUIO_ENABLE_ADDRESS_SANITIZER "Enable AddressSanitizer instrumentation" OFF)
option(HOUIO_ENABLE_UNDEFINED_SANITIZER "Enable UndefinedBehaviorSanitizer instrumentation" OFF)
option(HOUIO_BUILD_FUZZ_TARGETS "Build optional Clang libFuzzer targets" OFF)
set(HOUIO_HYTHON_EXECUTABLE "" CACHE FILEPATH "Optional hython executable for Houdini integration tests")

add_library(
    houio
    STATIC
    src/Attribute.cpp
    src/Field.cpp
    src/Geometry.cpp
    src/GeometryIO.cpp
    src/HouGeo.cpp
    src/HouGeoAdapter.cpp
    src/HouGeoIO.cpp
    src/json.cpp
    src/math/Color.cpp
    src/math/Math.cpp
    src/Scf.cpp
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
    if(CMAKE_DL_LIBS)
        target_link_libraries(houio PRIVATE ${CMAKE_DL_LIBS})
    endif()
    if(HOUIO_WARNINGS_AS_ERRORS)
        target_compile_options(houio PRIVATE -Werror)
    endif()
    if(HOUIO_ENABLE_UNDEFINED_SANITIZER)
        target_compile_options(houio PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        target_link_options(houio PUBLIC -fsanitize=undefined -fno-sanitize-recover=undefined)
    endif()
endif()

if(HOUIO_BUILD_TOOLS)
    add_executable(houio_convert tools/houio_convert.cpp)
    target_link_libraries(houio_convert PRIVATE houio::houio)
    target_compile_features(houio_convert PRIVATE cxx_std_20)
endif()

if(HOUIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HOUIO_BUILD_FUZZ_TARGETS)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "HOUIO_BUILD_FUZZ_TARGETS requires Clang with libFuzzer")
    endif()

    target_compile_options(houio PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)

    add_executable(houio_fuzz_parser tests/parser_corpus.cpp)
    target_link_libraries(houio_fuzz_parser PRIVATE houio::houio)
    target_compile_features(houio_fuzz_parser PRIVATE cxx_std_20)
    target_compile_options(houio_fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(houio_fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined)
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
install(
    DIRECTORY python/houio_hom
    DESTINATION ${CMAKE_INSTALL_DATADIR}/houio/python
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
)
install(
    FILES tools/houdini/install_hom_bridge.ps1
    DESTINATION ${CMAKE_INSTALL_DATADIR}/houio/houdini
)
if(TARGET houio_convert)
    install(TARGETS houio_convert RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

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
