# Project for CommonLisp-specific syntax tests
project(CommonLispTests)

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
        CommonLispTests.cpp
)

# ---------------------------------------------------------------------------
# Dialect Test Executable
# ---------------------------------------------------------------------------
add_executable(CommonLispTests ${TEST_SOURCES})

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------
target_include_directories(CommonLispTests PRIVATE
        ../../../include        # Access to parent's include dir
        ../../../include/Utilities
        ../../../include/ADT
)

# ---------------------------------------------------------------------------
# Common Lisp-specific compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(CommonLispTests PRIVATE
        # --- Enable Common Lisp Syntax ---
        EnableHash              # for #A(...) #\char
        EnableComma             # for , and ,@
        EnableQuasiColumn       # for ` (quasiquote)
        EnableColumn            # for package:symbol
        EnableAtSign            # for ,@ (unquote-splicing)
        EnableDashInID          # for symbols like my-func
        AggressiveVectorization
        InvalidateEmptySExpr

        # --- Define Common Lisp Keywords ---
        FuncKeyword="defun"
        VarKeyword="defvar"
        LambdaKeyword="lambda"
        TrueLiteral="t"
        FalseLiteral="nil"      # In Common Lisp, 'nil' is false
        NilKeyword=""
)

# ---------------------------------------------------------------------------
# Link to test frameworks (GTest:: targets are global)
# ---------------------------------------------------------------------------
target_link_libraries(CommonLispTests PRIVATE
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
            target_compile_options(CommonLispTests PRIVATE /Od /Zi -fcoverage-mapping -fprofile-instr-generate)
            target_link_options(CommonLispTests PRIVATE /OPT:NOREF)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(CommonLispTests PRIVATE -O0 -fcoverage-mapping -fprofile-instr-generate)
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
        target_link_libraries(CommonLispTests PRIVATE "${_ASAN_LIB}")
        if(EXISTS "${_ASAN_THUNK_LIB}")
            target_link_libraries(CommonLispTests PRIVATE "${_ASAN_THUNK_LIB}")
        endif()

        set(_ASAN_DLL_BIN "${CLANG_RESOURCE_DIR}/bin/clang_rt.asan_dynamic-x86_64.dll")
        set(_ASAN_DLL_LIB "${CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.dll")

        if(EXISTS "${_ASAN_DLL_BIN}")
            add_custom_command(TARGET CommonLispTests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ASAN_DLL_BIN}" "$<TARGET_FILE_DIR:CommonLispTests>")
        elseif(EXISTS "${_ASAN_DLL_LIB}")
            add_custom_command(TARGET CommonLispTests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ASAN_DLL_LIB}" "$<TARGET_FILE_DIR:CommonLispTests>")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Test Discovery
# ---------------------------------------------------------------------------
include(GoogleTest)
gtest_discover_tests(CommonLispTests)