add_library(stylized_engine STATIC
    include/asset/AssetHandle.hpp
    include/asset/AssetRegistry.hpp
    include/asset/MaterialAsset.hpp
    include/asset/MeshAsset.hpp
    include/asset/SceneAsset.hpp
    include/asset/TextureAsset.hpp
    include/core/Application.hpp
    include/core/NonCopyable.hpp
    include/graphics/Buffer.hpp
    include/graphics/GraphicsCommands.hpp
    include/graphics/GraphicsDevice.hpp
    include/graphics/GraphicsTypes.hpp
    include/graphics/OpenGLContext.hpp
    include/graphics/ShaderProgram.hpp
    include/graphics/Texture2D.hpp
    include/graphics/VertexArray.hpp
    include/math/Bounds.hpp
    include/platform/Window.hpp
    include/scene/Camera.hpp
    include/scene/Entity.hpp
    include/scene/Scene.hpp
    include/scene/Transform.hpp
    src/asset/MeshAsset.cpp
    src/asset/SceneAsset.cpp
    src/asset/TextureAsset.cpp
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
    src/scene/Scene.cpp
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
        assimp::assimp
        glfw
        stylized_glad
        stylized_stb
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
