
cmake_minimum_required(VERSION 3.28)

project(DueyLib
    VERSION 0.1.0
    DESCRIPTION "A lightweight, app-agnostic C++ static library and toolbox for music composition."
    LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# Build Options
# ------------------------------------------------------------------------------
# Defaults to ON when built standalone, and OFF when consumed via add_subdirectory/FetchContent
option(DUEY_BUILD_TESTS "Build the DueyLib GoogleTest unit test suite" ${PROJECT_IS_TOP_LEVEL})

# ------------------------------------------------------------------------------
# Library Target (DueyLib / Duey::DueyLib)
# ------------------------------------------------------------------------------
file(GLOB_RECURSE DUEY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE DUEY_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

add_library(DueyLib STATIC
    ${DUEY_SOURCES}
    ${DUEY_HEADERS}
)
add_library(Duey::DueyLib ALIAS DueyLib)

set_target_properties(DueyLib PROPERTIES
    OUTPUT_NAME duey              # Produces libduey.a (Unix) / duey.lib (Windows)
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Propagate C++20 requirement to downstream consumers
target_compile_features(DueyLib PUBLIC cxx_std_20)

target_include_directories(DueyLib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Enforce strict compiler warnings only when compiling DueyLib itself
target_compile_options(DueyLib
    PRIVATE
        $<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-Wall -Wextra -Wpedantic>
)

# ------------------------------------------------------------------------------
# Unit Tests (GoogleTest + CTest)
# ------------------------------------------------------------------------------
if(DUEY_BUILD_TESTS)
    enable_testing()
    include(FetchContent)

    # Uses a system-installed GTest if available, otherwise fetches v1.14.0
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        FIND_PACKAGE_ARGS NAMES GTest
    )

    # Prevent GoogleTest from overriding MSVC runtime flags or installing itself
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    file(GLOB_RECURSE DUEY_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    add_executable(duey_tests ${DUEY_TEST_SOURCES})

    set_target_properties(duey_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_link_libraries(duey_tests
        PRIVATE
            Duey::DueyLib
            GTest::gtest_main
    )

    target_compile_options(duey_tests
        PRIVATE
            $<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/W4>
            $<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:-Wall -Wextra -Wpedantic>
    )

    include(GoogleTest)
    gtest_discover_tests(duey_tests)
endif()
