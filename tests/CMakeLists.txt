cmake_minimum_required(VERSION 3.20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-character-conversion)
endif()

# ---------------------------------------------------------------------------
# SANITIZER CONFIGURATION
# ---------------------------------------------------------------------------
if(ENABLE_SANITIZERS)
    message(STATUS "Sanitizers are ENABLED (ASan + UBSan).")
    if(WIN32)
        # Windows: Support MSVC (including clang-cl) and LLVM Clang
        if(MSVC)
            message(STATUS "Using MSVC/clang-cl AddressSanitizer on Windows.")
            # ASan on MSVC/clang-cl requires the non-debug CRT; force /MD instead of /MDd
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
            add_compile_options(/fsanitize=address /Zi)
            add_link_options(/DEBUG /INCREMENTAL:NO /LTCG:OFF)
        else()
            message(WARNING "Sanitizers on Windows are supported with MSVC/clang-cl compilers.")
            message(WARNING "Current compiler: ${CMAKE_CXX_COMPILER_ID}")
        endif()
    elseif(UNIX)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
            message(STATUS "Using Clang sanitizers on ${CMAKE_SYSTEM_NAME}.")
            add_compile_options(-fsanitize=address,undefined -g -Wno-character-conversion)
            add_link_options(-fsanitize=address,undefined)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            message(STATUS "Using GCC sanitizers on ${CMAKE_SYSTEM_NAME}.")
            add_compile_options(-fsanitize=address,undefined -g -Wno-character-conversion)
            add_link_options(-fsanitize=address,undefined)
        else()
            message(WARNING "Sanitizers requested on ${CMAKE_SYSTEM_NAME}, but not configured for compiler: ${CMAKE_CXX_COMPILER_ID}")
        endif()
    else()
        message(WARNING "Sanitizers requested on unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Test Project Configuration
# ---------------------------------------------------------------------------
project(WideLipsTests LANGUAGES CXX)

# Inherit C++ standard from parent project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Auto-install Google Test based on platform
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP true
)

# Force Google Test to be built as static libraries (ignore BUILD_SHARED_LIBS)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Group Google Test projects in a solution folder
# ---------------------------------------------------------------------------
if(MSVC)
    set_target_properties(gtest gmock gtest_main gmock_main
            PROPERTIES FOLDER "Dependencies/GoogleTest")
endif()


# ---------------------------------------------------------------------------
# Test Sources
# ---------------------------------------------------------------------------
set(TEST_SOURCES
        ../src/LispLexer.cpp
        ../src/Diagnostic.cpp
        ../src/LispParser.cpp
        ../src/AlignedFileReader.cpp
        LispTokenTests.cpp
        LispLexerTests.cpp
        AlignedFileReaderTests.cpp
        LispParserTests.cpp
        BumpVectorTests.cpp
        MonoBumpVectorTests.cpp
)

# ---------------------------------------------------------------------------
# Test Executable
# ---------------------------------------------------------------------------
add_executable(WideLipsTests ${TEST_SOURCES})

# ---------------------------------------------------------------------------
# Include Directories (inherit from library)
# ---------------------------------------------------------------------------
target_include_directories(WideLipsTests PRIVATE
        ../include
        ../include/Utilities
        ../include/ADT
)

# ---------------------------------------------------------------------------
# COVERAGE CONFIGURATION (Inherited from parent)
# ---------------------------------------------------------------------------
if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(WARNING "Coverage is only configured for Clang/clang-cl.")
    else()
        if(MSVC)
            target_compile_options(WideLipsTests PRIVATE /Od /Zi -fcoverage-mapping -fprofile-instr-generate)
            target_link_options(WideLipsTests PRIVATE /OPT:NOREF)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(WideLipsTests PRIVATE -O0 -fcoverage-mapping -fprofile-instr-generate)
        endif ()
    endif()
endif()

# ---------------------------------------------------------------------------
# Windows and MSVC sanitizers lib linking
# ---------------------------------------------------------------------------
# Link ASan runtime explicitly when using clang-cl/MSVC toolchain on Windows
if(ENABLE_SANITIZERS AND WIN32 AND MSVC)
    # Try to locate clang's resource directory to find ASan import library
    execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
            OUTPUT_VARIABLE CLANG_RESOURCE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(_ASAN_LIB "${CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.lib")
    set(_ASAN_THUNK_LIB "${CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")
    if(EXISTS "${_ASAN_LIB}")
        message(STATUS "Linking ASan runtime: ${_ASAN_LIB}")
        target_link_libraries(WideLipsTests PRIVATE "${_ASAN_LIB}")
        if(EXISTS "${_ASAN_THUNK_LIB}")
            message(STATUS "Linking ASan runtime thunk: ${_ASAN_THUNK_LIB}")
            target_link_libraries(WideLipsTests PRIVATE "${_ASAN_THUNK_LIB}")
        else()
            message(WARNING "ASan runtime thunk not found at ${_ASAN_THUNK_LIB}")
        endif()

        # Copy ASan runtime DLL next to the test executable to satisfy runtime dependency
        set(_ASAN_DLL_BIN "${CLANG_RESOURCE_DIR}/bin/clang_rt.asan_dynamic-x86_64.dll")
        set(_ASAN_DLL_LIB "${CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.dll")
        if(EXISTS "${_ASAN_DLL_BIN}")
            add_custom_command(TARGET WideLipsTests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ASAN_DLL_BIN}" "$<TARGET_FILE_DIR:WideLipsTests>")
        elseif(EXISTS "${_ASAN_DLL_LIB}")
            add_custom_command(TARGET WideLipsTests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ASAN_DLL_LIB}" "$<TARGET_FILE_DIR:WideLipsTests>")
        else()
            message(WARNING "ASan runtime DLL not found; ensure LLVM bin directory is on PATH.")
        endif()
    endif()
endif()


# ---------------------------------------------------------------------------
# Link to the library and test frameworks
# ---------------------------------------------------------------------------
target_link_libraries(WideLipsTests PRIVATE
        GTest::gtest_main
        GTest::gmock_main
)

# ---------------------------------------------------------------------------
# Test-specific compile definitions (override library definitions)
# ---------------------------------------------------------------------------
target_compile_definitions(WideLipsTests PRIVATE
        EnableHash
        EnableComma
        EnableBrackets
        EnableQuasiColumn
        EnableColumn
        EnableAtSign
        EnableBenjamin
        EnableDashInID
        EnableTilda
        SanitizersEnabled=$<BOOL:${ENABLE_SANITIZERS}>
        InvalidateEmptySExpr
        FuncKeyword="defun"
        VarKeyword="defvar"
        LambdaKeyword="lambda"
        TrueLiteral="true"
        FalseLiteral="false"
        NilKeyword="nil"
)

# ---------------------------------------------------------------------------
# Test Discovery
# ---------------------------------------------------------------------------
include(GoogleTest)
gtest_discover_tests(WideLipsTests)

# ---------------------------------------------------------------------------
# Dialect-specific test subprojects
# ---------------------------------------------------------------------------
add_subdirectory(dialects)