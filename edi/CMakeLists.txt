cmake_minimum_required(VERSION 3.8)
project("edi" VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME})

file(GLOB_RECURSE PROJECT_SORUCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS "src/*.hpp")
file(GLOB_RECURSE PROJECT_CONFIGS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_SOURCE_DIR}/.gitignore"
    "${CMAKE_SOURCE_DIR}/.gitmodules"
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SORUCES}
        ${PROJECT_HEADERS}
        ${PROJECT_CONFIGS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_SOURCE_DIR}/eng/include/"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        "eng"
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/bin/${PROJECT_NAME}/"
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall -Wpedantic -Werror
)
