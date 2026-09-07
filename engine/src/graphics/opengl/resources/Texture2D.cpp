#include <graphics/resources/Texture2D.hpp>

#include <glad/gl.h>

#include <algorithm>
#include <bit>
#include <iostream>
#include <limits>
#include <utility>

namespace stylized::graphics
{

namespace
{

struct OpenGLTextureFormat
{
    GLenum internalFormat = GL_RGBA8;
    GLenum externalFormat = GL_RGBA;
    GLenum pixelType = GL_UNSIGNED_BYTE;
    std::size_t bytesPerPixel = 4;
};

OpenGLTextureFormat toOpenGLFormat(
    const TextureFormat format) noexcept
{
    switch (format)
    {
    case TextureFormat::R8:
        return {
            GL_R8,
            GL_RED,
            GL_UNSIGNED_BYTE,
            1
        };

    case TextureFormat::RG8:
        return {
            GL_RG8,
            GL_RG,
            GL_UNSIGNED_BYTE,
            2
        };

    case TextureFormat::RGB8:
        return {
            GL_RGB8,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            3
        };

    case TextureFormat::RGBA8:
        return {
            GL_RGBA8,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            4
        };

    case TextureFormat::SRGB8:
        return {
            GL_SRGB8,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            3
        };

    case TextureFormat::SRGBA8:
        return {
            GL_SRGB8_ALPHA8,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            4
        };
    }

    return {
        GL_RGBA8,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        4
    };
}

GLint toOpenGLWrap(
    const TextureWrap wrap) noexcept
{
    switch (wrap)
    {
    case TextureWrap::Repeat:
        return GL_REPEAT;

    case TextureWrap::ClampToEdge:
        return GL_CLAMP_TO_EDGE;
    }

    return GL_REPEAT;
}

GLint toOpenGLFilter(
    const TextureFilter filter) noexcept
{
    switch (filter)
    {
    case TextureFilter::Nearest:
        return GL_NEAREST;

    case TextureFilter::Linear:
        return GL_LINEAR;
    }

    return GL_LINEAR;
}

GLint toOpenGLMipmapFilter(
    const TextureFilter filter) noexcept
{
    return filter == TextureFilter::Nearest
        ? GL_NEAREST_MIPMAP_NEAREST
        : GL_LINEAR_MIPMAP_LINEAR;
}

bool fitsGLsizei(const uint32_t value) noexcept
{
    return value <= static_cast<uint32_t>(
        std::numeric_limits<GLsizei>::max());
}

bool calculatePixelDataSize(
    const Texture2DDesc& desc,
    const std::size_t bytesPerPixel,
    std::size_t& result) noexcept
{
    if (desc.width == 0 ||
        desc.height == 0 ||
        bytesPerPixel == 0)
    {
        return false;
    }

    const std::size_t width =
        static_cast<std::size_t>(desc.width);

    const std::size_t height =
        static_cast<std::size_t>(desc.height);

    const std::size_t maximum =
        std::numeric_limits<std::size_t>::max();

    if (height > maximum / width)
    {
        return false;
    }

    const std::size_t pixelCount =
        width * height;

    if (bytesPerPixel > maximum / pixelCount)
    {
        return false;
    }

    result = pixelCount * bytesPerPixel;
    return true;
}

void setTextureLabel(
    const GLuint texture,
    const std::string& label)
{
#ifndef NDEBUG
    if (texture == 0 || label.empty())
    {
        return;
    }

    if (label.size() >
        static_cast<std::size_t>(
            std::numeric_limits<GLsizei>::max()))
    {
        std::cerr
            << "Texture debug name exceeds "
            << "the OpenGL size range.\n";

        return;
    }

    glObjectLabel(
        GL_TEXTURE,
        texture,
        static_cast<GLsizei>(label.size()),
        label.c_str());
#else
    (void)texture;
    (void)label;
#endif
}

} // namespace

Texture2D::Texture2D(const Texture2DDesc& desc, const std::span<const std::byte> pixels)
    :width_(desc.width), height_(desc.height), format_(desc.format)
{
    if (desc.width == 0 || desc.height == 0)
    {
        std::cerr
            << "Cannot create a zero-sized Texture2D.\n";

        width_ = 0;
        height_ = 0;
        return;
    }

    if (!fitsGLsizei(desc.width) ||
        !fitsGLsizei(desc.height))
    {
        std::cerr
            << "Texture2D dimensions exceed "
            << "the OpenGL size range.\n";

        width_ = 0;
        height_ = 0;
        return;
    }

    const OpenGLTextureFormat glFormat = toOpenGLFormat(desc.format);

    std::size_t expectedDataSize = 0;

    if (!calculatePixelDataSize(desc, glFormat.bytesPerPixel, expectedDataSize))
    {
        std::cerr << "Texture2D pixel data size overflow.\n";

        width_ = 0;
        height_ = 0;
        return;
    }

    if (pixels.size_bytes() != expectedDataSize)
    {
        std::cerr
            << "Texture2D pixel data size mismatch. "
            << "Expected "
            << expectedDataSize
            << " bytes, received "
            << pixels.size_bytes()
            << " bytes.\n";

        width_ = 0;
        height_ = 0;
        return;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &id_);

    if (id_ == 0)
    {
        std::cerr << "OpenGL failed to create a Texture2D.\n";

        width_ = 0;
        height_ = 0;
        return;
    }

    const GLsizei mipLevelCount = desc.generateMipmaps
        ? static_cast<GLsizei>(std::bit_width(
            std::max(desc.width, desc.height)))
        : 1;

    glTextureStorage2D(
        id_,
        mipLevelCount,
        glFormat.internalFormat,
        static_cast<GLsizei>(desc.width),
        static_cast<GLsizei>(desc.height));
    glTextureParameteri(
        id_,
        GL_TEXTURE_WRAP_S,
        toOpenGLWrap(desc.wrapU));
    glTextureParameteri(
        id_,
        GL_TEXTURE_WRAP_T,
        toOpenGLWrap(desc.wrapV));
    glTextureParameteri(
        id_,
        GL_TEXTURE_MIN_FILTER,
        desc.generateMipmaps
            ? toOpenGLMipmapFilter(desc.minFilter)
            : toOpenGLFilter(desc.minFilter));
    glTextureParameteri(
        id_,
        GL_TEXTURE_MAG_FILTER,
        toOpenGLFilter(desc.magFilter));

    GLint previousUnpackAlignment = 4;
    glGetIntegerv(
        GL_UNPACK_ALIGNMENT,
        &previousUnpackAlignment);
    glPixelStorei(
        GL_UNPACK_ALIGNMENT,
        1);
    glTextureSubImage2D(
        id_,
        0,
        0,
        0,
        static_cast<GLsizei>(desc.width),
        static_cast<GLsizei>(desc.height),
        glFormat.externalFormat,
        glFormat.pixelType,
        pixels.data());
    glPixelStorei(
        GL_UNPACK_ALIGNMENT,
        previousUnpackAlignment);

    if (desc.generateMipmaps)
    {
        glGenerateTextureMipmap(id_);
    }

    setTextureLabel(
        id_,
        desc.debugName);

}

Texture2D::Texture2D(
    Texture2D&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)),
      format_(other.format_)
{
}

Texture2D& Texture2D::operator=(
    Texture2D&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();

    id_ = std::exchange(other.id_, 0);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    format_ = other.format_;

    return *this;
}

Texture2D::~Texture2D()
{
    release();
}

bool Texture2D::isValid() const noexcept
{
    return id_ != 0;
}

uint32_t Texture2D::width() const noexcept
{
    return width_;
}

uint32_t Texture2D::height() const noexcept
{
    return height_;
}

TextureFormat Texture2D::format() const noexcept
{
    return format_;
}

void Texture2D::bind(
    const uint32_t slot) const noexcept
{
    if (id_ == 0)
    {
        return;
    }

    glBindTextureUnit(
        slot,
        id_);
}

void Texture2D::release() noexcept
{
    if (id_ != 0)
    {
        glDeleteTextures(
            1,
            &id_);

        id_ = 0;
    }

    width_ = 0;
    height_ = 0;
}

}
