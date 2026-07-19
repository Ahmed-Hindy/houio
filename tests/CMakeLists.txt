find_package(Threads REQUIRED)

add_executable(houio_test_logger main.cpp)
target_link_libraries(houio_test_logger PRIVATE houio::houio)
target_compile_features(houio_test_logger PRIVATE cxx_std_20)
target_compile_definitions(houio_test_logger PRIVATE TESTS_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME houio.logger COMMAND houio_test_logger)

add_executable(houio_test_binary_json test_binary_json.cpp)
target_link_libraries(houio_test_binary_json PRIVATE houio::houio)
target_compile_features(houio_test_binary_json PRIVATE cxx_std_20)
add_test(NAME houio.binary_json COMMAND houio_test_binary_json)

add_executable(houio_test_attribute test_attribute.cpp)
target_link_libraries(houio_test_attribute PRIVATE houio::houio)
target_compile_features(houio_test_attribute PRIVATE cxx_std_20)
add_test(NAME houio.attribute COMMAND houio_test_attribute)

add_executable(houio_test_modern_geometry test_modern_geometry.cpp)
target_link_libraries(houio_test_modern_geometry PRIVATE houio::houio)
target_compile_features(houio_test_modern_geometry PRIVATE cxx_std_20)
add_test(NAME houio.modern_geometry COMMAND houio_test_modern_geometry)

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

add_executable(houio_test_export_safety test_export_safety.cpp)
target_link_libraries(houio_test_export_safety PRIVATE houio::houio Threads::Threads)
target_compile_features(houio_test_export_safety PRIVATE cxx_std_20)
add_test(NAME houio.export_safety COMMAND houio_test_export_safety)

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
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_package_consumer.cmake"
)

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
    target_compile_definitions(houio_example_readwrite PRIVATE TESTS_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(HOUIO_HYTHON_EXECUTABLE)
    if(NOT EXISTS "${HOUIO_HYTHON_EXECUTABLE}")
        message(FATAL_ERROR "HOUIO_HYTHON_EXECUTABLE does not exist: ${HOUIO_HYTHON_EXECUTABLE}")
    endif()

    set(HOUIO_FIXTURE_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
    set(HOUIO_FIXTURE_SOURCE_DIR "${HOUIO_FIXTURE_TEST_DIR}/source")
    set(HOUIO_FIXTURE_OUTPUT_DIR "${HOUIO_FIXTURE_TEST_DIR}/output")
    set(HOUIO_FIXTURE_MANIFEST "${HOUIO_FIXTURE_SOURCE_DIR}/manifest.json")
    set(
        HOUIO_FIXTURE_NAMES
        empty
        point_attributes
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
