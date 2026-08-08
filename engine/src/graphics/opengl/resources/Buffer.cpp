#include <graphics/resources/Buffer.hpp>

#include <glad/gl.h>

#include <iostream>
#include <limits>
#include <utility>

namespace stylized::graphics
{

namespace
{

GLenum toOpenGLUsage(const BufferUsage usage) noexcept
{
    switch (usage)
    {
    case BufferUsage::Static:
        return GL_STATIC_DRAW;

    case BufferUsage::Dynamic:
        return GL_DYNAMIC_DRAW;
    }

    return GL_STATIC_DRAW;
}

bool fitsOpenGLSize(const std::size_t value) noexcept
{
    return  value <= static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max());
}

} // namespace

Buffer::Buffer(const BufferDesc& desc, const std::span<const std::byte> initialData)
    :size_(desc.size), usage_(desc.usage)
{
    if (desc.size == 0)
    {
        std::cerr << "Cannot create a zero-sided GPU buffer.\n";
        size_ = 0;
        return;
    }

    if (!fitsOpenGLSize(desc.size))
    {
        std::cerr << "GPU buffer size exceeds the OpenGL size range.\n";
        size_ = 0;
        return;
    }

    if (initialData.size_bytes() > desc.size)
    {
        std::cerr << "Initial data is larger than the requested GPU buffer.\n";
        size_ = 0;
        return;
    }

    glCreateBuffers(1, &id_);

    if (id_ == 0)
    {
        std::cerr << "OpenGL failed to create a GPU buffer.\n";
        size_ = 0;
        return;
    }

    const bool hasCompleteInitialData = initialData.size_bytes() == desc.size;

    glNamedBufferData(id_, static_cast<GLsizeiptr>(desc.size), hasCompleteInitialData ? initialData.data() : nullptr, toOpenGLUsage(desc.usage));

    if (!hasCompleteInitialData && !initialData.empty())
    {
        glNamedBufferSubData(
            id_,
            0,
            static_cast<GLsizeiptr>(initialData.size_bytes()),
            initialData.data());
    }

#ifndef NDEBUG
    if (!desc.debugName.empty())
    {
        const auto maximumLabelLength =
            static_cast<std::size_t>(
                std::numeric_limits<GLsizei>::max());

        if (desc.debugName.size() <= maximumLabelLength)
        {
            glObjectLabel(
                GL_BUFFER,
                id_,
                static_cast<GLsizei>(desc.debugName.size()),
                desc.debugName.c_str());
        }
    }
#endif
}

Buffer::Buffer(Buffer&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      size_(std::exchange(other.size_, 0)),
      usage_(other.usage_)
{
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();

    id_ = std::exchange(other.id_, 0);
    size_ = std::exchange(other.size_, 0);
    usage_ = other.usage_;

    return *this;
}

Buffer::~Buffer()
{
    release();
}

bool Buffer::isValid() const noexcept
{
    return id_ != 0;
}

std::size_t Buffer::size() const noexcept
{
    return size_;
}

BufferUsage Buffer::usage() const noexcept
{
    return usage_;
}

bool Buffer::update(const std::size_t offset, const std::span<const std::byte> data)
{
    if (!isValid())
    {
        std::cerr << "Cannot update an invalid GPU buffer.\n";
        return false;
    }

    if (data.empty())
    {
        return true;
    }

    if (offset > size_ || data.size_bytes() > size_ - offset)
    {
        std::cerr
            << "GPU buffer update is outside the buffer bounds.\n";
        return false;
    }

    if (!fitsOpenGLSize(offset) || !fitsOpenGLSize(data.size_bytes()))
    {
        std::cerr << "GPU buffer update exceeds the OpenGL size range.\n";
        return false;
    }

    glNamedBufferSubData(id_, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(data.size_bytes()), data.data());

    return true;
}

void Buffer::release() noexcept
{
    if (id_ != 0)
    {
        glDeleteBuffers(1, &id_);
        id_ = 0;
    }

    size_ = 0;
}

} // namespace stylized::graphics
