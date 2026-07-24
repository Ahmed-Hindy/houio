find_package(Threads REQUIRED)

add_executable(houio_test_logger main.cpp)
target_link_libraries(houio_test_logger PRIVATE houio::houio)
target_compile_features(houio_test_logger PRIVATE cxx_std_20)
add_test(NAME houio.logger COMMAND houio_test_logger)

add_executable(houio_test_binary_json test_binary_json.cpp)
target_link_libraries(houio_test_binary_json PRIVATE houio::houio)
target_compile_features(houio_test_binary_json PRIVATE cxx_std_20)
add_test(NAME houio.binary_json COMMAND houio_test_binary_json)

add_executable(houio_test_parser_corpus parser_corpus.cpp)
target_link_libraries(houio_test_parser_corpus PRIVATE houio::houio)
target_compile_features(houio_test_parser_corpus PRIVATE cxx_std_20)
target_compile_definitions(houio_test_parser_corpus PRIVATE HOUIO_STANDALONE_CORPUS=1)
add_test(NAME houio.parser_corpus COMMAND houio_test_parser_corpus)

add_test(
    NAME houio.retired_sources
    COMMAND
        "${CMAKE_COMMAND}"
        "-DHOUIO_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_retired_sources.cmake"
)

add_executable(houio_test_attribute test_attribute.cpp)
target_link_libraries(houio_test_attribute PRIVATE houio::houio)
target_compile_features(houio_test_attribute PRIVATE cxx_std_20)
add_test(NAME houio.attribute COMMAND houio_test_attribute)

add_executable(houio_test_math test_math.cpp)
target_link_libraries(houio_test_math PRIVATE houio::houio)
target_compile_features(houio_test_math PRIVATE cxx_std_20)
add_test(NAME houio.math COMMAND houio_test_math)

add_executable(houio_test_modern_geometry test_modern_geometry.cpp)
target_link_libraries(houio_test_modern_geometry PRIVATE houio::houio)
target_compile_features(houio_test_modern_geometry PRIVATE cxx_std_20)
add_test(NAME houio.modern_geometry COMMAND houio_test_modern_geometry)

add_executable(houio_test_geometry_container test_geometry_container.cpp)
target_link_libraries(houio_test_geometry_container PRIVATE houio::houio)
target_compile_features(houio_test_geometry_container PRIVATE cxx_std_20)
add_test(NAME houio.geometry_container COMMAND houio_test_geometry_container)

add_executable(houio_test_polygon_runs test_polygon_runs.cpp)
target_link_libraries(houio_test_polygon_runs PRIVATE houio::houio)
target_compile_features(houio_test_polygon_runs PRIVATE cxx_std_20)
add_test(NAME houio.polygon_runs COMMAND houio_test_polygon_runs)

add_executable(houio_test_groups test_groups.cpp)
target_link_libraries(houio_test_groups PRIVATE houio::houio)
target_compile_features(houio_test_groups PRIVATE cxx_std_20)
add_test(NAME houio.groups COMMAND houio_test_groups)

add_executable(houio_test_malformed_geometry test_malformed_geometry.cpp)
target_link_libraries(houio_test_malformed_geometry PRIVATE houio::houio)
target_compile_features(houio_test_malformed_geometry PRIVATE cxx_std_20)
add_test(NAME houio.malformed_geometry COMMAND houio_test_malformed_geometry)

add_executable(houio_test_volumes test_volumes.cpp)
target_link_libraries(houio_test_volumes PRIVATE houio::houio)
target_compile_features(houio_test_volumes PRIVATE cxx_std_20)
add_test(NAME houio.volumes COMMAND houio_test_volumes)

add_executable(houio_test_conversion_safety test_conversion_safety.cpp)
target_link_libraries(houio_test_conversion_safety PRIVATE houio::houio)
target_compile_features(houio_test_conversion_safety PRIVATE cxx_std_20)
add_test(NAME houio.conversion_safety COMMAND houio_test_conversion_safety)

add_executable(houio_test_numeric_storage test_numeric_storage.cpp)
target_link_libraries(houio_test_numeric_storage PRIVATE houio::houio)
target_compile_features(houio_test_numeric_storage PRIVATE cxx_std_20)
add_test(NAME houio.numeric_storage COMMAND houio_test_numeric_storage)

add_executable(houio_test_attribute_pages test_attribute_pages.cpp)
target_link_libraries(houio_test_attribute_pages PRIVATE houio::houio)
target_compile_features(houio_test_attribute_pages PRIVATE cxx_std_20)
add_test(NAME houio.attribute_pages COMMAND houio_test_attribute_pages)

add_executable(houio_test_export_safety test_export_safety.cpp)
target_link_libraries(houio_test_export_safety PRIVATE houio::houio Threads::Threads)
target_compile_features(houio_test_export_safety PRIVATE cxx_std_20)
add_test(NAME houio.export_safety COMMAND houio_test_export_safety)

add_executable(houio_test_geometry_io test_geometry_io.cpp)
target_link_libraries(houio_test_geometry_io PRIVATE houio::houio)
target_compile_features(houio_test_geometry_io PRIVATE cxx_std_20)
add_test(NAME houio.geometry_io COMMAND houio_test_geometry_io)

add_test(
    NAME houio.package_consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DHOUIO_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DHOUIO_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DHOUIO_GENERATOR=${CMAKE_GENERATOR}"
        "-DHOUIO_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DHOUIO_BUILD_CONFIG=$<CONFIG>"
        "-DHOUIO_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DHOUIO_EXPECT_TOOLS=${HOUIO_BUILD_TOOLS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_package_consumer.cmake"
)

if(WIN32 AND TARGET houio_houdini_package)
    add_test(
        NAME houio.houdini_package.bootstrap
        COMMAND
            "${CMAKE_COMMAND}"
            "-DHOUIO_BINARY_DIR=${PROJECT_BINARY_DIR}"
            "-DHOUIO_BUILD_CONFIG=$<CONFIG>"
            "-DHOUIO_HOUDINI_PACKAGE_ARCHIVE=${HOUIO_HOUDINI_PACKAGE_ARCHIVE}"
            "-DHOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR=${CMAKE_CURRENT_BINARY_DIR}/houdini_package_bootstrap"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_houdini_bootstrap_test.cmake"
    )
endif()

add_executable(houio_roundtrip_geometry roundtrip_geometry.cpp)
target_link_libraries(houio_roundtrip_geometry PRIVATE houio::houio)
target_compile_features(houio_roundtrip_geometry PRIVATE cxx_std_20)

add_executable(houio_inspect_json inspect_json.cpp)
target_link_libraries(houio_inspect_json PRIVATE houio::houio)
target_compile_features(houio_inspect_json PRIVATE cxx_std_20)

add_executable(houio_inspect_attributes inspect_attributes.cpp)
target_link_libraries(houio_inspect_attributes PRIVATE houio::houio)
target_compile_features(houio_inspect_attributes PRIVATE cxx_std_20)

if(HOUIO_BUILD_EXAMPLES)
    add_executable(houio_example_readwrite example_readwrite.cpp)
    target_link_libraries(houio_example_readwrite PRIVATE houio::houio)
    target_compile_features(houio_example_readwrite PRIVATE cxx_std_20)
endif()

if(HOUIO_HYTHON_EXECUTABLE)
    if(NOT EXISTS "${HOUIO_HYTHON_EXECUTABLE}")
        message(FATAL_ERROR "HOUIO_HYTHON_EXECUTABLE does not exist: ${HOUIO_HYTHON_EXECUTABLE}")
    endif()

    get_filename_component(HOUIO_HYTHON_BIN_DIR "${HOUIO_HYTHON_EXECUTABLE}" DIRECTORY)
    if(WIN32)
        set(HOUIO_HYTHON_BLOSC_LIBRARY "${HOUIO_HYTHON_BIN_DIR}/blosc.dll")
    elseif(APPLE)
        set(HOUIO_HYTHON_BLOSC_LIBRARY "${HOUIO_HYTHON_BIN_DIR}/../Frameworks/libblosc.dylib")
    else()
        set(HOUIO_HYTHON_BLOSC_LIBRARY "libblosc.so.1")
    endif()

    set_tests_properties(
        houio.geometry_io
        PROPERTIES ENVIRONMENT "HOUIO_BLOSC_LIBRARY=${HOUIO_HYTHON_BLOSC_LIBRARY}"
    )

    if(TARGET houio_convert)
        add_test(
            NAME houio.hom_bridge
            COMMAND
                "${HOUIO_HYTHON_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tools/houdini/test_hom_bridge.py"
                "${CMAKE_CURRENT_BINARY_DIR}/hom_bridge"
        )
        set_tests_properties(
            houio.hom_bridge
            PROPERTIES
                ENVIRONMENT "HOUIO_CONVERT_EXECUTABLE=$<TARGET_FILE:houio_convert>;HOUIO_BLOSC_LIBRARY=${HOUIO_HYTHON_BLOSC_LIBRARY}"
                ENVIRONMENT_MODIFICATION "PYTHONPATH=path_list_prepend:${PROJECT_SOURCE_DIR}/python"
        )
    endif()

    if(TARGET houio_houdini_package)
        add_test(
            NAME houio.houdini_package.build
            COMMAND
                "${CMAKE_COMMAND}"
                --build "${PROJECT_BINARY_DIR}"
                --target houio_houdini_package
        )
        set_tests_properties(
            houio.houdini_package.build
            PROPERTIES FIXTURES_SETUP houio_houdini_package_archive
        )

        add_test(
            NAME houio.houdini_package.validate
            COMMAND
                "${CMAKE_COMMAND}"
                "-DHOUIO_HOUDINI_PACKAGE_ARCHIVE=${HOUIO_HOUDINI_PACKAGE_ARCHIVE}"
                "-DHOUIO_HOUDINI_PACKAGE_EXTRACT_DIR=${CMAKE_CURRENT_BINARY_DIR}/houdini_package"
                "-DHOUIO_HYTHON_EXECUTABLE=${HOUIO_HYTHON_EXECUTABLE}"
                "-DHOUIO_HOUDINI_PACKAGE_TEST_SCRIPT=${PROJECT_SOURCE_DIR}/tools/houdini/test_houdini_package.py"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_houdini_package_test.cmake"
        )
        set_tests_properties(
            houio.houdini_package.validate
            PROPERTIES FIXTURES_REQUIRED houio_houdini_package_archive
        )
    endif()

    set(HOUIO_FIXTURE_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
    set(HOUIO_FIXTURE_SOURCE_DIR "${HOUIO_FIXTURE_TEST_DIR}/source")
    set(HOUIO_FIXTURE_OUTPUT_DIR "${HOUIO_FIXTURE_TEST_DIR}/output")
    set(HOUIO_FIXTURE_MANIFEST "${HOUIO_FIXTURE_SOURCE_DIR}/manifest.json")
    set(
        HOUIO_FIXTURE_NAMES
        empty
        point_attributes
        numeric_precision
        paged_attributes
        triangles
        quads
        mixed_polygons
        ngon
        open_polygon
        uv_seam
        multiple_polygon_runs
        global_attributes
        dense_volume
        primitive_groups
    )

    add_test(
        NAME houio.fixtures.generate
        COMMAND
            "${HOUIO_HYTHON_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/houdini/generate_fixture_suite.py"
            "${HOUIO_FIXTURE_SOURCE_DIR}"
    )
    set_tests_properties(houio.fixtures.generate PROPERTIES FIXTURES_SETUP houio_fixture_sources)

    set(HOUIO_FIXTURE_ROUNDTRIP_TESTS)
    foreach(HOUIO_FIXTURE_NAME IN LISTS HOUIO_FIXTURE_NAMES)
        set(HOUIO_FIXTURE_SOURCE "${HOUIO_FIXTURE_SOURCE_DIR}/${HOUIO_FIXTURE_NAME}.bgeo")
        set(HOUIO_FIXTURE_OUTPUT "${HOUIO_FIXTURE_OUTPUT_DIR}/${HOUIO_FIXTURE_NAME}.bgeo")
        set(HOUIO_FIXTURE_TEST_NAME "houio.fixtures.${HOUIO_FIXTURE_NAME}.roundtrip")
        add_test(
            NAME "${HOUIO_FIXTURE_TEST_NAME}"
            COMMAND $<TARGET_FILE:houio_roundtrip_geometry> "${HOUIO_FIXTURE_SOURCE}" "${HOUIO_FIXTURE_OUTPUT}"
        )
        set_tests_properties(
            "${HOUIO_FIXTURE_TEST_NAME}"
            PROPERTIES FIXTURES_REQUIRED houio_fixture_sources
        )
        list(APPEND HOUIO_FIXTURE_ROUNDTRIP_TESTS "${HOUIO_FIXTURE_TEST_NAME}")
    endforeach()

    add_test(
        NAME houio.fixtures.triangles.scf_roundtrip
        COMMAND
            $<TARGET_FILE:houio_roundtrip_geometry>
            "${HOUIO_FIXTURE_SOURCE_DIR}/triangles.bgeo"
            "${HOUIO_FIXTURE_OUTPUT_DIR}/triangles.bgeo.sc"
    )
    set_tests_properties(
        houio.fixtures.triangles.scf_roundtrip
        PROPERTIES
            FIXTURES_REQUIRED houio_fixture_sources
            FIXTURES_SETUP houio_triangle_scf_output
            ENVIRONMENT "HOUIO_BLOSC_LIBRARY=${HOUIO_HYTHON_BLOSC_LIBRARY}"
    )

    add_test(
        NAME houio.fixtures.triangles.scf_validate
        COMMAND
            "${HOUIO_HYTHON_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/houdini/compare_geometry.py"
            "${HOUIO_FIXTURE_SOURCE_DIR}/triangles.bgeo"
            "${HOUIO_FIXTURE_OUTPUT_DIR}/triangles.bgeo.sc"
            --point-attribute P
            --topology
    )
    set_tests_properties(
        houio.fixtures.triangles.scf_validate
        PROPERTIES
            FIXTURES_REQUIRED "houio_fixture_sources;houio_triangle_scf_output"
    )

    if(TARGET houio_convert)
        add_test(
            NAME houio.fixtures.triangles.houdini_scf_generate
            COMMAND
                "${HOUIO_HYTHON_EXECUTABLE}"
                -m houio_hom encode
                "${HOUIO_FIXTURE_SOURCE_DIR}/triangles.bgeo"
                "${HOUIO_FIXTURE_SOURCE_DIR}/triangles_houdini.bgeo.sc"
        )
        set_tests_properties(
            houio.fixtures.triangles.houdini_scf_generate
            PROPERTIES
                FIXTURES_REQUIRED houio_fixture_sources
                FIXTURES_SETUP houio_houdini_scf_source
                ENVIRONMENT_MODIFICATION "PYTHONPATH=path_list_prepend:${PROJECT_SOURCE_DIR}/python"
        )

        add_test(
            NAME houio.fixtures.triangles.houdini_scf_read
            COMMAND
                $<TARGET_FILE:houio_convert>
                "${HOUIO_FIXTURE_SOURCE_DIR}/triangles_houdini.bgeo.sc"
                "${HOUIO_FIXTURE_OUTPUT_DIR}/triangles_houdini_scf.bgeo"
        )
        set_tests_properties(
            houio.fixtures.triangles.houdini_scf_read
            PROPERTIES
                FIXTURES_REQUIRED houio_houdini_scf_source
                FIXTURES_SETUP houio_houdini_scf_output
                ENVIRONMENT "HOUIO_BLOSC_LIBRARY=${HOUIO_HYTHON_BLOSC_LIBRARY}"
        )

        add_test(
            NAME houio.fixtures.triangles.houdini_scf_validate
            COMMAND
                "${HOUIO_HYTHON_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tools/houdini/compare_geometry.py"
                "${HOUIO_FIXTURE_SOURCE_DIR}/triangles.bgeo"
                "${HOUIO_FIXTURE_OUTPUT_DIR}/triangles_houdini_scf.bgeo"
                --point-attribute P
                --topology
        )
        set_tests_properties(
            houio.fixtures.triangles.houdini_scf_validate
            PROPERTIES
                FIXTURES_REQUIRED "houio_fixture_sources;houio_houdini_scf_output"
        )
    endif()

    add_test(
        NAME houio.fixtures.validate
        COMMAND
            "${HOUIO_HYTHON_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/houdini/validate_fixture_suite.py"
            "${HOUIO_FIXTURE_MANIFEST}"
            "${HOUIO_FIXTURE_SOURCE_DIR}"
            "${HOUIO_FIXTURE_OUTPUT_DIR}"
    )
    set_tests_properties(
        houio.fixtures.validate
        PROPERTIES
            FIXTURES_REQUIRED houio_fixture_sources
            DEPENDS "${HOUIO_FIXTURE_ROUNDTRIP_TESTS}"
    )

    set(HOUIO_CRAG_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/crag")
    set(HOUIO_CRAG_SOURCE "${HOUIO_CRAG_TEST_DIR}/crag_tpose.bgeo")
    set(HOUIO_CRAG_OUTPUT "${HOUIO_CRAG_TEST_DIR}/crag_tpose_houio.bgeo")

    add_test(
        NAME houio.crag.generate
        COMMAND
            "${HOUIO_HYTHON_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/houdini/generate_crag.py"
            "${HOUIO_CRAG_SOURCE}"
    )
    set_tests_properties(houio.crag.generate PROPERTIES FIXTURES_SETUP houio_crag_source)

    add_test(
        NAME houio.crag.roundtrip
        COMMAND $<TARGET_FILE:houio_roundtrip_geometry> "${HOUIO_CRAG_SOURCE}" "${HOUIO_CRAG_OUTPUT}"
    )
    set_tests_properties(
        houio.crag.roundtrip
        PROPERTIES
            FIXTURES_REQUIRED houio_crag_source
            FIXTURES_SETUP houio_crag_output
    )

    add_test(
        NAME houio.crag.compare
        COMMAND
            "${HOUIO_HYTHON_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/houdini/compare_geometry.py"
            "${HOUIO_CRAG_SOURCE}"
            "${HOUIO_CRAG_OUTPUT}"
            --point-attribute P
            --vertex-attribute N
            --vertex-attribute uv
            --primitive-string-attribute name
            --primitive-int-attribute piece
            --topology
    )
    set_tests_properties(houio.crag.compare PROPERTIES FIXTURES_REQUIRED houio_crag_output)

    add_test(
        NAME houio.crag.validate
        COMMAND
            "${HOUIO_HYTHON_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/houdini/validate_geometry.py"
            "${HOUIO_CRAG_OUTPUT}"
            --expect-points 90085
            --expect-primitives 89942
            --expect-point-attribute P
            --expect-vertex-attribute N
            --expect-vertex-attribute uv
            --expect-primitive-attribute name
            --expect-primitive-attribute piece
    )
    set_tests_properties(houio.crag.validate PROPERTIES FIXTURES_REQUIRED houio_crag_output)
endif()
