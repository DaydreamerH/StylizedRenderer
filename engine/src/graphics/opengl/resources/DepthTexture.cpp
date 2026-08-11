#include <graphics/resources/DepthTexture.hpp>

#include <glad/gl.h>

#include <iostream>
#include <limits>
#include <utility>

namespace stylized::graphics
{

namespace
{

GLenum toInternalFormat(
    const DepthTextureFormat format) noexcept
{
    switch (format)
    {
    case DepthTextureFormat::Depth24Stencil8:
        return GL_DEPTH24_STENCIL8;

    case DepthTextureFormat::Depth32Float:
        return GL_DEPTH_COMPONENT32F;
    }

    return GL_DEPTH24_STENCIL8;
}

bool fitsGLsizei(
    const std::uint32_t value) noexcept
{
    return value <= static_cast<std::uint32_t>(
        std::numeric_limits<GLsizei>::max());
}

} // namespace

DepthTexture::DepthTexture(
    const DepthTextureDesc& desc)
    : extent_(desc.extent),
      format_(desc.format)
{
    if (desc.extent.width == 0 ||
        desc.extent.height == 0)
    {
        std::cerr << "Cannot create a zero-sized DepthTexture.\n";

        extent_ = {};
        return;
    }

    if (!fitsGLsizei(desc.extent.width) ||
        !fitsGLsizei(desc.extent.height))
    {
        std::cerr << "DepthTexture extent exceeds OpenGL range.\n";

        extent_ = {};
        return;
    }

    glCreateTextures(
        GL_TEXTURE_2D,
        1,
        &id_);

    if (id_ == 0)
    {
        std::cerr << "OpenGL failed to create DepthTexture.\n";

        extent_ = {};
        return;
    }

    glTextureStorage2D(
        id_,
        1,
        toInternalFormat(desc.format),
        static_cast<GLsizei>(desc.extent.width),
        static_cast<GLsizei>(desc.extent.height));

    const GLint textureFilter =
        desc.comparisonSampling
            ? GL_LINEAR
            : GL_NEAREST;

    glTextureParameteri(
        id_,
        GL_TEXTURE_MIN_FILTER,
        textureFilter);

    glTextureParameteri(
        id_,
        GL_TEXTURE_MAG_FILTER,
        textureFilter);

    glTextureParameteri(
        id_,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);

    glTextureParameteri(
        id_,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);

    glTextureParameteri(
        id_,
        GL_TEXTURE_COMPARE_MODE,
        desc.comparisonSampling
            ? GL_COMPARE_REF_TO_TEXTURE
            : GL_NONE);

    if (desc.comparisonSampling)
    {
        glTextureParameteri(
            id_,
            GL_TEXTURE_COMPARE_FUNC,
            GL_LEQUAL);
    }

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

DepthTexture::DepthTexture(
    DepthTexture&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      extent_(std::exchange(other.extent_, {})),
      format_(other.format_)
{
}

DepthTexture& DepthTexture::operator=(
    DepthTexture&& other) noexcept
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

DepthTexture::~DepthTexture()
{
    release();
}

bool DepthTexture::isValid() const noexcept
{
    return id_ != 0;
}

Extent2D DepthTexture::extent() const noexcept
{
    return extent_;
}

DepthTextureFormat DepthTexture::format() const noexcept
{
    return format_;
}

void DepthTexture::bind(
    const std::uint32_t slot) const noexcept
{
    if (id_ == 0)
    {
        return;
    }

    glBindTextureUnit(slot, id_);
}

void DepthTexture::release() noexcept
{
    if (id_ != 0)
    {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }

    extent_ = {};
}

} // namespace stylized::graphics
