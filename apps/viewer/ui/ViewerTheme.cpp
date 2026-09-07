#include "ViewerTheme.hpp"

#include <algorithm>
#include <iostream>

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace stylized::viewer::ui
{

namespace
{

constexpr float baseFontSize = 18.0F;

[[nodiscard]] ImVec4 color(
    const int red,
    const int green,
    const int blue,
    const int alpha = 255) noexcept
{
    constexpr float byteToFloat =
        1.0F / 255.0F;

    return {
        static_cast<float>(red) * byteToFloat,
        static_cast<float>(green) * byteToFloat,
        static_cast<float>(blue) * byteToFloat,
        static_cast<float>(alpha) * byteToFloat
    };
}

[[nodiscard]] float contentScale(
    GLFWwindow* window) noexcept
{
    float horizontalScale = 1.0F;
    float verticalScale = 1.0F;

    glfwGetWindowContentScale(
        window,
        &horizontalScale,
        &verticalScale);

    return std::clamp(
        0.5F *
            (horizontalScale + verticalScale),
        1.0F,
        2.5F);
}

void applyColors(
    ImGuiStyle& style) noexcept
{
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = color(230, 232, 236);
    colors[ImGuiCol_TextDisabled] = color(143, 149, 159);

    colors[ImGuiCol_WindowBg] = color(24, 26, 31, 248);
    colors[ImGuiCol_ChildBg] = color(24, 26, 31, 0);
    colors[ImGuiCol_PopupBg] = color(31, 34, 40, 252);

    colors[ImGuiCol_Border] = color(58, 63, 73);
    colors[ImGuiCol_BorderShadow] = color(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg] = color(42, 45, 53);
    colors[ImGuiCol_FrameBgHovered] = color(53, 57, 66);
    colors[ImGuiCol_FrameBgActive] = color(62, 66, 76);

    colors[ImGuiCol_TitleBg] = color(24, 26, 31);
    colors[ImGuiCol_TitleBgActive] = color(32, 35, 41);
    colors[ImGuiCol_TitleBgCollapsed] = color(24, 26, 31);
    colors[ImGuiCol_MenuBarBg] = color(32, 35, 41);

    colors[ImGuiCol_ScrollbarBg] = color(24, 26, 31);
    colors[ImGuiCol_ScrollbarGrab] = color(63, 67, 77);
    colors[ImGuiCol_ScrollbarGrabHovered] = color(80, 84, 94);
    colors[ImGuiCol_ScrollbarGrabActive] = color(99, 103, 113);

    colors[ImGuiCol_CheckMark] = color(208, 164, 92);
    colors[ImGuiCol_SliderGrab] = color(190, 149, 82);
    colors[ImGuiCol_SliderGrabActive] = color(221, 178, 105);

    colors[ImGuiCol_Button] = color(57, 53, 46);
    colors[ImGuiCol_ButtonHovered] = color(84, 68, 47);
    colors[ImGuiCol_ButtonActive] = color(106, 83, 52);

    colors[ImGuiCol_Header] = color(55, 51, 45);
    colors[ImGuiCol_HeaderHovered] = color(79, 65, 47);
    colors[ImGuiCol_HeaderActive] = color(101, 80, 52);

    colors[ImGuiCol_Separator] = color(58, 63, 73);
    colors[ImGuiCol_SeparatorHovered] = color(181, 141, 77);
    colors[ImGuiCol_SeparatorActive] = color(208, 164, 92);

    colors[ImGuiCol_ResizeGrip] = color(190, 149, 82, 64);
    colors[ImGuiCol_ResizeGripHovered] = color(208, 164, 92, 170);
    colors[ImGuiCol_ResizeGripActive] = color(221, 178, 105, 230);

    colors[ImGuiCol_Tab] = color(37, 40, 47);
    colors[ImGuiCol_TabHovered] = color(76, 63, 47);
    colors[ImGuiCol_TabSelected] = color(91, 72, 47);
    colors[ImGuiCol_TabSelectedOverline] = color(208, 164, 92);
    colors[ImGuiCol_TabDimmed] = color(31, 34, 40);
    colors[ImGuiCol_TabDimmedSelected] = color(55, 51, 45);
    colors[ImGuiCol_TabDimmedSelectedOverline] = color(150, 119, 72);

    colors[ImGuiCol_TableHeaderBg] = color(42, 45, 53);
    colors[ImGuiCol_TableBorderStrong] = color(64, 69, 79);
    colors[ImGuiCol_TableBorderLight] = color(49, 53, 61);
    colors[ImGuiCol_TableRowBg] = color(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt] = color(255, 255, 255, 7);

    colors[ImGuiCol_TextLink] = color(221, 178, 105);
    colors[ImGuiCol_TextSelectedBg] = color(150, 113, 61, 110);
    colors[ImGuiCol_DragDropTarget] = color(221, 178, 105);
    colors[ImGuiCol_NavCursor] = color(221, 178, 105);

    colors[ImGuiCol_NavWindowingHighlight] = color(230, 232, 236, 180);
    colors[ImGuiCol_NavWindowingDimBg] = color(10, 11, 13, 120);
    colors[ImGuiCol_ModalWindowDimBg] = color(10, 11, 13, 160);
}

} // namespace

bool applyViewerTheme(
    GLFWwindow* window)
{
    if (window == nullptr)
    {
        return false;
    }

    const float scale =
        contentScale(window);

    ImGuiIO& io = ImGui::GetIO();

    ImFont* font =
        io.Fonts->AddFontFromFileTTF(
            STYLIZED_VIEWER_FONT_PATH,
            baseFontSize * scale,
            nullptr,
            io.Fonts->GetGlyphRangesDefault());

    if (font == nullptr)
    {
        ImFontConfig fallbackConfig;
        fallbackConfig.SizePixels =
            baseFontSize * scale;

        font = io.Fonts->AddFontDefault(
            &fallbackConfig);
    }

#ifdef STYLIZED_VIEWER_CJK_FONT_PATH
    if (font != nullptr)
    {
        ImFontConfig cjkConfig;
        cjkConfig.MergeMode = true;
        cjkConfig.PixelSnapH = false;
        cjkConfig.GlyphMinAdvanceX = 0.0F;

        if (io.Fonts->AddFontFromFileTTF(
                STYLIZED_VIEWER_CJK_FONT_PATH,
                baseFontSize * scale,
                &cjkConfig,
                io.Fonts->GetGlyphRangesChineseFull()) == nullptr)
        {
            std::cerr
                << "Failed to load Viewer CJK font: "
                << STYLIZED_VIEWER_CJK_FONT_PATH
                << '\n';
        }
    }
#endif

    if (font == nullptr)
    {
        return false;
    }

    io.FontDefault = font;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = {12.0F, 12.0F};
    style.FramePadding = {8.0F, 5.0F};
    style.CellPadding = {8.0F, 5.0F};
    style.ItemSpacing = {8.0F, 7.0F};
    style.ItemInnerSpacing = {6.0F, 5.0F};
    style.TouchExtraPadding = {0.0F, 0.0F};

    style.IndentSpacing = 18.0F;
    style.ScrollbarSize = 15.0F;
    style.GrabMinSize = 12.0F;

    style.WindowBorderSize = 0.0F;
    style.ChildBorderSize = 1.0F;
    style.PopupBorderSize = 1.0F;
    style.FrameBorderSize = 0.0F;
    style.TabBorderSize = 0.0F;

    style.WindowRounding = 0.0F;
    style.ChildRounding = 4.0F;
    style.FrameRounding = 4.0F;
    style.PopupRounding = 4.0F;
    style.ScrollbarRounding = 8.0F;
    style.GrabRounding = 4.0F;
    style.TabRounding = 4.0F;

    style.ScaleAllSizes(scale);

    applyColors(style);

    return true;
}

} // namespace stylized::viewer::ui
