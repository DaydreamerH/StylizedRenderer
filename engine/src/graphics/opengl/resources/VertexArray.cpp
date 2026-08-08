#include <graphics/resources/VertexArray.hpp>

#include <glad/gl.h>

#include <iostream>
#include <limits>
#include <utility>

namespace stylized::graphics
{

namespace
{

struct OpenGLAttributeFormat
{
    GLint componentCount = 0;
    GLenum componentType = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
};

OpenGLAttributeFormat toOpenGLFormat(const VertexAttributeFormat format) noexcept
{
    switch (format)
    {
    case VertexAttributeFormat::Float:
        return {1, GL_FLOAT, GL_FALSE};
    
    case VertexAttributeFormat::Float2:
        return {2, GL_FLOAT, GL_FALSE};
    
    case VertexAttributeFormat::Float3:
        return {3, GL_FLOAT, GL_FALSE};

    case VertexAttributeFormat::Float4:
        return {4, GL_FLOAT, GL_FALSE};
    
    case VertexAttributeFormat::Uint8Normalized4:
        return {4, GL_UNSIGNED_BYTE, GL_TRUE};
    }

    return {
        0,
        GL_FLOAT,
        GL_FALSE
    };
}

bool fitsGLintptr(const std::size_t value) noexcept
{
    return value <= static_cast<std::size_t>(
        std::numeric_limits<GLintptr>::max());
}

bool fitsGLsizei(const std::size_t value) noexcept
{
    return value <= static_cast<std::size_t>(
        std::numeric_limits<GLsizei>::max());
}

bool fitsGLuint(const std::size_t value) noexcept
{
    return value <= static_cast<std::size_t>(
        std::numeric_limits<GLuint>::max());
}

} // namespace

VertexArray::VertexArray(const VertexArrayDesc& desc)
{
    if (desc.vertexBuffer == nullptr)
    {
        std::cerr
            << "Cannot create a VertexArray without a vertex buffer.\n";
        return;
    }

    if (!desc.vertexBuffer->isValid())
    {
        std::cerr
            << "Cannot create a VertexArray with an invalid vertex buffer.\n";
        return;
    }

    if (desc.attributes.empty())
    {
        std::cerr
            << "Cannot create a VertexArray without vertex attributes.\n";
        return;
    }

    if (desc.vertexBinding.stride == 0)
    {
        std::cerr
            << "Vertex buffer stride must be greater than zero.\n";
        return;
    }

    if (!fitsGLintptr(desc.vertexBufferOffset))
    {
        std::cerr
            << "Vertex buffer offset exceeds the OpenGL range.\n";
        return;
    }

    if (!fitsGLsizei(desc.vertexBinding.stride))
    {
        std::cerr
            << "Vertex buffer stride exceeds the OpenGL range.\n";
        return;
    }

    if (desc.indexBuffer != nullptr &&
        !desc.indexBuffer->isValid())
    {
        std::cerr
            << "Cannot attach an invalid index buffer.\n";
        return;
    }

    for (const VertexAttributeDesc& attribute : desc.attributes)
    {
        if (attribute.binding != desc.vertexBinding.binding)
        {
            std::cerr
                << "Vertex attribute binding does not match "
                << "the vertex buffer binding.\n";
            return;
        }

        if (!fitsGLuint(attribute.offset))
        {
            std::cerr
                << "Vertex attribute offset exceeds the OpenGL range.\n";
            return;
        }

        const OpenGLAttributeFormat format =
            toOpenGLFormat(attribute.format);

        if (format.componentCount == 0)
        {
            std::cerr
                << "Unsupported vertex attribute format.\n";
            return;
        }
    }

    glCreateVertexArrays(1, &id_);

    if (id_ == 0)
    {
        std::cerr << "OpenGL failed to create a VertexArray.\n";
        return;
    }

    glVertexArrayVertexBuffer(id_, desc.vertexBinding.binding, desc.vertexBuffer->id_, 
        static_cast<GLintptr>(desc.vertexBufferOffset), static_cast<GLsizei>(desc.vertexBinding.stride));

    for (const VertexAttributeDesc& attribute : desc.attributes)
    {
        const OpenGLAttributeFormat format = toOpenGLFormat(attribute.format);

        glEnableVertexArrayAttrib(id_, attribute.location);

        glVertexArrayAttribFormat(id_, attribute.location, format.componentCount, format.componentType, format.normalized, static_cast<GLuint>(attribute.offset));

        glVertexArrayAttribBinding(id_, attribute.location, attribute.binding);
    }

    if (desc.indexBuffer != nullptr)
    {
        glVertexArrayElementBuffer(id_, desc.indexBuffer->id_);

        hasIndexBuffer_ = true;
    }

#ifndef NDEBUG
    if (!desc.debugName.empty())
    {
        if (fitsGLsizei(desc.debugName.size()))
        {
            glObjectLabel(
                GL_VERTEX_ARRAY,
                id_,
                static_cast<GLsizei>(desc.debugName.size()),
                desc.debugName.c_str());
        }
        else
        {
            std::cerr
                << "VertexArray debug name exceeds the OpenGL range.\n";
        }
    }
#endif
}

VertexArray::VertexArray(VertexArray&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      hasIndexBuffer_(
          std::exchange(other.hasIndexBuffer_, false))
{
}

VertexArray& VertexArray::operator=(
    VertexArray&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();

    id_ = std::exchange(other.id_, 0);
    hasIndexBuffer_ =
        std::exchange(other.hasIndexBuffer_, false);

    return *this;
}

VertexArray::~VertexArray()
{
    release();
}

bool VertexArray::isValid() const noexcept
{
    return id_ != 0;
}

bool VertexArray::hasIndexBuffer() const noexcept
{
    return hasIndexBuffer_;
}

void VertexArray::release() noexcept
{
    if (id_ != 0)
    {
        glDeleteVertexArrays(1, &id_);
        id_ = 0;
    }

    hasIndexBuffer_ = false;
}

}
