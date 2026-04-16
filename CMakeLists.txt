cmake_minimum_required(VERSION 3.16)
project(mvalgrind VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Main binary ───────────────────────────────────────────────────────────────

add_executable(mvalgrind
    src/main.cpp
    src/args.cpp
    src/classify.cpp
    src/image.cpp
    src/runner.cpp
)

target_include_directories(mvalgrind PRIVATE src)

target_compile_definitions(mvalgrind PRIVATE
    MVALGRIND_VERSION="${CMAKE_PROJECT_VERSION}"
)

# Warn loudly during development builds.
if(CMAKE_BUILD_TYPE STREQUAL "" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(mvalgrind PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────

enable_testing()
add_subdirectory(tests)
