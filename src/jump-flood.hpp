/*
    An implementation of the Jump Flood Algorithm to calculate an SDF texture
    of an arbitrarily complex scene

    Reference:
    - https://mini.gmshaders.com/p/gm-shaders-mini-jfa
*/

#pragma once

#include <array>

#include <webgpu/webgpu.hpp>

class ShaderLibrary;

class JumpFlood
{
public:
    JumpFlood() = default;
    ~JumpFlood();

    JumpFlood(const JumpFlood&) = delete;
    JumpFlood& operator=(const JumpFlood&) = delete;

    void Init(wgpu::Device device, int width, int height, const ShaderLibrary& shaderLibrary);

    void Render(wgpu::CommandEncoder commandEncoder, wgpu::TextureView sourceTexture, int numPasses);

    wgpu::TextureView GetSDFTextureView() const;

private:
    int m_Width = 0;
    int m_Height = 0;

    wgpu::Device m_Device;
    wgpu::ShaderModule m_ShaderModule;

    static constexpr WGPUTextureFormat m_TextureFormat = WGPUTextureFormat_RG16Float;

    wgpu::Texture m_PingPongTexture;
    std::array<wgpu::TextureView, 2> m_PingPongTextureViews;

    wgpu::Sampler m_Sampler;

    wgpu::BindGroupLayout m_BindGroupLayout;
    std::array<wgpu::BindGroup, 2> m_PingPongBindGroups;

    wgpu::RenderPipeline m_InitRenderPipeline;
    wgpu::RenderPipeline m_MainPassRenderPipeline;
    wgpu::RenderPipeline m_FinalPassRenderPipeline;

    wgpu::BindGroup CreateBindGroup(wgpu::TextureView textureView, wgpu::Sampler sampler);

    void InitTextures();
    void InitSamplers();
    void InitBindGroups();
    void InitRenderPipelines();
};
