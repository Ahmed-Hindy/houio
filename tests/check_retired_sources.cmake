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

file(READ "${HOUIO_SOURCE_DIR}/include/houio/Field.h" field_header)
foreach(retired_name IN ITEMS "sample(" "lvalue(" "getResolution(" "getVoxelSize(")
    string(FIND "${field_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Field public API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/Geometry.h" geometry_header)
foreach(retired_name IN ITEMS
    "getBound("
    "hasAttr("
    "removeAttr("
    "numPrimitives("
    "numPrimitiveVertices("
)
    string(FIND "${geometry_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Geometry public API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/Attribute.h" attribute_header)
foreach(retired_pattern IN ITEMS
    "static constexpr ComponentType INVALID"
    "static constexpr ComponentType INT"
    "static constexpr ComponentType FLOAT"
    "static constexpr ComponentType INT64"
    "static constexpr ComponentType HALF"
    "std::span<std::byte> bytes()"
    "std::span<std::byte> elementBytes("
)
    string(FIND "${attribute_header}" "${retired_pattern}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Attribute API reintroduced retired pattern ${retired_pattern}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/HouGeoAdapter.h" adapter_header)
foreach(retired_pattern IN ITEMS
    "ATTR_TYPE_"
    "ATTR_STORAGE_"
    "getName("
    "getType("
    "getTupleSize("
    "getStorage("
    "getNumElements("
    "getString("
    "getDictionary("
    "friend class ::houio::HouGeoIO"
)
    string(FIND "${adapter_header}" "${retired_pattern}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Adapter API reintroduced retired pattern ${retired_pattern}")
    endif()
endforeach()

foreach(vector_header IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec2.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec3.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec4.h"
)
    file(READ "${vector_header}" vector_content)
    foreach(retired_name IN ITEMS
        "getLength("
        "getSquaredLength("
        "constexpr bool operator+=("
        "constexpr bool operator-=("
        "constexpr bool operator*=("
        "constexpr bool operator/=("
    )
        string(FIND "${vector_content}" "${retired_name}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Vector API reintroduced retired pattern ${retired_name}: ${vector_header}")
        endif()
    endforeach()
endforeach()

foreach(vector_algo IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec2Algo.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec3Algo.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec4Algo.h"
)
    file(READ "${vector_algo}" vector_algo_content)
    foreach(retired_pattern IN ITEMS
        "dotProduct("
        "crossProduct("
        "nondominantAxis("
        "baseVec3("
        "baseVec4("
        "coordinateSystem("
        "#include <math.h>"
        "#include \"Vec2Algo.h\""
    )
        string(FIND "${vector_algo_content}" "${retired_pattern}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Vector algorithm reintroduced retired pattern ${retired_pattern}: ${vector_algo}")
        endif()
    endforeach()
endforeach()

foreach(matrix_header IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix22.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix33.h"
)
    file(READ "${matrix_header}" matrix_content)
    foreach(retired_name IN ITEMS
        "getDeterminant("
        "Zero("
        "Identity("
        "RotationMatrix("
    )
        string(FIND "${matrix_content}" "${retired_name}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Matrix API reintroduced retired name ${retired_name}: ${matrix_header}")
        endif()
    endforeach()
endforeach()

foreach(matrix_algo IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix22Algo.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix33Algo.h"
)
    file(READ "${matrix_algo}" matrix_algo_content)
    string(FIND "${matrix_algo_content}" "matrixMultiply(" retired_matrix_multiply)
    if(NOT retired_matrix_multiply EQUAL -1)
        message(FATAL_ERROR "Matrix algorithm reintroduced output-parameter multiplication: ${matrix_algo}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix44.h" matrix44_header)
foreach(retired_name IN ITEMS
    "getRight("
    "getUp("
    "getDir("
    "getTranslation("
    "getOrientation("
    "getNormalizedOrientation("
    "getTransposed("
    "Zero("
    "Identity("
    "RotationMatrixX("
    "RotationMatrixY("
    "RotationMatrixZ("
    "RotationMatrix("
    "TranslationMatrix("
    "ScaleMatrix("
    "inverse()"
    "setDir("
    "multiplied("
)
    string(FIND "${matrix44_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Matrix44 API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix44Algo.h" matrix44_algo)
foreach(retired_name IN ITEMS
    "extractEulerXYZ("
    "extractAxisAngle("
    "matrixMultiply("
    "createLookAtMatrix("
    "createMatrixFromPolarCoordinates("
    "basisFromVector("
    "transformFromVector("
    "orthographicProjectionMatrix("
    "projectionMatrix("
    "constexpr void transform("
    "constexpr Matrix44<T>& transpose("
)
    string(FIND "${matrix44_algo}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Matrix44 algorithm reintroduced retired name ${retired_name}")
    endif()
endforeach()

foreach(bounds_header IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/BoundingBox2.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/BoundingBox3.h"
)
    file(READ "${bounds_header}" bounds_content)
    foreach(retired_name IN ITEMS "getCenter(" "isEmpty(" "makeEmpty(" "maxExtend(" "extendBy(")
        string(FIND "${bounds_content}" "${retired_name}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Bounding-box API reintroduced retired name ${retired_name}: ${bounds_header}")
        endif()
    endforeach()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Ray3.h" ray_header)
string(FIND "${ray_header}" "getPosition(" retired_ray_position)
if(NOT retired_ray_position EQUAL -1)
    message(FATAL_ERROR "Ray3 API reintroduced retired name getPosition(")
endif()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Color.h" color_header)
foreach(retired_name IN ITEMS
    "From255("
    "makeDWORD("
    "White("
    "Black("
    "Blue("
    "Yellow("
    "Green("
    "Red("
    "invert()"
)
    string(FIND "${color_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Color API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Math.h" math_header)
foreach(retired_name IN ITEMS
    "getAlpha("
    "getRed("
    "getGreen("
    "getBlue("
    "setColor("
    "setRGBColor("
    "radToDeg("
    "degToRad("
    "#define MATH_"
    "float area("
    "distancePointPlane("
    "distancePointLine("
    "distancePointTriangle("
    "projectPointOnPlane("
    "projectPointOnLine("
    "mapValueToRange("
    "mapValueTo0_1("
    "Vec3f slerp("
    "float clamp("
    "inline T max("
    "inline T min("
    "inline T clamp("
    "sphericalToCartesian("
    "cartesianToSpherical("
    "bool quadratic("
)
    string(FIND "${math_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Math color API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/RNG.h" rng_header)
foreach(retired_pattern IN ITEMS
    "#define MATH_RNG_"
    "class RNG"
    "randomFloat("
    "randomUInt("
    "Seed("
    "g_randomNumber"
)
    string(FIND "${rng_header}" "${retired_pattern}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Random API reintroduced retired pattern ${retired_pattern}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/CMakeLists.txt" root_cmake)
string(FIND "${root_cmake}" "src/math/Half/half.cpp" compiled_half_reference)
if(NOT compiled_half_reference EQUAL -1)
    message(FATAL_ERROR "The retired half implementation is still part of the build")
endif()
