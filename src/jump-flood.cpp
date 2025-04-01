#include "jump-flood.hpp"
#include "shader-library.hpp"
#include "application.hpp"

JumpFlood::~JumpFlood()
{
    if (m_PingPongTexture) m_PingPongTexture.destroy();
    for (wgpu::TextureView& view : m_PingPongTextureViews)
    {
        if (view) view.release();
    }

    if (m_Sampler) m_Sampler.release();

    if (m_BindGroupLayout) m_BindGroupLayout.release();
    for (wgpu::BindGroup& bindGroup : m_PingPongBindGroups)
    {
        if (bindGroup) bindGroup.release();
    }

    if (m_InitRenderPipeline) m_InitRenderPipeline.release();
    if (m_MainPassRenderPipeline) m_MainPassRenderPipeline.release();
    if (m_FinalPassRenderPipeline) m_FinalPassRenderPipeline.release();
}

void JumpFlood::Init(wgpu::Device device, int width, int height, const ShaderLibrary& shaderLibrary)
{
    m_Width = width;
    m_Height = height;

    m_Device = device;
    m_Shader = shaderLibrary.GetShader("jump-flood");

    // TODO: Failure?
    InitTextures();
    InitSamplers();
    InitBindGroups();
    InitRenderPipelines();
}

void JumpFlood::Render(wgpu::CommandEncoder commandEncoder, wgpu::TextureView sourceTexture, int numPasses)
{
    WGPURenderPassColorAttachment colorAttachment {
        .nextInChain = nullptr,
        .view = m_PingPongTextureViews[0],
        .depthSlice = ~(uint32_t)0,
        .resolveTarget = nullptr,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color{ 0, 0, 0, 1 }
    };

    WGPURenderPassDescriptor renderPassDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Jump Flood Render Pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
        .depthStencilAttachment = nullptr,
        .occlusionQuerySet = nullptr,
        .timestampWrites = nullptr
    };

    { // Initial pass
        wgpu::BindGroup bindGroup = CreateBindGroup(sourceTexture, m_Sampler);

        wgpu::RenderPassEncoder renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

        renderEncoder.setPipeline(m_InitRenderPipeline);
        renderEncoder.setBindGroup(0, bindGroup, 0, nullptr);
        renderEncoder.draw(4, 1, 0, 0);
        renderEncoder.end();

        renderEncoder.release();
        bindGroup.release();
    }

    // Jump flood passes (we must ensure the last texture view rendered to is m_PingPongTextureViews[0])
    int pingPongOffset = numPasses % 2;
    for (int i = 0; i < numPasses; i++)
    {
        int jump = 1 << (numPasses - i - 1);

        colorAttachment.view = m_PingPongTextureViews[(i + pingPongOffset + 1) % 2];
        wgpu::RenderPassEncoder renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

        if (i + 1 < numPasses)
        {
            renderEncoder.setPipeline(m_MainPassRenderPipeline);
        }
        else
        {
            renderEncoder.setPipeline(m_FinalPassRenderPipeline);
        }
        renderEncoder.setBindGroup(0, m_PingPongBindGroups[(i + pingPongOffset) % 2], 0, nullptr);
        renderEncoder.draw(4, 1, 0, jump);
        renderEncoder.end();

        renderEncoder.release();
    }
}

wgpu::TextureView JumpFlood::GetSDFTextureView() const
{
    return m_PingPongTextureViews[0];
}

wgpu::BindGroup JumpFlood::CreateBindGroup(wgpu::TextureView textureView, wgpu::Sampler sampler)
{
    std::array<WGPUBindGroupEntry, 2> bindGroupEntries {
        WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = nullptr,
            .offset = 0,
            .size = 0,
            .sampler = nullptr,
            .textureView = textureView
        },
        WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = 1,
            .buffer = nullptr,
            .offset = 0,
            .size = 0,
            .sampler = sampler,
            .textureView = nullptr
        },
    };
    return m_Device.createBindGroup(WGPUBindGroupDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Jump Flood Bind Group",
        .layout = m_BindGroupLayout,
        .entryCount = bindGroupEntries.size(),
        .entries = bindGroupEntries.data()
    });
}

void JumpFlood::InitTextures()
{
    m_PingPongTexture = m_Device.createTexture(WGPUTextureDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Jump Flood Texture",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::_2D,
        .size = {
            .width = (uint32_t)m_Width,
            .height = (uint32_t)m_Height,
            .depthOrArrayLayers = 2
        },
        .format = m_TextureFormat,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 0,
        .viewFormats = nullptr
    });
    for (int i = 0; i < (int)m_PingPongTextureViews.size(); i++)
    {
        String label;
        label.arena = &TransientArena;
        label << "Jump Flood Slice View " << i << '\0';

        m_PingPongTextureViews[i] = m_PingPongTexture.createView(WGPUTextureViewDescriptor {
            .nextInChain = nullptr,
            // TODO: No null termination
            .label = (StringView)label,
            .format = m_TextureFormat,
            .dimension = wgpu::TextureViewDimension::_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = (uint32_t)i,
            .arrayLayerCount = 1,
            .aspect = wgpu::TextureAspect::All
        });
    }
}

void JumpFlood::InitSamplers()
{
    m_Sampler = m_Device.createSampler(WGPUSamplerDescriptor {
        .label = (StringView)"Jump Flood Sampler",
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .addressModeW = wgpu::AddressMode::Undefined,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Undefined,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1.0f,
        .compare = wgpu::CompareFunction::Undefined,
        .maxAnisotropy = 1
    });
}

void JumpFlood::InitBindGroups()
{
    std::array<WGPUBindGroupLayoutEntry, 2> bindGroupLayoutEntries = {
        WGPUBindGroupLayoutEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer = {},
            .sampler = {},
            .texture = {
                .nextInChain = nullptr,
                .sampleType = wgpu::TextureSampleType::Float,
                .viewDimension = wgpu::TextureViewDimension::_2D,
                .multisampled = false
            },
            .storageTexture = {}
        },
        WGPUBindGroupLayoutEntry {
            .nextInChain = nullptr,
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer = {},
            .sampler = {
                .nextInChain = nullptr,
                .type = wgpu::SamplerBindingType::Filtering
            },
            .texture = {},
            .storageTexture = {}
        }
    };
    m_BindGroupLayout = m_Device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Jump Flood Bind Group Layout",
        .entryCount = bindGroupLayoutEntries.size(),
        .entries = bindGroupLayoutEntries.data()
    });

    for (int i = 0; i < (int)m_PingPongBindGroups.size(); i++)
    {
        m_PingPongBindGroups[i] = CreateBindGroup(m_PingPongTextureViews[i], m_Sampler);
    }
}

void JumpFlood::InitRenderPipelines()
{
    wgpu::PipelineLayout pipelineLayout = m_Device.createPipelineLayout(WGPUPipelineLayoutDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Jump Flood Pipeline Layout",
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout*)&m_BindGroupLayout
    });

    WGPUColorTargetState renderTarget {
        .nextInChain = nullptr,
        .format = m_TextureFormat,
        .blend = nullptr,
        .writeMask = wgpu::ColorWriteMask::All
    };
    WGPUFragmentState fragmentState {
        .nextInChain = nullptr,
        .module = m_Shader->shaderModule,
        .entryPoint = (StringView)"jump_flood_init_frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &renderTarget
    };
    WGPURenderPipelineDescriptor renderPipelineDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Jump Flood Render Pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = m_Shader->shaderModule,
            .entryPoint = (StringView)"jump_flood_vert",
            .constantCount = 0,
            .constants = nullptr,
            .bufferCount = 0,
            .buffers = nullptr
        },
        .primitive = {
            .nextInChain = nullptr,
            .topology = wgpu::PrimitiveTopology::TriangleStrip,
            .stripIndexFormat = wgpu::IndexFormat::Undefined,
            .frontFace = wgpu::FrontFace::Undefined,
            .cullMode = wgpu::CullMode::None
        },
        .depthStencil = nullptr,
        .multisample = {
            .nextInChain = nullptr,
            .count = 1,
            .mask = ~0u,
            .alphaToCoverageEnabled = false
        },
        .fragment = &fragmentState
    };

    m_InitRenderPipeline = m_Device.createRenderPipeline(renderPipelineDescriptor);
    fragmentState.entryPoint = (StringView)"jump_flood_main_frag";
    m_MainPassRenderPipeline = m_Device.createRenderPipeline(renderPipelineDescriptor);
    fragmentState.entryPoint = (StringView)"jump_flood_final_pass_frag";
    m_FinalPassRenderPipeline = m_Device.createRenderPipeline(renderPipelineDescriptor);
}
