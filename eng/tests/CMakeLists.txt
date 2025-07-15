set(PROJECT_NAME ${PROJECT_NAME}_tests)
project("eng-tests")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(gtest_force_shared_crt ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE TESTS_SOURCES "*.cpp")

enable_testing()

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE
        ${TESTS_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_SOURCE_DIR}/eng/include"
)

target_link_libraries(${PROJECT_NAME} gtest gtest_main "eng")

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY 
            "${CMAKE_SOURCE_DIR}/out/bin/${PROJECT_NAME}-${CMAKE_BUILD_TYPE}"
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall -Wpedantic -Werror
)
