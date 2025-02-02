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
#include "jump-flood.hpp"

class Shader;
class ShaderLibrary;

class Lighting
{
public:
    Lighting() = default;
    ~Lighting();

    Lighting(const Lighting&) = delete;
    Lighting& operator=(const Lighting&) = delete;

    bool Init(wgpu::Device device, const ShaderLibrary& shaderLibrary);

    void FitToScreen(int width, int height);

    void Render(wgpu::Queue& queue, wgpu::CommandEncoder& commandEncoder, wgpu::TextureView framebuffer);

private:
    uint16_t m_Width = 0;
    uint16_t m_Height = 0;

    wgpu::Device m_Device;
    const Shader* m_Shader = nullptr;

    JumpFlood m_JumpFlood;

    constexpr static int s_NumCascades = 6;
    constexpr static WGPUTextureFormat s_CascadeTextureFormat = WGPUTextureFormat_RGB10A2Unorm;
    wgpu::Texture m_CascadeTexture;
    // TODO: Texture ping-ponging, which would only require two textures for N cascades
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
    wgpu::RenderPipeline m_LightingRenderPipeline;

    void InitTextures();
    void InitSamplers();
    void InitBuffers();
    void InitBindGroupLayouts();
    void InitBindGroups();
    void InitRenderPipelines();

    void DestroyTextures();
    void DestroyBindGroups();
    void Resize(int width, int height);

    friend class Renderer;
};
