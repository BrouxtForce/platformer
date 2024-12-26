#pragma once

#include <optional>

#include <webgpu/webgpu.hpp>

class ShaderLibrary
{
public:
    ShaderLibrary() = default;
    ~ShaderLibrary();

    void Load(wgpu::Device device);

    wgpu::ShaderModule GetShaderModule(const std::string& name);

private:
    static constexpr std::string_view m_HeaderFilename = "header.wgsl";
    std::string m_Header;

    std::unordered_map<std::string, wgpu::ShaderModule> m_ShaderModuleMap;

    wgpu::ShaderModule LoadShaderModule(wgpu::Device, const std::string& filepath);

    std::string Preprocess(const std::string& source);
};
