#include <graphics/resources/Framebuffer.hpp>

#include <glad/gl.h>

#include <iostream>
#include <limits>
#include <utility>
#include <vector>
#include <algorithm>

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
    if (desc.colorTextures.empty() &&
        desc.depthTexture == nullptr)
    {
        std::cerr
            << "Framebuffer requires at least one attachment.\n";

        return;
    }

    GLint maximumColorAttachments = 0;
    GLint maximumDrawBuffers = 0;

    glGetIntegerv(
        GL_MAX_COLOR_ATTACHMENTS,
        &maximumColorAttachments
    );

    glGetIntegerv(
        GL_MAX_DRAW_BUFFERS,
        &maximumDrawBuffers
    );

    const std::size_t maximumColorCount =
        static_cast<std::size_t>(
            std::min(
                maximumColorAttachments,
                maximumDrawBuffers
            )
        );

    if (desc.colorTextures.size() > maximumColorCount)
    {
        std::cerr
            << "Framebuffer color attachment count "
            << "exceeds the OpenGL limit.\n";

        return;
    }

    for (const RenderTexture* colorTexture :
        desc.colorTextures)
    {
        if (colorTexture == nullptr ||
            !colorTexture->isValid())
        {
            std::cerr
                << "Cannot attach an invalid color texture.\n";

            extent_ = {};
            return;
        }

        const Extent2D colorExtent =
            colorTexture->extent();

        if (extent_.width == 0)
        {
            extent_ = colorExtent;
        }
        else if (extent_.width != colorExtent.width ||
                 extent_.height != colorExtent.height)
        {
            std::cerr
                << "Framebuffer attachment extents "
                << "do not match.\n";

            extent_ = {};
            return;
        }
    }

    if (desc.depthTexture != nullptr)
    {
        if (!desc.depthTexture->isValid())
        {
            std::cerr
                << "Cannot attach an invalid depth texture.\n";

            extent_ = {};
            return;
        }

        const Extent2D depthExtent =
            desc.depthTexture->extent();

        if (extent_.width == 0)
        {
            extent_ = depthExtent;
        }
        else if (extent_.width != depthExtent.width ||
                 extent_.height != depthExtent.height)
        {
            std::cerr
                << "Framebuffer attachment extents "
                << "do not match.\n";

            extent_ = {};
            return;
        }
    }

    glCreateFramebuffers(
        1,
        &id_);

    if (id_ == 0)
    {
        std::cerr
            << "OpenGL failed to create Framebuffer.\n";

        extent_ = {};
        return;
    }

    if (!desc.colorTextures.empty())
    {
        std::vector<GLenum> drawBuffers;
        drawBuffers.reserve(
            desc.colorTextures.size());

        for (std::size_t index = 0;
            index < desc.colorTextures.size();
            ++index)
        {
            const GLenum attachment =
                GL_COLOR_ATTACHMENT0 +
                static_cast<GLenum>(index);

            glNamedFramebufferTexture(
                id_,
                attachment,
                desc.colorTextures[index]->id_,
                0
            );

            drawBuffers.push_back(attachment);
        }

        glNamedFramebufferDrawBuffers(
            id_,
            static_cast<GLsizei>(
                drawBuffers.size()),
            drawBuffers.data());
    }
    else
    {
        glNamedFramebufferDrawBuffer(
            id_,
            GL_NONE);

        glNamedFramebufferReadBuffer(
            id_,
            GL_NONE);
    }

    if (desc.depthTexture != nullptr)
    {
        glNamedFramebufferTexture(
            id_,
            depthAttachment(
                desc.depthTexture->format()),
            desc.depthTexture->id_,
            0);
    }

    const GLenum status =
        glCheckNamedFramebufferStatus(
            id_,
            GL_FRAMEBUFFER);

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
