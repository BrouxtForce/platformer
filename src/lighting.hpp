/*
    Lighting is calculated using the relatively-new technique Radiance Cascades.

    References:
    - https://mini.gmshaders.com/p/radiance-cascades
    - https://mini.gmshaders.com/p/radiance-cascades2
    - https://jason.today/rc
    - https://m4xc.dev/articles/fundamental-rc
    - https://radiance-cascades.com
*/

#pragma once

#include <vector>

#include <webgpu/webgpu.hpp>

#include "buffer.hpp"
#include "shader-library.hpp"

class Lighting
{
public:
    Lighting() = default;
    ~Lighting();

    bool Init(wgpu::Device device, const ShaderLibrary& shaderLibrary);

    void Render(wgpu::Queue& queue, wgpu::CommandEncoder& commandEncoder, wgpu::TextureView framebuffer);

private:
    uint16_t m_Width = 512;
    uint16_t m_Height = 512;

    wgpu::ShaderModule m_ShaderModule;

    constexpr static int s_NumCascades = 4;
    constexpr static WGPUTextureFormat s_CascadeTextureFormat = WGPUTextureFormat_RGB10A2Unorm;
    wgpu::Texture m_CascadeTexture;
    std::array<wgpu::TextureView, s_NumCascades> m_CascadeTextureSliceViews;
    wgpu::TextureView m_CascadeTextureView;

    constexpr static WGPUTextureFormat s_RadianceTextureFormat = WGPUTextureFormat_BGRA8Unorm;
    wgpu::Texture m_RadianceTexture;
    wgpu::TextureView m_RadianceTextureView;

    wgpu::Sampler m_Sampler;

    Buffer m_CascadeUniformBuffer;

    struct UniformData
    {
        uint32_t probeSize{};
        float intervalStart{};
        float intervalLength{};
        uint32_t cascadeCount{};
        uint32_t textureSizeX{};
        uint32_t textureSizeY{};
    };
    constexpr static int s_UniformBufferSize = sizeof(UniformData);

    wgpu::BindGroupLayout m_CascadeUniformsBindGroupLayout;
    wgpu::BindGroup m_CascadeUniformBindGroup;

    wgpu::BindGroupLayout m_RadianceBindGroupLayout;
    wgpu::BindGroup m_RadianceBindGroup;

    wgpu::BindGroupLayout m_CascadeBindGroupLayout;
    std::array<wgpu::BindGroup, s_NumCascades> m_CascadeBindGroups;

    wgpu::RenderPipeline m_CascadeRenderPipeline;
    wgpu::RenderPipeline m_RadianceRenderPipeline;
    wgpu::RenderPipeline m_LightingRenderPipeline;

    void InitTextures(wgpu::Device device);
    void InitSamplers(wgpu::Device device);
    void InitBuffers(wgpu::Device device);
    void InitBindGroups(wgpu::Device device);
    void InitRenderPipelines(wgpu::Device device);

    friend class Renderer;
};
