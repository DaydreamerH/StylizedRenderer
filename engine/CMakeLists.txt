add_library(stylized_engine STATIC
    include/stylized/core/Application.hpp
    include/stylized/core/NonCopyable.hpp
    include/stylized/graphics/Buffer.hpp
    include/stylized/graphics/GraphicsCommands.hpp
    include/stylized/graphics/GraphicsDevice.hpp
    include/stylized/graphics/GraphicsTypes.hpp
    include/stylized/graphics/OpenGLContext.hpp
    include/stylized/graphics/ShaderProgram.hpp
    include/stylized/graphics/Texture2D.hpp
    include/stylized/graphics/VertexArray.hpp
    include/stylized/math/Bounds.hpp
    include/stylized/platform/Window.hpp
    include/stylized/scene/Camera.hpp
    include/stylized/scene/Transform.hpp
    src/core/Application.cpp
    src/graphics/opengl/Buffer.cpp
    src/graphics/opengl/GraphicsDevice.cpp
    src/graphics/opengl/OpenGLContext.cpp
    src/graphics/opengl/ShaderProgram.cpp
    src/graphics/opengl/Texture2D.cpp
    src/graphics/opengl/VertexArray.cpp
    src/math/Bounds.cpp
    src/platform/Window.cpp
    src/scene/Camera.cpp
    src/scene/Transform.cpp
)

target_include_directories(stylized_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(stylized_engine
    PUBLIC
        glm::glm
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
