#include <graphics/Framebuffer.hpp>

#include <glad/gl.h>

#include <iostream>
#include <limits>
#include <utility>

namespace stylized::graphics
{
    
namespace
{

GLenum depthAttachment(const DepthTextureFormat format) noexcept
{
    switch (format)
    {
    case DepthTextureFormat::Depth24Stencil8:
        return GL_DEPTH_STENCIL_ATTACHMENT;

    case DepthTextureFormat::Depth32Float:
        return GL_DEPTH_ATTACHMENT;
    }

    return GL_DEPTH_ATTACHMENT;
}

} // namespace

Framebuffer::Framebuffer(const FramebufferDesc& desc)
{
    if (desc.colorTexture == nullptr &&
        desc.depthTexture == nullptr)
    {
        std::cerr << "Framebuffer requires at least one attachment.\n";
        return;
    }

    if (desc.colorTexture != nullptr)
    {
        if (!desc.colorTexture->isValid())
        {
            std::cerr << "Cannot attach an invalid color texture.\n";
            return;
        }
        extent_ = desc.colorTexture->extent();
    }

    if (desc.depthTexture != nullptr)
    {
        if (!desc.depthTexture->isValid())
        {
            std::cerr << "Cannot attach an invalid depth texture.\n";
            extent_ = {};
            return;
        }
        if (extent_.width == 0)
        {
            extent_ = desc.depthTexture->extent();
        }
        else if (extent_.width != desc.depthTexture->extent().width ||
                 extent_.height != desc.depthTexture->extent().height)
        {
            std::cerr << "Framebuffer attachment extents do not match.\n";
            extent_ = {};
            return;
        }
    }

    glCreateFramebuffers(1, &id_);

    if (id_ == 0)
    {
        std::cerr << "OpenGL failed to create Framebuffer.\n";

        extent_ = {};
        return;
    }

    if (desc.colorTexture != nullptr)
    {
        glNamedFramebufferTexture(id_, GL_COLOR_ATTACHMENT0, desc.colorTexture->id_, 0);

        const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;

        glNamedFramebufferDrawBuffers(id_, 1, &drawBuffer);
    }
    else
    {
        glNamedFramebufferDrawBuffer(id_, GL_NONE);
        glNamedFramebufferReadBuffer(id_, GL_NONE);
    }

    if (desc.depthTexture != nullptr)
    {
        glNamedFramebufferTexture(id_, depthAttachment(desc.depthTexture->format()), desc.depthTexture->id_, 0);
    }

    const GLenum status = glCheckNamedFramebufferStatus(id_, GL_FRAMEBUFFER);
    
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr
            << "Framebuffer is incomplete. Status: 0x"
            << std::hex
            << status
            << std::dec
            << '\n';
        
        release();
        extent_ = {};
        return;
    }

#ifndef NDEBUG
    if (!desc.debugName.empty() &&
        desc.debugName.size() <=
            static_cast<std::size_t>(
                std::numeric_limits<GLsizei>::max()))
    {
        glObjectLabel(
            GL_FRAMEBUFFER,
            id_,
            static_cast<GLsizei>(
                desc.debugName.size()),
            desc.debugName.c_str());
    }
#endif
}

Framebuffer::Framebuffer(
    Framebuffer&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      extent_(std::exchange(other.extent_, {}))
{
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();

    id_ = std::exchange(other.id_, 0);
    extent_ = std::exchange(other.extent_, {});

    return *this;
}

Framebuffer::~Framebuffer()
{
    release();
}

bool Framebuffer::isValid() const noexcept
{
    return id_ != 0;
}

Extent2D Framebuffer::extent() const noexcept
{
    return extent_;
}

void Framebuffer::release() noexcept
{
    if (id_ != 0)
    {
        glDeleteFramebuffers(1, &id_);
        id_ = 0;
    }

    extent_ = {};
}

} // namespace stylized::graphics
