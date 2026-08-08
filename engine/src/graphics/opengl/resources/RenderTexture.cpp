#include <graphics/resources/RenderTexture.hpp>

#include <glad/gl.h>

#include <iostream>
#include <limits>
#include <utility>

namespace stylized::graphics
{
    
namespace
{
    
GLenum toInternalFormat(const RenderTextureFormat format) noexcept
{
    switch (format)
    {
    case RenderTextureFormat::RGBA8:
        return GL_RGBA8;
    
    case RenderTextureFormat::RGBA16Float:
        return GL_RGBA16F;
    }

    return GL_RGBA16F;
}

bool fitsGLsizei(const std::uint32_t value) noexcept
{
    return value <= static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max());
}

} // namespace

RenderTexture::RenderTexture(const RenderTextureDesc& desc)
    : extent_(desc.extent), format_(desc.format)
{
    if (desc.extent.width == 0 || desc.extent.height == 0)
    {
        std::cerr << "Cannot create a zero-sized RenderTexture.\n";

        extent_ = {};
        return;
    }

    if (!fitsGLsizei(desc.extent.width) || !fitsGLsizei(desc.extent.height))
    {
        std::cerr << "RenderTexture extent exceeds OpenGL range.\n";

        extent_ = {};
        return;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &id_);

    if (id_ == 0)
    {
        std::cerr << "OpenGL failed to create RenderTexture.\n";
        extent_ = {};

        return;
    }

    glTextureStorage2D(
        id_,
        1,
        toInternalFormat(desc.format),
        static_cast<GLsizei>(desc.extent.width),
        static_cast<GLsizei>(desc.extent.height));

    glTextureParameteri(
        id_,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR);

    glTextureParameteri(
        id_,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR);

    glTextureParameteri(
        id_,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);

    glTextureParameteri(
        id_,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);

#ifndef NDEBUG
    if (!desc.debugName.empty() &&
        desc.debugName.size() <=
            static_cast<std::size_t>(
                std::numeric_limits<GLsizei>::max()))
    {
        glObjectLabel(
            GL_TEXTURE,
            id_,
            static_cast<GLsizei>(
                desc.debugName.size()),
            desc.debugName.c_str());
    }
#endif
}

RenderTexture::RenderTexture(RenderTexture&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      extent_(std::exchange(other.extent_, {})),
      format_(other.format_)
{
}

RenderTexture& RenderTexture::operator=(RenderTexture&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();

    id_ = std::exchange(other.id_, 0);
    extent_ = std::exchange(other.extent_, {});
    format_ = other.format_;

    return *this;
}

RenderTexture::~RenderTexture()
{
    release();
}

bool RenderTexture::isValid() const noexcept
{
    return id_ != 0;
}

Extent2D RenderTexture::extent() const noexcept
{
    return extent_;
}

RenderTextureFormat RenderTexture::format() const noexcept
{
    return format_;
}

void RenderTexture::bind(const std::uint32_t slot) const noexcept
{
    if (id_ == 0) return;

    glBindTextureUnit(slot, id_);
}

void RenderTexture::release() noexcept
{
    if (id_ != 0)
    {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }

    extent_ = {};
}

} // namespace stylized::graphics
