include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Unit test binary — no Docker required.
add_executable(test_unit
    unit/test_args.cpp
    unit/test_classify.cpp
    ${CMAKE_SOURCE_DIR}/src/args.cpp
    ${CMAKE_SOURCE_DIR}/src/classify.cpp
)

target_include_directories(test_unit PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_unit PRIVATE Catch2::Catch2WithMain)
target_compile_definitions(test_unit PRIVATE
    MVALGRIND_VERSION="${CMAKE_PROJECT_VERSION}"
)

include(CTest)
include(Catch)
catch_discover_tests(test_unit)
