# Project for Clojure-specific syntax tests
project(WideLipsScheme)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Test Sources
# ---------------------------------------------------------------------------
set(TEST_SOURCES
        ../../src/LispLexer.cpp
        ../../src/Diagnostic.cpp
        ../../src/LispParser.cpp
        ../../src/AlignedFileReader.cpp
        SchemeParser.cpp
        main.cpp
)

# ---------------------------------------------------------------------------
# Dialect Test Executable
# ---------------------------------------------------------------------------
add_executable(WideLipsScheme ${TEST_SOURCES})

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------
target_include_directories(WideLipsScheme PRIVATE
        ../../include        # Access to parent's include dir
        ../../include/Utilities
        ../../include/ADT
)

# ---------------------------------------------------------------------------
# Scheme R7RS-specific compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(WideLipsScheme PRIVATE
        # --- Enable Scheme R7RS Syntax ---
        EnableHash              # for #t, #f, #(...) vectors, #\c characters
        EnableQuasiColumn       # for ` (quasiquote)
        EnableComma             # for , (unquote) and ,@ (unquote-splicing)
        EnableAtSign            # for @ in ,@
        EnableDashInID          # for identifiers like list->vector

        # --- Define Scheme R7RS Keywords ---
        FuncKeyword="define"     # (define (f x) ...) and (define f ...)
        #VarKeyword="define"     # Same as FuncKeyword in Scheme
        LambdaKeyword="lambda"   # (lambda (x) ...)
        TrueLiteral="true"       # #t in syntax, but we use "true" for token
        FalseLiteral="false"     # #f in syntax, but we use "false" for token
        NilKeyword="nil"         # empty list representation

        # --- Scheme allows empty expressions in some contexts ---
        # InvalidateEmptySExpr is NOT defined for Scheme
)

# ---------------------------------------------------------------------------
# Compilation options
# ---------------------------------------------------------------------------


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(WideLipsScheme PUBLIC DEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(WideLipsScheme PUBLIC RELEASE)
endif()

# Compilation flags
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(WideLipsScheme PRIVATE -O3 -march=native -flto -fomit-frame-pointer -mavx2 -mfma)
        elseif(MSVC)
            target_compile_options(WideLipsScheme PRIVATE /O2 /Oy /GL /arch:AVX2)
            target_link_options(WideLipsScheme PRIVATE /LTCG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            target_compile_options(WideLipsScheme PRIVATE -O3 -march=native -flto -fomit-frame-pointer -mavx2 -mfma)
        endif()
    endif()
endif()