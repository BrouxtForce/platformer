#pragma once

#include <optional>

#include <webgpu/webgpu.hpp>

#include "log.hpp"
#include "data-structures.hpp"

class Shader
{
public:
    StringView name;
    wgpu::ShaderModule shaderModule{};

    Shader(wgpu::ShaderModule shaderModule, StringView name, StringView source);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    enum class DataType : uint8_t
    {
        // Scalar types (we don't include bool because it isn't allowed in the uniform address space)
        Int32,
        Uint32,
        Float,

        // Vector types
        Int2,   Int3,   Int4,
        Uint2,  Uint3,  Uint4,
        Float2, Float3, Float4,

        // TODO: Half (f16), matrices?

        Count
    };

    static constexpr uint8_t s_DataTypeSize[(int)DataType::Count] {
        [(int)DataType::Int32]  = 4,
        [(int)DataType::Uint32] = 4,
        [(int)DataType::Float]  = 4,

        [(int)DataType::Int2] = 8,
        [(int)DataType::Int3] = 12,
        [(int)DataType::Int4] = 16,

        [(int)DataType::Uint2] = 8,
        [(int)DataType::Uint3] = 12,
        [(int)DataType::Uint4] = 16,

        [(int)DataType::Float2] = 8,
        [(int)DataType::Float3] = 12,
        [(int)DataType::Float4] = 16
    };

    struct UniformData
    {
        DataType dataType{};
        uint8_t offset{};
    };

    std::unordered_map<StringView, UniformData> m_UniformMap;

private:
    struct TokenInputStream;

    void GenerateReflectionInfo(StringView source);
    void ParseMaterialStruct(TokenInputStream& stream);
};

class ShaderLibrary
{
public:
    ShaderLibrary() = default;

    ShaderLibrary(const ShaderLibrary&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&) = delete;

    void Load(wgpu::Device device);

    const Shader* GetShader(StringView name) const;

private:
    static constexpr StringView m_HeaderFilename = "header.wgsl";
    StringView m_Header;

    std::unordered_map<StringView, std::unique_ptr<Shader>> m_ShaderModuleMap;

    std::unique_ptr<Shader> LoadShader(wgpu::Device, StringView filepath, StringView shaderName);

    String Preprocess(StringView source, MemoryArena* arena);
};
