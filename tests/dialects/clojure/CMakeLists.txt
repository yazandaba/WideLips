# Project for Clojure-specific syntax tests
project(ClojureTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Test Sources
# ---------------------------------------------------------------------------
set(TEST_SOURCES
        ../../../src/LispLexer.cpp
        ../../../src/Diagnostic.cpp
        ../../../src/LispParser.cpp
        ../../../src/AlignedFileReader.cpp
        ClojureTests.cpp
)

# ---------------------------------------------------------------------------
# Dialect Test Executable
# ---------------------------------------------------------------------------
add_executable(ClojureTests ${TEST_SOURCES})

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------
target_include_directories(ClojureTests PRIVATE
        ../../../include        # Access to parent's include dir
        ../../../include/Utilities
        ../../../include/ADT
)

# ---------------------------------------------------------------------------
# Clojure-specific compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(ClojureTests PRIVATE
        # --- Enable Clojure Syntax ---
        EnableHash              # for #{...} #(...)
        EnableTilda             # for ~
        EnableBrackets          # for [vectors]
        EnableQuasiColumn       # for ` (syntax-quote)
        EnableColumn            # for :keywords
        EnableAtSign            # for @deref
        EnableDashInID          # for symbols like my-func
        InvalidateEmptySExpr

        # --- Define Clojure Keywords ---
        FuncKeyword="defn"
        VarKeyword="def"
        LambdaKeyword="fn"
        TrueLiteral="true"
        FalseLiteral="false"
        NilKeyword="nil"
)

# ---------------------------------------------------------------------------
# Link to test frameworks (GTest:: targets are global)
# ---------------------------------------------------------------------------
target_link_libraries(ClojureTests PRIVATE
        GTest::gtest_main
        GTest::gmock_main
)

# ---------------------------------------------------------------------------
# COVERAGE CONFIGURATION (Inherited from parent)
# ---------------------------------------------------------------------------
if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(WARNING "Coverage is only configured for Clang/clang-cl.")
    else()
        if(MSVC)
            target_compile_options(ClojureTests PRIVATE /Od /Zi -fcoverage-mapping -fprofile-instr-generate)
            target_link_options(ClojureTests PRIVATE /OPT:NOREF)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(ClojureTests PRIVATE -O0 -fcoverage-mapping -fprofile-instr-generate)
        endif ()
    endif()
endif()

# ---------------------------------------------------------------------------
# Windows and MSVC sanitizers lib linking (Inherited from parent)
# ---------------------------------------------------------------------------
if(ENABLE_SANITIZERS AND WIN32 AND MSVC)
    if(NOT CLANG_RESOURCE_DIR)
        message(WARNING "CLANG_RESOURCE_DIR not set. Re-running discovery.")
        execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
                OUTPUT_VARIABLE CLANG_RESOURCE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()

    set(_ASAN_LIB "${CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.lib")
    set(_ASAN_THUNK_LIB "${CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")

    if(EXISTS "${_ASAN_LIB}")
        target_link_libraries(ClojureTests PRIVATE "${_ASAN_LIB}")
        if(EXISTS "${_ASAN_THUNK_LIB}")
            target_link_libraries(ClojureTests PRIVATE "${_ASAN_THUNK_LIB}")
        endif()

        set(_ASAN_DLL_BIN "${CLANG_RESOURCE_DIR}/bin/clang_rt.asan_dynamic-x86_64.dll")
        set(_ASAN_DLL_LIB "${CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.dll")

        if(EXISTS "${_ASAN_DLL_BIN}")
            add_custom_command(TARGET ClojureTests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ASAN_DLL_BIN}" "$<TARGET_FILE_DIR:ClojureTests>")
        elseif(EXISTS "${_ASAN_DLL_LIB}")
            add_custom_command(TARGET ClojureTests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ASAN_DLL_LIB}" "$<TARGET_FILE_DIR:ClojureTests>")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Test Discovery
# ---------------------------------------------------------------------------
include(GoogleTest)
gtest_discover_tests(ClojureTests)