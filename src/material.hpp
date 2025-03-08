#pragma once

#include "shader-library.hpp"
#include "data-structures.hpp"
#include "renderer.hpp"

#include <webgpu/webgpu.hpp>

struct Material
{
    StringView name;
    Span<uint8_t> data;
    const Shader* shader = nullptr;
    wgpu::Buffer buffer;
    wgpu::BindGroup bindGroup;
    bool updated = false;

    template<typename T>
    void SetUniform(StringView name, T value);

    template<typename T>
    std::optional<T> GetUniform(StringView name) const;

    void Flush(wgpu::Queue queue);
};

namespace MaterialManager
{
    void Init(const ShaderLibrary& shaderLibrary, wgpu::Device device);

    Material* GetMaterial(StringView name);
    Material* GetDefaultMaterial();

    Array<String> GetMaterialNames(MemoryArena* arena);
}
