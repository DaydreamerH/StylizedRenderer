#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace stylized::asset
{

enum class TexturePixelFormat : std::uint8_t
{
    R8,
    RG8,
    RGB8,
    RGBA8
};

enum class ColorSpace : std::uint8_t
{
    Linear,
    Srgb
};

struct TextureAsset
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    TexturePixelFormat format = TexturePixelFormat::RGBA8;
    ColorSpace colorSpace = ColorSpace::Linear;

    std::vector<std::byte> pixels;

    std::filesystem::path sourcePath;
    std::string debugName;

    [[nodiscard]] std::size_t bytesPerPixel() const noexcept;
    [[nodiscard]] std::size_t expectedDataSize() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
};


} // namespace stylized::asset
