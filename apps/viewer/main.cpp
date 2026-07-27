#include <stylized/core/Application.hpp>
#include <stylized/graphics/Buffer.hpp>
#include <stylized/graphics/GraphicsCommands.hpp>
#include <stylized/graphics/GraphicsDevice.hpp>
#include <stylized/graphics/ShaderProgram.hpp>
#include <stylized/graphics/VertexArray.hpp>
#include <stylized/platform/Window.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string_view>

namespace
{

stylized::core::ApplicationDesc makeApplicationDesc(const bool smokeTest)
{
    stylized::core::ApplicationDesc desc;
    desc.title = "StylizedRenderer";
    desc.width = 1280;
    desc.height = 720;
    desc.visible = !smokeTest;
    desc.vsync = !smokeTest;
    return desc;
}

struct Vertex 
{
    float position[3];
    float uv[2];
};

class ViewerApplication final : public stylized::core::Application
{
public:
    explicit ViewerApplication(const bool smokeTest)
        : Application(makeApplicationDesc(smokeTest)),
          smokeTest_(smokeTest)
    {
    }

protected:
    bool onInit() override
    {
        constexpr std::array<Vertex, 4> vertices {
            Vertex{
                {-0.5F, -0.5F, 0.0F},
                {0.0F, 0.0F}
            },
            Vertex{
                {0.5F, -0.5F, 0.0F},
                {1.0F, 0.0F}
            },
            Vertex{
                {0.5F, 0.5F, 0.0F},
                {1.0F, 1.0F}
            },
            Vertex{
                {-0.5F, 0.5F, 0.0F},
                {0.0F, 1.0F}
            }
        };

        constexpr std::array<uint32_t, 6> indices{0, 1, 2, 2, 3, 0};

        if (!createBuffers(vertices, indices)) return false;

        if (!createVertexArray()) return false;

        if (!createShader()) return false;

        return true;
    }

    void onUpdate(float) override
    {
        if (window().isKeyPressed(stylized::platform::Key::Escape))
        {
            requestExit();
        }
    }

    void onRender() override
    {
        graphicsDevice().clear({0.06F, 0.07F, 0.10F, 1.0F});

        stylized::graphics::DrawIndexedCommand command;
        command.shader = &shaderProgram_;
        command.vertexArray = &vertexArray_;
        command.topology = stylized::graphics::PrimitiveTopology::Triangles;
        command.indexType = stylized::graphics::IndexType::Uint32;
        command.indexCount = 6;
        command.firstIndex = 0;

        graphicsDevice().drawIndexed(command);

        ++renderedFrameCount_;

        if (smokeTest_ && renderedFrameCount_ >= 3)
        {
            requestExit();
        }
    }

private:
    bool smokeTest_ = false;
    int renderedFrameCount_ = 0;

    stylized::graphics::Buffer vertexBuffer_;
    stylized::graphics::Buffer indexBuffer_;
    stylized::graphics::VertexArray vertexArray_;
    stylized::graphics::ShaderProgram shaderProgram_;

    bool createBuffers(
        std::span<const Vertex> vertices,
        std::span<const uint32_t> indices)
    {
        stylized::graphics::BufferDesc vertexBufferDesc;
        vertexBufferDesc.usage = stylized::graphics::BufferUsage::Static;
        vertexBufferDesc.debugName = "Foundation Vertex Buffer";

        vertexBuffer_ = graphicsDevice().createBuffer(vertexBufferDesc, vertices);

        if (!vertexBuffer_.isValid()) return false;

        stylized::graphics::BufferDesc indexBufferDesc;
        indexBufferDesc.usage = stylized::graphics::BufferUsage::Static;
        indexBufferDesc.debugName = "Foundation Index Buffer";

        indexBuffer_ = graphicsDevice().createBuffer(indexBufferDesc, indices);

        return indexBuffer_.isValid();
    }

    bool createVertexArray()
    {
        std::array<stylized::graphics::VertexAttributeDesc, 2> vertexAttributeDescs;
        vertexAttributeDescs[0] = {
            stylized::graphics::VertexAttributeDesc{.location = 0, .binding = 0, .format = stylized::graphics::VertexAttributeFormat::Float3, .offset = offsetof(Vertex, position)}
        }; 
        vertexAttributeDescs[1] = {
            stylized::graphics::VertexAttributeDesc{.location = 1, .binding = 0, .format = stylized::graphics::VertexAttributeFormat::Float2, .offset = offsetof(Vertex, uv)}
        };

        stylized::graphics::VertexArrayDesc vertexArrayDesc;
        vertexArrayDesc.vertexBuffer = &vertexBuffer_;
        vertexArrayDesc.indexBuffer = &indexBuffer_;
        vertexArrayDesc.vertexBinding.binding = 0;
        vertexArrayDesc.vertexBinding.stride = sizeof(Vertex);
        vertexArrayDesc.vertexBufferOffset = 0;
        vertexArrayDesc.attributes = std::span<const stylized::graphics::VertexAttributeDesc>{vertexAttributeDescs};
        vertexArrayDesc.debugName = "Foundation Vertex Array";
        vertexArray_ = graphicsDevice().createVertexArray(vertexArrayDesc);

        return vertexArray_.isValid();
    }

    bool createShader()
    {
        stylized::graphics::ShaderProgramDesc desc;
        desc.vertexShaderPath = "assets/shaders/foundation/foundation.vert";
        desc.fragmentShaderPath = "assets/shaders/foundation/foundation.frag";
        desc.debugName = "Foundation Shader";

        shaderProgram_ = graphicsDevice().createShaderProgram(desc);

        if (!shaderProgram_.isValid()) return false;
        
        return shaderProgram_.setVec4(
            "uTint",
            1.0F,
            1.0F,
            1.0F,
            1.0F);
    }
};

} // namespace

int main(const int argc, char* argv[])
{
    const bool smokeTest =
        argc > 1 &&
        std::string_view{argv[1]} == "--smoke-test";

    ViewerApplication application(smokeTest);
    return application.run();
}
