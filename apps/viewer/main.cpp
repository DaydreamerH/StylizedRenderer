#include <stylized/core/Application.hpp>
#include <stylized/graphics/GraphicsDevice.hpp>
#include <stylized/platform/Window.hpp>

#include <cstdlib>
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

class ViewerApplication final : public stylized::core::Application
{
public:
    explicit ViewerApplication(const bool smokeTest)
        : Application(makeApplicationDesc(smokeTest)),
          smokeTest_(smokeTest)
    {
    }

protected:
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

        ++renderedFrameCount_;
        if (smokeTest_ && renderedFrameCount_ >= 3)
        {
            requestExit();
        }
    }

private:
    bool smokeTest_ = false;
    int renderedFrameCount_ = 0;
};

} // namespace

int main(const int argc, char* argv[])
{
    const bool smokeTest =
        argc > 1 && std::string_view{argv[1]} == "--smoke-test";

    ViewerApplication application(smokeTest);
    return application.run();
}
