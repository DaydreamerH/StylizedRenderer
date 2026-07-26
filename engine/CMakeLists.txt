add_library(stylized_engine STATIC
    include/stylized/core/Application.hpp
    include/stylized/core/NonCopyable.hpp
    include/stylized/graphics/Buffer.hpp
    include/stylized/graphics/GraphicsDevice.hpp
    include/stylized/graphics/GraphicsTypes.hpp
    include/stylized/graphics/OpenGLContext.hpp
    include/stylized/graphics/VertexArray.hpp
    include/stylized/platform/Window.hpp
    src/core/Application.cpp
    src/graphics/opengl/Buffer.cpp
    src/graphics/opengl/GraphicsDevice.cpp
    src/graphics/opengl/OpenGLContext.cpp
    src/graphics/opengl/VertexArray.cpp
    src/platform/Window.cpp
)

target_include_directories(stylized_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(stylized_engine
    PRIVATE
        glfw
        stylized_glad
)

target_compile_definitions(stylized_engine
    PRIVATE
        GLFW_INCLUDE_NONE
)

if(MSVC)
    target_compile_options(stylized_engine PRIVATE
        /W4
        /permissive-
        /utf-8
    )
else()
    target_compile_options(stylized_engine PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

set_target_properties(stylized_engine PROPERTIES
    FOLDER "Engine"
)
