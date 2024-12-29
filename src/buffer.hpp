#pragma once

#include <webgpu/webgpu.hpp>

class Buffer
{
public:
    Buffer() = default;
    Buffer(wgpu::Buffer buffer, uint32_t offset = 0);
    Buffer(wgpu::Device device, uint32_t size, uint32_t usage);

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other);
    Buffer& operator=(Buffer&& other);

    ~Buffer();

    void Write(wgpu::Queue queue, void* data, uint32_t size, uint32_t offset = 0);

    template<typename T>
    void Write(wgpu::Queue queue, const T& data, uint32_t offset = 0)
    {
        Write(queue, (void*)&data, sizeof(T), offset);
    }

    bool IsEmpty() const;
    wgpu::Buffer get() const;
    uint32_t Size() const;

private:
    wgpu::Buffer m_Buffer;
    uint32_t m_Size = 0;
    uint32_t m_Offset = 0;

    bool m_Owning = false;
};
