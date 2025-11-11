cmake_minimum_required(VERSION 3.20)
project(WideLipsBench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Google Benchmark via FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(-Wno-error -Wno-switch -Wno-invalid-offsetof -O3 -march=native -flto -fomit-frame-pointer)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-error -Wno-switch -Wno-invalid-offsetof -O3 -march=native -flto -fomit-frame-pointer)
    endif()
endif()

set(BENCHMARK_ENABLE_GTEST OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_MakeAvailable(googlebenchmark)

# ---------------------------------------------------------------------------
# Library Headers
# ---------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_LIST_DIR}/../include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../include/Utilities)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../include/ADT)

# ---------------------------------------------------------------------------
# Benchmark target
# ---------------------------------------------------------------------------
add_executable(WideLipsBench Benchmarks.cpp)

target_link_libraries(WideLipsBench
        PRIVATE
        libWideLips
        benchmark::benchmark
        benchmark::benchmark_main
)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(WideLipsBench PRIVATE Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Copy DLL to executable directory (Windows + Shared libs)
# ---------------------------------------------------------------------------
if(WIN32 AND BUILD_SHARED_LIBS)
    # For MSVC multi-config generators (Debug/Release folders)
    add_custom_command(TARGET WideLipsBench POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libWideLips>
            $<TARGET_FILE_DIR:WideLipsBench>
            COMMENT "Copying libWideLips.dll to benchmark directory"
    )

    add_custom_command(TARGET WideLipsBench POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:benchmark::benchmark>
            $<TARGET_FILE_DIR:WideLipsBench>
            COMMENT "Copying benchmark.dll"
    )

    # Copy benchmark_main.dll if it exists
    add_custom_command(TARGET WideLipsBench POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:benchmark::benchmark_main>
            $<TARGET_FILE_DIR:WideLipsBench>
            COMMENT "Copying benchmark_main.dll"
    )
endif()

# Set working directory for Visual Studio
set_target_properties(WideLipsBench PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:WideLipsBench>
)

message(STATUS "Configured benchmark target: WideLipsBench")