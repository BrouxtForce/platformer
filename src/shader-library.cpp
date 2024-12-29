#include "shader-library.hpp"
#include "utility.hpp"
#include "log.hpp"

ShaderLibrary::~ShaderLibrary()
{
    for (auto& [_, shaderModule] : m_ShaderModuleMap)
    {
        shaderModule.release();
    }
}

void ShaderLibrary::Load(wgpu::Device device)
{
    constexpr std::string_view dirname = "shaders/";
    m_Header = ReadFile((std::string)dirname + (std::string)m_HeaderFilename);

    for (const std::string& filename : GetFilesInDirectory((std::string)dirname))
    {
        constexpr std::string_view extension = ".wgsl";
        if (!filename.ends_with(extension) || filename == m_HeaderFilename)
        {
            continue;
        }
        std::string moduleName = filename.substr(0, filename.size() - extension.size());
        wgpu::ShaderModule shaderModule = LoadShaderModule(device, (std::string)dirname + filename);
        m_ShaderModuleMap.insert({ moduleName, shaderModule });
        Log::Debug("Loaded shader module '" + moduleName + "'");
    }
}

wgpu::ShaderModule ShaderLibrary::GetShaderModule(const std::string& name) const
{
    auto it = m_ShaderModuleMap.find(name);
    if (it == m_ShaderModuleMap.end())
    {
        return nullptr;
    }
    return it->second;
}

wgpu::ShaderModule ShaderLibrary::LoadShaderModule(wgpu::Device device, const std::string& filepath)
{
    const std::string shaderSource = Preprocess(ReadFile(filepath));

    wgpu::ShaderModuleWGSLDescriptor wgslDescriptor = wgpu::Default;
    wgslDescriptor.chain.next = nullptr;
    wgslDescriptor.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgslDescriptor.code = shaderSource.c_str();

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor = wgpu::Default;
    shaderModuleDescriptor.label = filepath.c_str();
#if WEBGPU_BACKEND_WGPU
    shaderModuleDescriptor.hintCount = 0;
    shaderModuleDescriptor.hints = nullptr;
#endif
    shaderModuleDescriptor.nextInChain = &wgslDescriptor.chain;

    return device.createShaderModule(shaderModuleDescriptor);
}

std::string ShaderLibrary::Preprocess(const std::string& source)
{
    return source + m_Header;
}
