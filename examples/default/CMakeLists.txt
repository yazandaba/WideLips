cmake_minimum_required(VERSION 3.20)
project(WideLipsUsageExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Library Headers
# ---------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_LIST_DIR}/../../include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../../include/Utilities)
include_directories(${CMAKE_CURRENT_LIST_DIR}/../../include/ADT)

# ---------------------------------------------------------------------------
# Example target
# ---------------------------------------------------------------------------
add_executable(WideLipsUsageExample Parse.cpp)

# ---------------------------------------------------------------------------
# Link against libraries
# ---------------------------------------------------------------------------
target_link_libraries(WideLipsUsageExample
        PRIVATE
        libWideLips
)