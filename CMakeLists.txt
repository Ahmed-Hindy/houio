cmake_minimum_required(VERSION 3.24)

project(
    houio
    VERSION 0.2.0
    DESCRIPTION "Standalone Houdini geometry, Alembic, and USD I/O library and tools"
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
option(HOUIO_ENABLE_MSVC_ANALYSIS "Enable the MSVC native static analyzer" OFF)
option(HOUIO_BUILD_FUZZ_TARGETS "Build optional Clang libFuzzer targets" OFF)
option(HOUIO_BUILD_BENCHMARKS "Build opt-in HouIO performance baselines" OFF)
option(HOUIO_ENABLE_OPENVDB "Enable the optional OpenVDB FloatGrid backend" OFF)
option(HOUIO_BUILD_HOUDINI_PLUGIN "Build the native Houdini ROP plugin" OFF)
set(HOUIO_HOUDINI_ROOT "" CACHE PATH "Houdini installation root used for the native HDK plugin")
set(HOUIO_HYTHON_EXECUTABLE "" CACHE FILEPATH "Optional hython executable for Houdini integration tests")

include(cmake/houio-scene-io.cmake)

add_library(
    houio
    STATIC
    src/Attribute.cpp
    src/Field.cpp
    src/Geometry.cpp
    src/GeometryIO.cpp
    src/HomManifest.cpp
    src/HomManifestAttributes.cpp
    src/HomManifestPrimitives.cpp
    src/HomManifestVdb.cpp
    src/HouGeo.cpp
    src/HouGeoAttributeLoad.cpp
    src/HouGeoAttributeSchema.cpp
    src/HouGeoPrimitiveLoad.cpp
    src/HouGeoAdapter.cpp
    src/HouGeoAdapt.cpp
    src/HouGeoIO.cpp
    src/json.cpp
    src/NativeVdbPayload.cpp
    src/NativePolygonWriter.cpp
    src/NativeSceneWriter.cpp
    src/OpenVdbBackend.cpp
    src/SceneGeometryAdapter.cpp
    src/SparseGrid.cpp
    src/math/Color.cpp
    src/math/Math.cpp
    src/Scf.cpp
    src/Writer.cpp
)
add_library(houio::houio ALIAS houio)

if(HOUIO_HAS_ALEMBIC_VALUE OR HOUIO_HAS_USD_VALUE)
    add_library(houio_scene_backend OBJECT src/SceneArchive.cpp)
    if(HOUIO_SCENE_BACKEND_COMPILE_TARGETS)
        target_link_libraries(
            houio_scene_backend
            PRIVATE
                ${HOUIO_SCENE_BACKEND_COMPILE_TARGETS}
        )
    endif()
    target_include_directories(
        houio_scene_backend
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${HOUIO_SCENE_IO_INCLUDE_DIRECTORIES}
    )
    target_compile_definitions(
        houio_scene_backend
        PRIVATE
            HOUIO_HAS_ALEMBIC=1
            HOUIO_HAS_USD=1
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
    if(NOT HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "houdini")
        target_compile_features(houio_scene_backend PRIVATE cxx_std_20)
    endif()
    set_target_properties(
        houio_scene_backend
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
    )
    if(MSVC)
        target_compile_options(
            houio_scene_backend
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /utf-8
                /wd4251
                /wd4275
                /wd4702
        )
        if(HOUIO_WARNINGS_AS_ERRORS)
            target_compile_options(houio_scene_backend PRIVATE /WX)
        endif()
    endif()
    target_sources(houio PRIVATE $<TARGET_OBJECTS:houio_scene_backend>)
else()
    target_sources(houio PRIVATE src/SceneArchive.cpp)
endif()

target_compile_features(houio PUBLIC cxx_std_20)
target_include_directories(
    houio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${HOUIO_SCENE_IO_INCLUDE_DIRECTORIES}
)
target_compile_definitions(
    houio
    PUBLIC
        HOUIO_HAS_ALEMBIC=${HOUIO_HAS_ALEMBIC_VALUE}
        HOUIO_HAS_USD=${HOUIO_HAS_USD_VALUE}
)
if(HOUIO_SCENE_IO_LIBRARIES)
    target_link_libraries(houio PRIVATE ${HOUIO_SCENE_IO_LIBRARIES})
endif()

if(HOUIO_ENABLE_OPENVDB)
    find_package(OpenVDB QUIET)
    if(NOT OpenVDB_FOUND)
        message(FATAL_ERROR
            "HOUIO_ENABLE_OPENVDB=ON requires an OpenVDB development package. "
            "Add the installed OpenVDB CMake module directory to CMAKE_MODULE_PATH, "
            "or provide a package through OpenVDB_DIR/CMAKE_PREFIX_PATH.")
    endif()
    if(NOT TARGET OpenVDB::openvdb)
        message(FATAL_ERROR "OpenVDB was found but did not provide the OpenVDB::openvdb target")
    endif()
    target_link_libraries(houio PUBLIC OpenVDB::openvdb)
    target_compile_definitions(houio PUBLIC HOUIO_HAS_OPENVDB=1)
    set(HOUIO_CONFIG_FIND_OPENVDB "include(CMakeFindDependencyMacro)\nfind_dependency(OpenVDB)")
else()
    target_compile_definitions(houio PUBLIC HOUIO_HAS_OPENVDB=0)
    set(HOUIO_CONFIG_FIND_OPENVDB "")
endif()

set_target_properties(
    houio
    PROPERTIES
        CXX_EXTENSIONS OFF
        DEBUG_POSTFIX d
        EXPORT_NAME houio
        POSITION_INDEPENDENT_CODE ON
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
    if(HOUIO_ENABLE_MSVC_ANALYSIS)
        target_compile_options(houio PRIVATE /analyze /analyze:external-)
    endif()
else()
    if(HOUIO_ENABLE_MSVC_ANALYSIS)
        message(FATAL_ERROR "HOUIO_ENABLE_MSVC_ANALYSIS requires the MSVC compiler")
    endif()
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
    add_executable(houio_cli tools/houio.cpp)
    set_target_properties(houio_cli PROPERTIES OUTPUT_NAME houio)
    target_link_libraries(houio_cli PRIVATE houio::houio)
    target_compile_features(houio_cli PRIVATE cxx_std_20)
    target_compile_definitions(houio_cli PRIVATE HOUIO_VERSION_STRING="${PROJECT_VERSION}")

    if(WIN32)
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            set(
                HOUIO_HOUDINI_PACKAGE_ARCHIVE
                "${CMAKE_CURRENT_BINARY_DIR}/houio-houdini-package-${PROJECT_VERSION}-windows-x86_64.zip"
            )
            file(
                GLOB_RECURSE HOUIO_HOUDINI_PACKAGE_SOURCES
                CONFIGURE_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/houdini/package/*"
                "${CMAKE_CURRENT_SOURCE_DIR}/python/houio_hom/*"
            )
            set(HOUIO_HOUDINI_PACKAGE_RUNTIME_ARGS)
            set(HOUIO_HOUDINI_PACKAGE_RUNTIME_SOURCES)
            if(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "bundled")
                list(APPEND HOUIO_HOUDINI_PACKAGE_RUNTIME_ARGS
                    --runtime-root "${HOUIO_BUNDLED_SCENE_IO_ROOT}")
                file(
                    GLOB_RECURSE HOUIO_HOUDINI_PACKAGE_RUNTIME_SOURCES
                    CONFIGURE_DEPENDS
                    "${HOUIO_BUNDLED_SCENE_IO_ROOT}/bin/*"
                    "${HOUIO_BUNDLED_SCENE_IO_ROOT}/lib/*"
                    "${HOUIO_BUNDLED_SCENE_IO_ROOT}/plugin/*"
                    "${HOUIO_BUNDLED_SCENE_IO_ROOT}/share/*"
                )
            endif()
            add_custom_command(
                OUTPUT "${HOUIO_HOUDINI_PACKAGE_ARCHIVE}"
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/houdini/build_houdini_package.py"
                    --cli "$<TARGET_FILE:houio_cli>"
                    --output "${HOUIO_HOUDINI_PACKAGE_ARCHIVE}"
                    --version "${PROJECT_VERSION}"
                    ${HOUIO_HOUDINI_PACKAGE_RUNTIME_ARGS}
                DEPENDS
                    houio_cli
                    ${HOUIO_HOUDINI_PACKAGE_SOURCES}
                    ${HOUIO_HOUDINI_PACKAGE_RUNTIME_SOURCES}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/houdini/build_houdini_package.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/houdini/bootstrap_houdini_package.ps1"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/houdini/install_houdini_package.ps1"
                VERBATIM
            )
            add_custom_target(
                houio_houdini_package
                DEPENDS "${HOUIO_HOUDINI_PACKAGE_ARCHIVE}"
            )
        endif()
    endif()
endif()

if(HOUIO_BUILD_HOUDINI_PLUGIN)
    add_subdirectory(houdini/hdk)
endif()

if(HOUIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HOUIO_BUILD_BENCHMARKS)
    add_executable(houio_benchmarks benchmarks/houio_benchmarks.cpp)
    add_executable(houio_memory_probe benchmarks/houio_memory_probe.cpp)
    foreach(benchmark_target IN ITEMS houio_benchmarks houio_memory_probe)
        target_link_libraries(${benchmark_target} PRIVATE houio::houio)
        target_compile_features(${benchmark_target} PRIVATE cxx_std_20)
        set_target_properties(${benchmark_target} PROPERTIES CXX_EXTENSIONS OFF)
        if(MSVC)
            target_compile_options(${benchmark_target} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
            target_compile_definitions(${benchmark_target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
            if(HOUIO_WARNINGS_AS_ERRORS)
                target_compile_options(${benchmark_target} PRIVATE /WX)
            endif()
        else()
            target_compile_options(${benchmark_target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
            if(HOUIO_WARNINGS_AS_ERRORS)
                target_compile_options(${benchmark_target} PRIVATE -Werror)
            endif()
        endif()
    endforeach()
    if(WIN32)
        target_link_libraries(houio_memory_probe PRIVATE Psapi)
    endif()
endif()

if(HOUIO_BUILD_FUZZ_TARGETS)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "HOUIO_BUILD_FUZZ_TARGETS requires Clang with libFuzzer")
    endif()

    target_compile_options(houio PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)

    add_executable(houio_fuzz_parser tests/parser_corpus.cpp)
    target_link_libraries(houio_fuzz_parser PRIVATE houio::houio)
    target_compile_features(houio_fuzz_parser PRIVATE cxx_std_20)
    target_compile_options(houio_fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(houio_fuzz_parser PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined)
endif()

install(
    TARGETS houio
    EXPORT HouIOTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    DIRECTORY include/houio
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
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
install(
    FILES
        LICENSE_STATUS.md
        THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/houio/legal
)
install(
    FILES docs/provenance.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/houio/legal/docs
)

if(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "bundled")
    foreach(dependency_directory IN ITEMS bin lib include plugin share cmake)
        if(EXISTS "${HOUIO_BUNDLED_SCENE_IO_ROOT}/${dependency_directory}")
            install(
                DIRECTORY "${HOUIO_BUNDLED_SCENE_IO_ROOT}/${dependency_directory}/"
                DESTINATION "${dependency_directory}"
            )
        endif()
    endforeach()
    if(EXISTS "${HOUIO_BUNDLED_SCENE_IO_ROOT}/pxrConfig.cmake")
        install(
            FILES "${HOUIO_BUNDLED_SCENE_IO_ROOT}/pxrConfig.cmake"
            DESTINATION .
        )
    endif()
    if(EXISTS "${HOUIO_BUNDLED_SCENE_IO_ROOT}/houio-scene-dependencies.json")
        install(
            FILES "${HOUIO_BUNDLED_SCENE_IO_ROOT}/houio-scene-dependencies.json"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/houio/licenses
        )
    endif()
endif()
if(TARGET houio_cli)
    install(TARGETS houio_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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

if(PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_NAME "houio")
    set(CPACK_PACKAGE_VENDOR "HouIO")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_CHECKSUM "SHA256")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    if(WIN32)
        set(CPACK_PACKAGE_FILE_NAME
            "houio-${PROJECT_VERSION}-windows-x86_64")
    else()
        set(CPACK_PACKAGE_FILE_NAME
            "houio-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    include(CPack)

    set(HOUIO_PORTABLE_PACKAGE_DEPENDENCIES houio)
    if(TARGET houio_cli)
        list(APPEND HOUIO_PORTABLE_PACKAGE_DEPENDENCIES houio_cli)
    endif()
    add_custom_target(
        houio_portable_package
        COMMAND
            "${CMAKE_CPACK_COMMAND}"
            -G ZIP
            --config "${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake"
        DEPENDS ${HOUIO_PORTABLE_PACKAGE_DEPENDENCIES}
        USES_TERMINAL
        VERBATIM
    )
endif()
