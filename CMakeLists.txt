cmake_minimum_required(VERSION 3.25)

project(cgfs
    VERSION 0.1.0
    DESCRIPTION "Software 3D renderer written from scratch"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed. Configure into a separate directory, "
        "for example: cmake --preset dev")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config generators (Ninja, Makefiles) default to Release.
get_property(_cgfs_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _cgfs_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

option(CGFS_BUILD_TESTS "Build the tests and register them with CTest" ON)
option(CGFS_BUILD_APPS "Build the demo application" ON)
option(CGFS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(CGFS_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Applies the project's warning and sanitizer settings to one target. Flags go on
# targets, never on CMAKE_CXX_FLAGS, so third-party code is never affected.
function(cgfs_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(CGFS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        # -Wmissing-field-initializers (part of -Wextra) fires on designated initializers
        # that rely on default member values, which is how scenes are written here.
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
            -Wno-missing-field-initializers)
        if(CGFS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(CGFS_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

# --- Third-party code -----------------------------------------------------------------
add_subdirectory(third_party)

# --- The library ----------------------------------------------------------------------
add_library(cgfs STATIC
    src/canvas.cpp
    src/demo_scenes.cpp
    src/image_io.cpp
    src/rasterizer.cpp
    src/raytracer.cpp
    src/viewport.cpp)
add_library(cgfs::cgfs ALIAS cgfs)
target_include_directories(cgfs PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_features(cgfs PUBLIC cxx_std_20)
target_link_libraries(cgfs PRIVATE stb_image_write)
cgfs_set_warnings(cgfs)

# --- Applications ---------------------------------------------------------------------
if(CGFS_BUILD_APPS)
    add_subdirectory(apps)
endif()

# --- Tests ----------------------------------------------------------------------------
if(CGFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
