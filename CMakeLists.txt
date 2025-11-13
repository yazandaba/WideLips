cmake_minimum_required(VERSION 3.20)
project(WideLips)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_TESTING ON)
set(BUILD_BENCHMARK ON)
set(BUILD_EXAMPLES ON)

# ---------------------------------------------------------------------------
# AVX2 Support Detection and Flags
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma ")
        message(STATUS "Using Clang flags: -mavx2 -mfma for AVX detection")
    elseif(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
        message(STATUS "Using MSVC flags: /arch:AVX2 for AVX detection")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
        message(STATUS "Using GCC flags: -mavx2 -mfma for AVX detection")
    else()
        message(WARNING "Unknown compiler, AVX2 flags not set")
    endif()
else()
    message(WARNING "AVX2 is only supported on x86-64 architecture")
endif()


# ---------------------------------------------------------------------------
# Library Submodule
# ---------------------------------------------------------------------------
add_subdirectory(src)

# ---------------------------------------------------------------------------
# Testing Submodule
# ---------------------------------------------------------------------------
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Building with tests enabled")
endif()
# ---------------------------------------------------------------------------
# benchmarking Submodule
# ---------------------------------------------------------------------------
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_BENCHMARK)
    add_subdirectory(benchmark)
    message(STATUS "Building with benchmarks enabled")
endif()
# ---------------------------------------------------------------------------
# Examples Submodule
# ---------------------------------------------------------------------------
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_EXAMPLES)
    add_subdirectory(examples)
    message(STATUS "Building with examples enabled")
endif()

# ---------------------------------------------------------------------------
# Visual Studio Solution Folder Grouping
# ---------------------------------------------------------------------------
# Enable and set up solution folders for better organization in Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if(MSVC)
    if(TARGET ALL_BUILD)
        set_target_properties(ALL_BUILD PROPERTIES FOLDER "CMake/Solution Items")
    endif()
    if(TARGET ZERO_CHECK)
        set_target_properties(ZERO_CHECK PROPERTIES FOLDER "CMake/Solution Items")
    endif()

    if(TARGET INSTALL)
        set_target_properties(INSTALL PROPERTIES FOLDER "CMake/Solution Items")
    endif()
endif()
