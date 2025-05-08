cmake_minimum_required(VERSION 3.8)
project("eng" VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)

add_subdirectory("extern/glfw")

add_library(${PROJECT_NAME} SHARED)

file(GLOB_RECURSE PROJECT_SORUCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS "include/eng/*.hpp")
file(GLOB_RECURSE PROJECT_VENDORS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/glad/src/glad.c"
)
file(GLOB_RECURSE PROJECT_CONFIGS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_SOURCE_DIR}/.gitignore"
    "${CMAKE_SOURCE_DIR}/.gitmodules"
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SORUCES}
        ${PROJECT_HEADERS}
        ${PROJECT_VENDORS}
        ${PROJECT_CONFIGS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/eng"
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/glfw/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/glad/include"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        ${GLFW_LIBRARIES}
        ${GLAD_LIBRARIES}
)
