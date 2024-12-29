#include "lighting.hpp"
#include "webgpu/webgpu.hpp"

#include <array>

Lighting::~Lighting()
{
    if (m_CascadeTexture) m_CascadeTexture.destroy();
    for (wgpu::TextureView& view : m_CascadeTextureSliceViews)
    {
        if (view) view.release();
    }
    if (m_CascadeTextureView) m_CascadeTextureView.release();

    if (m_RadianceTexture) m_RadianceTexture.destroy();
    if (m_RadianceTextureView) m_RadianceTextureView.release();

    if (m_Sampler) m_Sampler.release();

    if (m_CascadeUniformsBindGroupLayout) m_CascadeUniformsBindGroupLayout.release();
    if (m_CascadeUniformBindGroup) m_CascadeUniformBindGroup.release();

    if (m_RadianceBindGroupLayout) m_RadianceBindGroupLayout.release();
    if (m_RadianceBindGroup) m_RadianceBindGroup.release();

    if (m_CascadeBindGroupLayout) m_CascadeBindGroupLayout.release();
    for (wgpu::BindGroup& group : m_CascadeBindGroups)
    {
        if (group) group.release();
    }

    if (m_CascadeRenderPipeline) m_CascadeRenderPipeline.release();
    if (m_RadianceRenderPipeline) m_RadianceRenderPipeline.release();
    if (m_LightingRenderPipeline) m_LightingRenderPipeline.release();
}

bool Lighting::Init(wgpu::Device device, const ShaderLibrary& shaderLibrary)
{
    m_ShaderModule = shaderLibrary.GetShaderModule("lighting");

    // TODO: Failure?
    InitTextures(device);
    InitSamplers(device);
    InitBuffers(device);
    InitBindGroups(device);
    InitRenderPipelines(device);

    return true;
}

void Lighting::Render(wgpu::Queue& queue, wgpu::CommandEncoder& commandEncoder, wgpu::TextureView framebuffer)
{
    WGPURenderPassColorAttachment colorAttachment {
        .nextInChain = nullptr,
        .view = nullptr, // set later
        .depthSlice = ~(uint32_t)0,
        .resolveTarget = nullptr,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color{ 0, 0, 0, 1 }
    };

    WGPURenderPassDescriptor renderPassDescriptor {
        .nextInChain = nullptr,
        .label = "Fill Cascades Render Pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
        .depthStencilAttachment = nullptr,
        .occlusionQuerySet = nullptr,
        .timestampWrites = nullptr
    };

    uint32_t probeSize = 2;
    float intervalLength = 0.01f;
    float intervalStart = 0.0f;
    for (int i = 0; i < s_NumCascades; i++)
    {
        m_CascadeUniformBuffer.Write(queue, UniformData {
            .intervalLength = intervalLength,
            .intervalStart = intervalStart,
            .probeSize = probeSize,
            .cascadeCount = s_NumCascades,
            .textureSizeX = m_Width,
            .textureSizeY = m_Height
        }, i * sizeof(UniformData));
        probeSize *= 2;
        intervalStart += intervalLength;
        intervalLength *= 4.0f;
    }

    for (int i = s_NumCascades - 1; i >= 0; i--)
    {
        colorAttachment.view = m_CascadeTextureSliceViews[i];
        wgpu::RenderPassEncoder renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

        renderEncoder.setPipeline(m_CascadeRenderPipeline);

        renderEncoder.setBindGroup(0, m_CascadeUniformBindGroup, 0, nullptr);
        renderEncoder.setBindGroup(1, m_RadianceBindGroup, 0, nullptr);
        renderEncoder.setBindGroup(2, m_CascadeBindGroups[(i + 1) % s_NumCascades], 0, nullptr);

        // Set the first instance to the current cascade index
        renderEncoder.draw(4, 1, 0, i);

        renderEncoder.end();
        renderEncoder.release();
    }

    colorAttachment.view = framebuffer;
    colorAttachment.loadOp = wgpu::LoadOp::Load;
    renderPassDescriptor.label = "Lighting Render Pass";

    wgpu::RenderPassEncoder renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    renderEncoder.setPipeline(m_LightingRenderPipeline);

    renderEncoder.setBindGroup(0, m_CascadeUniformBindGroup, 0, nullptr);
    renderEncoder.setBindGroup(1, m_RadianceBindGroup, 0, nullptr);
    renderEncoder.setBindGroup(2, m_CascadeBindGroups[0], 0, nullptr);

    // The first instance is zero, corresponding to the lowest cascade layer
    renderEncoder.draw(4, 1, 0, 0);

    renderEncoder.end();
    renderEncoder.release();
}

void Lighting::InitTextures(wgpu::Device device)
{
    m_CascadeTexture = device.createTexture(WGPUTextureDescriptor {
        .nextInChain = nullptr,
        .label = "Cascade Texture",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::_2D,
        .size = {
            .width = m_Width,
            .height = m_Height,
            .depthOrArrayLayers = s_NumCascades
        },
        .format = s_CascadeTextureFormat,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 0,
        .viewFormats = nullptr
    });
    for (int i = 0; i < s_NumCascades; i++)
    {
        m_CascadeTextureSliceViews[i] = m_CascadeTexture.createView(WGPUTextureViewDescriptor {
            .nextInChain = nullptr,
            .label = ("Cascade Texture Slice View " + std::to_string(i)).c_str(),
            .format = s_CascadeTextureFormat,
            .dimension = wgpu::TextureViewDimension::_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = (uint32_t)i,
            .arrayLayerCount = 1,
            .aspect = wgpu::TextureAspect::All
        });
    }
    m_CascadeTextureView = m_CascadeTexture.createView();

    m_RadianceTexture = device.createTexture(WGPUTextureDescriptor {
        .nextInChain = nullptr,
        .label = "Radiance Texture",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::_2D,
        .size = {
            .width = m_Width,
            .height = m_Height,
            .depthOrArrayLayers = 1
        },
        .format = s_RadianceTextureFormat,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 0,
        .viewFormats = nullptr
    });
    m_RadianceTextureView = m_RadianceTexture.createView();
}

void Lighting::InitSamplers(wgpu::Device device)
{
    m_Sampler = device.createSampler(WGPUSamplerDescriptor {
        .label = "Radiance Cascades Sampler",
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

void Lighting::InitBuffers(wgpu::Device device)
{
    m_CascadeUniformBuffer = Buffer(
        device,
        sizeof(UniformData) * s_NumCascades,
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage
    );
}

void Lighting::InitBindGroups(wgpu::Device device)
{
    { // Initialize cascade uniforms bind groups
        WGPUBindGroupLayoutEntry bindGroupLayoutEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer = {
                .nextInChain = nullptr,
                .type = wgpu::BufferBindingType::ReadOnlyStorage,
                .hasDynamicOffset = false,
                .minBindingSize = s_UniformBufferSize
            },
            .sampler = {},
            .texture = {},
            .storageTexture = {}
        };
        m_CascadeUniformsBindGroupLayout = device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
            .nextInChain = nullptr,
            .label = "Cascade Uniforms Bind Group Layout",
            .entryCount = 1,
            .entries = &bindGroupLayoutEntry
        });
        WGPUBindGroupEntry bindGroupEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = m_CascadeUniformBuffer.get(),
            .offset = 0,
            .size = m_CascadeUniformBuffer.Size(),
            .sampler = nullptr,
            .textureView = nullptr
        };
        m_CascadeUniformBindGroup = device.createBindGroup(WGPUBindGroupDescriptor {
            .nextInChain = nullptr,
            .label = "Cascade Uniforms Bind Group",
            .layout = m_CascadeUniformsBindGroupLayout,
            .entryCount = 1,
            .entries = &bindGroupEntry
        });
    }

    { // Initialize radiance bind group
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
        m_RadianceBindGroupLayout = device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
            .nextInChain = nullptr,
            .label = "Radiance Bind Group Layout",
            .entryCount = bindGroupLayoutEntries.size(),
            .entries = bindGroupLayoutEntries.data()
        });
        std::array<WGPUBindGroupEntry, 2> bindGroupEntries {
            WGPUBindGroupEntry {
                .nextInChain = nullptr,
                .binding = 0,
                .buffer = nullptr,
                .offset = 0,
                .size = 0,
                .sampler = nullptr,
                .textureView = m_RadianceTextureView
            },
            WGPUBindGroupEntry {
                .nextInChain = nullptr,
                .binding = 1,
                .buffer = nullptr,
                .offset = 0,
                .size = 0,
                .sampler = m_Sampler,
                .textureView = nullptr
            },
        };
        m_RadianceBindGroup = device.createBindGroup(WGPUBindGroupDescriptor {
            .nextInChain = nullptr,
            .label = "Radiance Bind Group",
            .layout = m_RadianceBindGroupLayout,
            .entryCount = bindGroupEntries.size(),
            .entries = bindGroupEntries.data()
        });
    }

    { // Initialize cascade bind group
        WGPUBindGroupLayoutEntry bindGroupLayoutEntry {
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
        };
        m_CascadeBindGroupLayout = device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
            .nextInChain = nullptr,
            .label = "Cascade Bind Group Layout",
            .entryCount = 1,
            .entries = &bindGroupLayoutEntry
        });
        for (int i = 0; i < s_NumCascades; i++)
        {
            WGPUBindGroupEntry bindGroupEntry {
                .nextInChain = nullptr,
                .binding = 0,
                .buffer = nullptr,
                .offset = 0,
                .size = 0,
                .sampler = nullptr,
                .textureView = m_CascadeTextureSliceViews[i]
            };
            m_CascadeBindGroups[i] = device.createBindGroup(WGPUBindGroupDescriptor {
                .nextInChain = nullptr,
                .label = "Cascade Bind Group",
                .layout = m_CascadeBindGroupLayout,
                .entryCount = 1,
                .entries = &bindGroupEntry
            });
        }
    }
}

void Lighting::InitRenderPipelines(wgpu::Device device)
{
    std::array<WGPUBindGroupLayout, 3> bindGroupLayouts {
        m_CascadeUniformsBindGroupLayout,
        m_RadianceBindGroupLayout,
        m_CascadeBindGroupLayout
    };
    wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(WGPUPipelineLayoutDescriptor {
        .nextInChain = nullptr,
        .label = "Radiance Cascades Pipeline Layout",
        .bindGroupLayoutCount = bindGroupLayouts.size(),
        .bindGroupLayouts = bindGroupLayouts.data()
    });

    WGPUColorTargetState cascadeRenderTarget {
        .nextInChain = nullptr,
        .format = s_CascadeTextureFormat,
        .blend = nullptr,
        .writeMask = wgpu::ColorWriteMask::All
    };
    WGPUFragmentState cascadeFragmentState {
        .nextInChain = nullptr,
        .module = m_ShaderModule,
        .entryPoint = "merge_cascades_frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &cascadeRenderTarget
    };
    m_CascadeRenderPipeline = device.createRenderPipeline(WGPURenderPipelineDescriptor {
        .nextInChain = nullptr,
        .label = "Cascade Render Pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = m_ShaderModule,
            .entryPoint = "fullscreen_quad_vert",
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
        .fragment = &cascadeFragmentState
    });

    WGPUColorTargetState radianceRenderTarget {
        .nextInChain = nullptr,
        .format = s_CascadeTextureFormat,
        .blend = nullptr,
        .writeMask = wgpu::ColorWriteMask::All
    };
    WGPUFragmentState radianceFragmentState {
        .nextInChain = nullptr,
        .module = m_ShaderModule,
        .entryPoint = "radiance_frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &radianceRenderTarget
    };
    m_RadianceRenderPipeline = device.createRenderPipeline(WGPURenderPipelineDescriptor {
        .nextInChain = nullptr,
        .label = "Radiance Render Pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = m_ShaderModule,
            .entryPoint = "fullscreen_quad_vert",
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
        .fragment = &radianceFragmentState
    });

    WGPUBlendState blendState {
        .color = {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::One,
            .dstFactor = wgpu::BlendFactor::One
        },
        .alpha = {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::One,
            .dstFactor = wgpu::BlendFactor::One
        }
    };
    WGPUColorTargetState lightingRenderTarget {
        .nextInChain = nullptr,
        .format = wgpu::TextureFormat::BGRA8Unorm,
        .blend = &blendState,
        .writeMask = wgpu::ColorWriteMask::All
    };
    WGPUFragmentState lightingFragmentState {
        .nextInChain = nullptr,
        .module = m_ShaderModule,
        .entryPoint = "lighting_frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &lightingRenderTarget
    };
    m_LightingRenderPipeline = device.createRenderPipeline(WGPURenderPipelineDescriptor {
        .nextInChain = nullptr,
        .label = "Lighting Render Pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = m_ShaderModule,
            .entryPoint = "fullscreen_quad_vert",
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
        .fragment = &lightingFragmentState
    });
}
