#pragma once

#include <optional>

#include <webgpu/webgpu.hpp>

#include "log.hpp"
#include "scene.hpp"

struct Material;

class Shader
{
public:
    wgpu::ShaderModule shaderModule{};

    Shader(wgpu::ShaderModule shaderModule, std::string_view source);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    template<typename T>
    void WriteUniform(Material& material, const std::string& name, T value) const;

    template<typename T>
    std::optional<T> GetUniform(const Material& material, const std::string& name) const;

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

private:
    struct TokenInputStream;

    void GenerateReflectionInfo(std::string_view source);
    void ParseMaterialStruct(TokenInputStream& stream);

    struct UniformData
    {
        DataType dataType{};
        uint8_t offset{};
    };

    std::unordered_map<std::string, UniformData> m_UniformMap;
};

template<typename T>
void WriteUniform(Entity* entity, const std::string& name, T value)
{
    if (!entity->shader)
    {
        Log::Error("Shader is nullptr");
        return;
    }
    entity->shader->WriteUniform<T>(entity->material, name, value);
}

template<typename T>
[[nodiscard]]
std::optional<T> GetUniform(const Entity* entity, const std::string& name)
{
    if (!entity->shader)
    {
        Log::Error("Shader is nullptr");
        return std::nullopt;
    }
    return entity->shader->GetUniform<T>(entity->material, name);
}

class ShaderLibrary
{
public:
    ShaderLibrary() = default;

    ShaderLibrary(const ShaderLibrary&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&) = delete;

    void Load(wgpu::Device device);

    const Shader* GetShader(const std::string& name) const;

private:
    static constexpr std::string_view m_HeaderFilename = "header.wgsl";
    std::string m_Header;

    std::unordered_map<std::string, std::unique_ptr<Shader>> m_ShaderModuleMap;

    std::unique_ptr<Shader> LoadShader(wgpu::Device, const std::string& filepath);

    std::string Preprocess(const std::string& source);
};
