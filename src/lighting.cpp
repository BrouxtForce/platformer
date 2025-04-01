#include "lighting.hpp"

#include "math.hpp"
#include "log.hpp"
#include "shader-library.hpp"
#include "application.hpp"

Lighting::~Lighting()
{
    DestroyTextures();

    if (m_Sampler) m_Sampler.release();

    if (m_CascadeUniformsBindGroupLayout) m_CascadeUniformsBindGroupLayout.release();
    if (m_RadianceBindGroupLayout) m_RadianceBindGroupLayout.release();
    if (m_CascadeBindGroupLayout) m_CascadeBindGroupLayout.release();

    if (m_CascadeRenderPipeline) m_CascadeRenderPipeline.release();
    if (m_LightingRenderPipeline) m_LightingRenderPipeline.release();
}

bool Lighting::Init(wgpu::Device device, const ShaderLibrary& shaderLibrary)
{
    m_Device = device;
    m_Shader = shaderLibrary.GetShader("lighting");

    // TODO: Failure?
    InitSamplers();
    InitBuffers();
    InitBindGroupLayouts();
    InitRenderPipelines();

    m_JumpFlood.Init(device, 256, 256, shaderLibrary);

    return true;
}

void Lighting::FitToScreen(int width, int height)
{
    constexpr int probeSizeMultiple = 1 << s_NumCascades;

    int newWidth  = probeSizeMultiple * Math::DivideCeil(width, probeSizeMultiple);
    int newHeight = probeSizeMultiple * Math::DivideCeil(height, probeSizeMultiple);

    Resize(newWidth, newHeight);
}

void Lighting::Render(wgpu::Queue& queue, wgpu::CommandEncoder& commandEncoder, wgpu::TextureView framebuffer)
{
    m_JumpFlood.Render(commandEncoder, framebuffer, 8);

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
        .label = (StringView)"Fill Cascades Render Pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
        .depthStencilAttachment = nullptr,
        .occlusionQuerySet = nullptr,
        .timestampWrites = nullptr
    };

    uint32_t probeSize = 2;
    float intervalLength = 0.001f;
    float intervalStart = 0.0f;
    for (int i = 0; i < s_NumCascades; i++)
    {
        UniformData uniformData {
            .intervalLength = intervalLength,
            .intervalStart = intervalStart,
            .probeSize = probeSize,
            .cascadeCount = s_NumCascades,
            .textureSizeX = m_Width,
            .textureSizeY = m_Height
        };
        queue.writeBuffer(m_CascadeUniformBuffer, i * sizeof(UniformData), &uniformData, sizeof(UniformData));
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
    renderPassDescriptor.label = (StringView)"Lighting Render Pass";

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

void Lighting::InitTextures()
{
    m_CascadeTexture = m_Device.createTexture(WGPUTextureDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Cascade Texture",
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
        String label;
        label.arena = &TransientArena;
        label << "Cascade Texture Slice View " << i << '\0';

        m_CascadeTextureSliceViews[i] = m_CascadeTexture.createView(WGPUTextureViewDescriptor {
            .nextInChain = nullptr,
            // TODO: No null termination
            .label = (StringView)label,
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

    // TODO: This does not need to be the same size
    m_RadianceTexture = m_Device.createTexture(WGPUTextureDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Radiance Texture",
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

void Lighting::InitSamplers()
{
    m_Sampler = m_Device.createSampler(WGPUSamplerDescriptor {
        .label = (StringView)"Radiance Cascades Sampler",
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

void Lighting::InitBuffers()
{
    m_CascadeUniformBuffer = m_Device.createBuffer(WGPUBufferDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Cascade Uniform Buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage,
        .size = sizeof(UniformData) * s_NumCascades,
        .mappedAtCreation = false
    });
}

void Lighting::InitBindGroupLayouts()
{
    { // Initialize cascade uniforms bind group layout
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
        m_CascadeUniformsBindGroupLayout = m_Device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
            .nextInChain = nullptr,
            .label = (StringView)"Cascade Uniforms Bind Group Layout",
            .entryCount = 1,
            .entries = &bindGroupLayoutEntry
        });
    }
    { // Initialize radiance bind group layout
        std::array<WGPUBindGroupLayoutEntry, 3> bindGroupLayoutEntries = {
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
                .binding = 2,
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
        m_RadianceBindGroupLayout = m_Device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
            .nextInChain = nullptr,
            .label = (StringView)"Radiance Bind Group Layout",
            .entryCount = bindGroupLayoutEntries.size(),
            .entries = bindGroupLayoutEntries.data()
        });
    }
    { // Initialize cascade bind group layout
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
        m_CascadeBindGroupLayout = m_Device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
            .nextInChain = nullptr,
            .label = (StringView)"Cascade Bind Group Layout",
            .entryCount = 1,
            .entries = &bindGroupLayoutEntry
        });
    }
}

void Lighting::InitBindGroups()
{
    { // Initialize cascade uniforms bind group
        WGPUBindGroupEntry bindGroupEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = m_CascadeUniformBuffer,
            .offset = 0,
            .size = m_CascadeUniformBuffer.getSize(),
            .sampler = nullptr,
            .textureView = nullptr
        };
        m_CascadeUniformBindGroup = m_Device.createBindGroup(WGPUBindGroupDescriptor {
            .nextInChain = nullptr,
            .label = (StringView)"Cascade Uniforms Bind Group",
            .layout = m_CascadeUniformsBindGroupLayout,
            .entryCount = 1,
            .entries = &bindGroupEntry
        });
    }

    { // Initialize radiance bind group
        std::array<WGPUBindGroupEntry, 3> bindGroupEntries {
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
                .sampler = nullptr,
                .textureView = m_JumpFlood.GetSDFTextureView()
            },
            WGPUBindGroupEntry {
                .nextInChain = nullptr,
                .binding = 2,
                .buffer = nullptr,
                .offset = 0,
                .size = 0,
                .sampler = m_Sampler,
                .textureView = nullptr
            }
        };
        m_RadianceBindGroup = m_Device.createBindGroup(WGPUBindGroupDescriptor {
            .nextInChain = nullptr,
            .label = (StringView)"Radiance Bind Group",
            .layout = m_RadianceBindGroupLayout,
            .entryCount = bindGroupEntries.size(),
            .entries = bindGroupEntries.data()
        });
    }

    { // Initialize cascade bind groups
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
            m_CascadeBindGroups[i] = m_Device.createBindGroup(WGPUBindGroupDescriptor {
                .nextInChain = nullptr,
                .label = (StringView)"Cascade Bind Group",
                .layout = m_CascadeBindGroupLayout,
                .entryCount = 1,
                .entries = &bindGroupEntry
            });
        }
    }
}

void Lighting::InitRenderPipelines()
{
    std::array<WGPUBindGroupLayout, 3> bindGroupLayouts {
        m_CascadeUniformsBindGroupLayout,
        m_RadianceBindGroupLayout,
        m_CascadeBindGroupLayout
    };
    wgpu::PipelineLayout pipelineLayout = m_Device.createPipelineLayout(WGPUPipelineLayoutDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Radiance Cascades Pipeline Layout",
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
        .module = m_Shader->shaderModule,
        .entryPoint = (StringView)"merge_cascades_frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &cascadeRenderTarget
    };
    m_CascadeRenderPipeline = m_Device.createRenderPipeline(WGPURenderPipelineDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Cascade Render Pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = m_Shader->shaderModule,
            .entryPoint = (StringView)"fullscreen_quad_vert",
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
        .module = m_Shader->shaderModule,
        .entryPoint = (StringView)"lighting_frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &lightingRenderTarget
    };
    m_LightingRenderPipeline = m_Device.createRenderPipeline(WGPURenderPipelineDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Lighting Render Pipeline",
        .layout = pipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = m_Shader->shaderModule,
            .entryPoint = (StringView)"fullscreen_quad_vert",
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

void Lighting::DestroyTextures()
{
    if (m_CascadeTexture) m_CascadeTexture.destroy();
    m_CascadeTexture = nullptr;

    for (wgpu::TextureView& view : m_CascadeTextureSliceViews)
    {
        if (view) view.release();
        view = nullptr;
    }
    if (m_CascadeTextureView) m_CascadeTextureView.release();
    m_CascadeTextureView = nullptr;

    if (m_RadianceTexture) m_RadianceTexture.destroy();
    m_RadianceTexture = nullptr;

    if (m_RadianceTextureView) m_RadianceTextureView.release();
    m_RadianceTextureView = nullptr;
}

void Lighting::DestroyBindGroups()
{
    if (m_CascadeUniformBindGroup) m_CascadeUniformBindGroup.release();
    m_CascadeUniformBindGroup = nullptr;

    if (m_RadianceBindGroup) m_RadianceBindGroup.release();
    m_RadianceBindGroup = nullptr;

    for (wgpu::BindGroup& group : m_CascadeBindGroups)
    {
        if (group) group.release();
        group = nullptr;
    }
}

void Lighting::Resize(int width, int height)
{
    if (m_Width == width && m_Height == height)
    {
        return;
    }
    m_Width = width;
    m_Height = height;

    Log::Debug("Resized lighting textures: %, %", m_Width, m_Height);

    DestroyTextures();
    DestroyBindGroups();
    InitTextures();
    InitBindGroups();
}
