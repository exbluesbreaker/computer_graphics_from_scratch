include(FetchContent)

# A system-wide Catch2 3.x is used when present (FIND_PACKAGE_ARGS); otherwise it is
# fetched. SYSTEM keeps its headers out of our warning settings.
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 3)
FetchContent_MakeAvailable(Catch2)

# The test tree mirrors the source tree: tests/<layer>/<module>_test.cpp.
add_executable(cgfs_tests
    math/vec3_test.cpp
    math/mat4_test.cpp
    image/color_test.cpp
    image/canvas_test.cpp
    image/image_io_test.cpp
    camera/viewport_test.cpp
    camera/camera_test.cpp
    raytracer/raytracer_test.cpp
    rasterizer/interpolate_test.cpp
    rasterizer/line_test.cpp
    rasterizer/triangle_test.cpp
    rasterizer/projection_test.cpp
    rasterizer/object_test.cpp
    rasterizer/model_test.cpp
    rasterizer/instance_test.cpp
    rasterizer/scene_test.cpp
    demo/golden_test.cpp)
# So that helpers are included by their path in the tree: "support/test_helpers.h".
target_include_directories(cgfs_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cgfs_tests PRIVATE cgfs::demo Catch2::Catch2WithMain)
target_compile_definitions(cgfs_tests
    PRIVATE CGFS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo/golden")
cgfs_set_warnings(cgfs_tests)

if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)
catch_discover_tests(cgfs_tests)
