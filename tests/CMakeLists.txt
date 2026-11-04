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

add_executable(cgfs_tests
    vec3_test.cpp
    color_test.cpp
    canvas_test.cpp
    image_io_test.cpp
    viewport_test.cpp
    raytracer_test.cpp
    rasterizer_test.cpp
    golden_test.cpp)
target_link_libraries(cgfs_tests PRIVATE cgfs::cgfs Catch2::Catch2WithMain)
target_compile_definitions(cgfs_tests PRIVATE CGFS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
cgfs_set_warnings(cgfs_tests)

if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)
catch_discover_tests(cgfs_tests)
