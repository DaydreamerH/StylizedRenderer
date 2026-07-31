#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

#include <assimp/texture.h>
#include <stb_image.h>

namespace stylized::asset::importers::detail
{

namespace
{

[[nodiscard]] bool decodeCompressedBytes(
    const std::byte* encodedData,
    const std::size_t encodedSize,
    const std::filesystem::path& sourcePath,
    const std::string& debugName,
    TextureAsset& textureAsset)
{
    if (encodedData == nullptr ||
        encodedSize == 0 ||
        encodedSize > static_cast<std::size_t>(INT_MAX))
    {
        return false;
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    stbi_uc* decoded = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(encodedData),
        static_cast<int>(encodedSize),
        &width,
        &height,
        &sourceChannels,
        STBI_rgb_alpha);

    if (decoded == nullptr || width <= 0 || height <= 0)
    {
        if (decoded != nullptr)
        {
            stbi_image_free(decoded);
        }

        return false;
    }

    const std::size_t widthValue =
        static_cast<std::size_t>(width);
    const std::size_t heightValue =
        static_cast<std::size_t>(height);

    if (widthValue >
        std::numeric_limits<std::size_t>::max() /
            heightValue / 4)
    {
        stbi_image_free(decoded);
        return false;
    }

    const std::size_t pixelByteCount =
        widthValue * heightValue * 4;

    textureAsset = {};
    textureAsset.width =
        static_cast<std::uint32_t>(width);
    textureAsset.height =
        static_cast<std::uint32_t>(height);
    textureAsset.format = TexturePixelFormat::RGBA8;
    textureAsset.colorSpace = ColorSpace::Srgb;
    textureAsset.sourcePath = sourcePath;
    textureAsset.debugName = debugName;

    const auto* firstPixel =
        reinterpret_cast<const std::byte*>(decoded);

    textureAsset.pixels.assign(
        firstPixel,
        firstPixel + pixelByteCount);

    stbi_image_free(decoded);
    return textureAsset.isValid();
}

} // namespace

bool decodeEmbeddedTexture(
    const aiTexture& sourceTexture,
    const std::filesystem::path& modelPath,
    TextureAsset& textureAsset)
{
    const std::string debugName =
        sourceTexture.mFilename.length > 0
            ? sourceTexture.mFilename.C_Str()
            : "Embedded Texture";

    if (sourceTexture.mHeight == 0)
    {
        return decodeCompressedBytes(
            reinterpret_cast<const std::byte*>(
                sourceTexture.pcData),
            sourceTexture.mWidth,
            modelPath,
            debugName,
            textureAsset);
    }

    if (sourceTexture.pcData == nullptr ||
        sourceTexture.mWidth == 0 ||
        sourceTexture.mHeight == 0)
    {
        return false;
    }

    textureAsset = {};
    textureAsset.width = sourceTexture.mWidth;
    textureAsset.height = sourceTexture.mHeight;
    textureAsset.format = TexturePixelFormat::RGBA8;
    textureAsset.colorSpace = ColorSpace::Srgb;
    textureAsset.sourcePath = modelPath;
    textureAsset.debugName = debugName;

    const std::size_t pixelCount =
        static_cast<std::size_t>(sourceTexture.mWidth) *
        static_cast<std::size_t>(sourceTexture.mHeight);

    textureAsset.pixels.resize(pixelCount * 4);

    for (std::size_t pixelIndex = 0;
         pixelIndex < pixelCount;
         ++pixelIndex)
    {
        const aiTexel& sourcePixel =
            sourceTexture.pcData[pixelIndex];

        const std::size_t destinationIndex =
            pixelIndex * 4;

        textureAsset.pixels[destinationIndex + 0] =
            static_cast<std::byte>(sourcePixel.r);
        textureAsset.pixels[destinationIndex + 1] =
            static_cast<std::byte>(sourcePixel.g);
        textureAsset.pixels[destinationIndex + 2] =
            static_cast<std::byte>(sourcePixel.b);
        textureAsset.pixels[destinationIndex + 3] =
            static_cast<std::byte>(sourcePixel.a);
    }

    return textureAsset.isValid();
}

bool decodeExternalTexture(
    const std::filesystem::path& texturePath,
    TextureAsset& textureAsset)
{
    std::ifstream stream{
        texturePath,
        std::ios::binary};

    if (!stream)
    {
        return false;
    }

    const std::vector<char> fileBytes{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};

    return decodeCompressedBytes(
        reinterpret_cast<const std::byte*>(
            fileBytes.data()),
        fileBytes.size(),
        texturePath,
        texturePath.filename().string(),
        textureAsset);
}

} // namespace stylized::asset::importers::detail
