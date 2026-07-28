set(
    HOUIO_SCENE_IO_PROVIDER
    "disabled"
    CACHE STRING
    "Scene writer dependency provider: disabled, houdini, system, or bundled"
)
set_property(
    CACHE HOUIO_SCENE_IO_PROVIDER
    PROPERTY STRINGS disabled houdini system bundled
)
set(
    HOUIO_BUNDLED_SCENE_IO_ROOT
    ""
    CACHE PATH
    "Installed prefix containing HouIO's pinned Alembic and OpenUSD dependencies"
)

set(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER "${HOUIO_SCENE_IO_PROVIDER}")
set(HOUIO_HAS_ALEMBIC_VALUE 0)
set(HOUIO_HAS_USD_VALUE 0)
set(HOUIO_SCENE_IO_LIBRARIES)
set(HOUIO_SCENE_IO_INCLUDE_DIRECTORIES)
set(HOUIO_SCENE_BACKEND_COMPILE_TARGETS)
set(HOUIO_CONFIG_FIND_SCENE_IO "")

if(HOUIO_BUILD_HOUDINI_PLUGIN)
    if(NOT HOUIO_HOUDINI_ROOT)
        message(FATAL_ERROR
            "HOUIO_BUILD_HOUDINI_PLUGIN=ON requires HOUIO_HOUDINI_ROOT")
    endif()
    set(Houdini_DIR "${HOUIO_HOUDINI_ROOT}/toolkit/cmake")
    find_package(Houdini CONFIG REQUIRED)

    # Preserve the existing developer workflow while bundled upstream
    # dependencies are being prepared. Release presets must select bundled.
    if(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "disabled")
        set(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER "houdini")
    endif()
endif()

function(houio_require_target output_variable description)
    foreach(candidate IN LISTS ARGN)
        if(TARGET "${candidate}")
            set("${output_variable}" "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "The ${description} package was found but none of these targets exist: ${ARGN}")
endfunction()

if(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "disabled")
    # Dependency-free builds retain explicit unavailable-backend diagnostics.
elseif(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "houdini")
    if(NOT HOUIO_BUILD_HOUDINI_PLUGIN)
        message(FATAL_ERROR
            "HOUIO_SCENE_IO_PROVIDER=houdini is a plugin validation provider and "
            "requires HOUIO_BUILD_HOUDINI_PLUGIN=ON")
    endif()
    if(NOT WIN32)
        message(FATAL_ERROR
            "The current Houdini scene dependency provider is implemented for Windows only")
    endif()

    set(HOUIO_HAS_ALEMBIC_VALUE 1)
    set(HOUIO_HAS_USD_VALUE 1)
    set(HOUIO_HOUDINI_DSOLIB_DIRECTORY
        "${HOUIO_HOUDINI_ROOT}/custom/houdini/dsolib")

    file(GLOB HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES
        "${HOUIO_HOUDINI_ROOT}/python*/libs/python*.lib")
    list(FILTER HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES
        EXCLUDE REGEX "[/\\\\]python3\\.lib$|stub\\.lib$")
    list(LENGTH HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES
        HOUIO_HOUDINI_PYTHON_LIBRARY_COUNT)
    if(NOT HOUIO_HOUDINI_PYTHON_LIBRARY_COUNT EQUAL 1)
        message(FATAL_ERROR
            "Expected exactly one Houdini Python import library, found: "
            "${HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES}")
    endif()
    list(GET HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES 0
        HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARY)

    set(HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES)
    foreach(optional_library IN ITEMS libpxr_boost.lib libpxr_python.lib)
        if(EXISTS "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/${optional_library}")
            list(APPEND HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES
                "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/${optional_library}")
        endif()
    endforeach()
    file(GLOB HOUIO_HOUDINI_BOOST_PYTHON_LIBRARIES
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/hboost_python*-mt-x64.lib")

    if(EXISTS "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/tbb12.lib")
        set(HOUIO_HOUDINI_TBB_LIBRARY
            "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/tbb12.lib")
    elseif(EXISTS "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/tbb.lib")
        set(HOUIO_HOUDINI_TBB_LIBRARY
            "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/tbb.lib")
    else()
        message(FATAL_ERROR
            "Houdini scene provider could not locate a bundled TBB import library")
    endif()

    list(APPEND HOUIO_SCENE_IO_LIBRARIES
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/Alembic_sidefx.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_arch.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_ar.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_gf.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_js.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_kind.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_pcp.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_plug.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_sdf.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_tf.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_trace.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_usd.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_usdGeom.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_vt.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_work.lib"
        "${HOUIO_HOUDINI_TBB_LIBRARY}"
        ${HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES}
        ${HOUIO_HOUDINI_BOOST_PYTHON_LIBRARIES}
        "${HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARY}")
    list(APPEND HOUIO_SCENE_IO_INCLUDE_DIRECTORIES
        "${HOUIO_HOUDINI_ROOT}/toolkit/include/Imath")
    list(APPEND HOUIO_SCENE_BACKEND_COMPILE_TARGETS Houdini)
elseif(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "system"
       OR HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "bundled")
    if(HOUIO_EFFECTIVE_SCENE_IO_PROVIDER STREQUAL "bundled")
        if(NOT HOUIO_BUNDLED_SCENE_IO_ROOT)
            message(FATAL_ERROR
                "HOUIO_SCENE_IO_PROVIDER=bundled requires HOUIO_BUNDLED_SCENE_IO_ROOT")
        endif()
        if(NOT EXISTS "${HOUIO_BUNDLED_SCENE_IO_ROOT}")
            message(FATAL_ERROR
                "HOUIO_BUNDLED_SCENE_IO_ROOT does not exist: "
                "${HOUIO_BUNDLED_SCENE_IO_ROOT}")
        endif()
        list(PREPEND CMAKE_PREFIX_PATH "${HOUIO_BUNDLED_SCENE_IO_ROOT}")
    endif()

    find_package(Alembic CONFIG REQUIRED)
    find_package(pxr CONFIG QUIET)
    if(pxr_FOUND)
        set(HOUIO_USD_CONFIG_PACKAGE "pxr")
    else()
        find_package(OpenUSD CONFIG REQUIRED)
        set(HOUIO_USD_CONFIG_PACKAGE "OpenUSD")
    endif()

    houio_require_target(
        HOUIO_ALEMBIC_TARGET
        "Alembic"
        Alembic::Alembic
        Alembic)
    list(APPEND HOUIO_SCENE_IO_LIBRARIES "${HOUIO_ALEMBIC_TARGET}")

    foreach(component IN ITEMS arch ar gf js kind pcp plug sdf tf trace usd usdGeom vt work)
        houio_require_target(
            HOUIO_USD_${component}_TARGET
            "OpenUSD ${component}"
            pxr::${component}
            OpenUSD::${component}
            ${component})
        list(APPEND HOUIO_SCENE_IO_LIBRARIES
            "${HOUIO_USD_${component}_TARGET}")
    endforeach()

    set(HOUIO_HAS_ALEMBIC_VALUE 1)
    set(HOUIO_HAS_USD_VALUE 1)
    set(HOUIO_CONFIG_FIND_SCENE_IO
        "include(CMakeFindDependencyMacro)\nfind_dependency(Alembic CONFIG)\nfind_dependency(${HOUIO_USD_CONFIG_PACKAGE} CONFIG)")
else()
    message(FATAL_ERROR
        "Unknown HOUIO_SCENE_IO_PROVIDER: ${HOUIO_SCENE_IO_PROVIDER}")
endif()
