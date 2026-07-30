#include <stylized/asset/TextureAsset.hpp>

#include <limits>

namespace stylized::asset
{

std::size_t TextureAsset::bytesPerPixel() const noexcept
{
    switch (format)
    {
    case TexturePixelFormat::R8:
        return 1;
    case TexturePixelFormat::RG8:
        return 2;
    case TexturePixelFormat::RGB8:
        return 3;
    case TexturePixelFormat::RGBA8:
        return 4;
    }
    return 0;
}

std::size_t TextureAsset::expectedDataSize() const noexcept
{
    if (width == 0 || height == 0) return 0;

    const std::size_t pixelSize = bytesPerPixel();
    if (pixelSize == 0) return 0;

    const std::size_t widthValue = static_cast<std::size_t>(width);
    const std::size_t heightValue = static_cast<std::size_t>(height);

    constexpr std::size_t maximumSize = std::numeric_limits<std::size_t>::max();

    if (widthValue > maximumSize / heightValue)
    {
        return 0;
    }

    const std::size_t pixelCount = widthValue * heightValue;
    if (pixelCount > maximumSize / pixelSize)
    {
        return 0;
    }

    return pixelCount * pixelSize;
}

bool TextureAsset::isValid() const noexcept
{
    const std::size_t expectedSize = expectedDataSize();

    return expectedSize > 0 && pixels.size() == expectedSize;
}

}