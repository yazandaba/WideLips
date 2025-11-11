cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Library Project Configuration
# ---------------------------------------------------------------------------
project(libWideLips LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Library Sources
# ---------------------------------------------------------------------------
set(LIB_SOURCES
        LispLexer.cpp
        Diagnostic.cpp
        LispParser.cpp
        AlignedFileReader.cpp
)

# ---------------------------------------------------------------------------
# Option to build shared libraries
# ---------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# ---------------------------------------------------------------------------
# Library Target (Single Target)
# ---------------------------------------------------------------------------
# Set the library type based on the option
set(WIDELIPS_LIB_TYPE STATIC)
if(BUILD_SHARED_LIBS)
    set(WIDELIPS_LIB_TYPE SHARED)
endif()

# Define the single library target
add_library(libWideLips ${WIDELIPS_LIB_TYPE} ${LIB_SOURCES})

# Set properties directly on the single target
set_target_properties(libWideLips PROPERTIES
        OUTPUT_NAME libWideLips # Ensures the name is "libWideLips"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Conditionally apply definitions for a shared build
if(BUILD_SHARED_LIBS)
    target_compile_definitions(libWideLips PRIVATE WIDELIPS_EXPORTS)
    target_compile_definitions(libWideLips PUBLIC WIDELIPS_SHARED)
    set_target_properties(libWideLips PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
    )
endif()

# ---------------------------------------------------------------------------
# Common configuration function
# ---------------------------------------------------------------------------
function(configure_widelips_target TARGET_NAME)
    # Include Directories
    target_include_directories(${TARGET_NAME} PUBLIC
            ${PROJECT_SOURCE_DIR}/../include
            ${PROJECT_SOURCE_DIR}/../include/Utilities
            ${PROJECT_SOURCE_DIR}/../include/ADT
    )

    # Library Compile Definitions (Main Configuration)
    target_compile_definitions(${TARGET_NAME} PUBLIC
            #EnableHash
            EnableComma
            #EnableBrackets
            EnableQuasiColumn
            #EnableColumn
            EnableAtSign
            #EnableBenjamin
            EnableDashInID
            #EnableTilda
            #InvalidateEmptySExpr
            FuncKeyword="defun"
            VarKeyword="defvar"
            LambdaKeyword="lambda"
            TrueLiteral="true"
            FalseLiteral="false"
            NilKeyword="nil"
    )

    # Add debug/release specific definitions
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${TARGET_NAME} PUBLIC DEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_compile_definitions(${TARGET_NAME} PUBLIC RELEASE)
    endif()

    # Compilation flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_compile_options(${TARGET_NAME} PRIVATE -O3 -march=native -flto -fomit-frame-pointer -mavx2 -mfma)
                message(STATUS "Using Clang flags: -O3 -march=native -flto -fomit-frame-pointer")
            elseif(MSVC)
                target_compile_options(${TARGET_NAME} PRIVATE /O2 /Oy /GL /arch:AVX2)
                target_link_options(${TARGET_NAME} PRIVATE /LTCG)
                message(STATUS "Using MSVC flags: /O2 /Oy /GL /LTCG")
            elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                target_compile_options(${TARGET_NAME} PRIVATE -O3 -march=native -flto -fomit-frame-pointer -mavx2 -mfma)
                message(STATUS "Using GCC flags: -O3 -march=native -flto -fomit-frame-pointer")
            endif()
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Apply configuration to the single target
# ---------------------------------------------------------------------------
configure_widelips_target(libWideLips)