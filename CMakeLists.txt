cmake_minimum_required(VERSION 3.25)

project(
    StylizedRenderer
    VERSION 0.1.0
    DESCRIPTION "A maintainable and extensible stylized rendering framework"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/Dependencies.cmake)

add_library(stylized_glad STATIC
    third_party/glad/src/gl.c
)

target_include_directories(stylized_glad
    PUBLIC
        third_party/glad/include
)

set_target_properties(stylized_glad PROPERTIES
    FOLDER "Dependencies"
)

add_subdirectory(engine)
add_subdirectory(apps/viewer)

