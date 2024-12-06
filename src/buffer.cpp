#include "buffer.hpp"

Buffer::Buffer(wgpu::Buffer buffer, uint32_t offset)
    : m_Buffer(buffer), m_Offset(offset)
{
    m_Size = buffer.getSize();
}

Buffer::Buffer(wgpu::Device device, uint32_t size, uint32_t usage)
    : m_Size(size), m_Owning(true)
{
    assert(size > 0);

    wgpu::BufferDescriptor descriptor;
    descriptor.label = nullptr;
    descriptor.usage = usage;
    descriptor.size = size;
    descriptor.mappedAtCreation = false;
    m_Buffer = device.createBuffer(descriptor);
}

Buffer::Buffer(Buffer&& other)
{
    m_Buffer = other.m_Buffer;
    m_Size = other.m_Size;
    m_Offset = other.m_Offset;
    m_Owning = other.m_Owning;

    other.m_Buffer = {};
    other.m_Size = 0;
    other.m_Offset = 0;
    other.m_Owning = false;
}

Buffer& Buffer::operator=(Buffer&& other)
{
    if (this != &other)
    {
        this->~Buffer();

        m_Buffer = other.m_Buffer;
        m_Size = other.m_Size;
        m_Offset = other.m_Offset;
        m_Owning = other.m_Owning;

        other.m_Buffer = {};
        other.m_Size = 0;
        other.m_Offset = 0;
        other.m_Owning = false;
    }
    return *this;
}

Buffer::~Buffer()
{
    if (m_Owning)
    {
        m_Buffer.destroy();
    }
}

void Buffer::Write(wgpu::Queue queue, void* data, uint32_t size, uint32_t offset)
{
    assert(size <= m_Size);
    queue.writeBuffer(m_Buffer, offset, data, size);
}

bool Buffer::IsEmpty() const
{
    return m_Size == 0;
}

wgpu::Buffer Buffer::get() const
{
    return m_Buffer;
}
