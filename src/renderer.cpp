#include <sdl3webgpu.h>
#include <webgpu/webgpu.hpp>

#if DEBUG
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>
#endif

#include "math.hpp"
#include "renderer.hpp"
#include "utility.hpp"
#include "log.hpp"

bool Renderer::Init(SDL_Window* window)
{
    m_Window = window;

    #if __EMSCRIPTEN__
        m_Instance = wgpuCreateInstance(nullptr);
    #else
        wgpu::InstanceDescriptor desc = {};
        m_Instance = wgpu::createInstance(desc);
    #endif

    if (!m_Instance) {
        Log::Error("Failed to create WebGPU instance.");
        return false;
    }

    m_Surface = SDL_GetWGPUSurface(m_Instance, window);
    LoadAdapterSync();
    LoadDeviceSync();

    m_Queue = m_Device.getQueue();

    m_ShaderLibrary.Load(m_Device);

    m_Format = m_Surface.getPreferredFormat(m_Adapter);

    // Material bind group layout
    wgpu::BindGroupLayoutEntry bindGroupLayoutEntry = wgpu::Default;
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindGroupLayoutEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    bindGroupLayoutEntry.buffer.minBindingSize = sizeof(Material);

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor;
    bindGroupLayoutDescriptor.entryCount = 1;
    bindGroupLayoutDescriptor.entries = &bindGroupLayoutEntry;
    m_BindGroupLayouts[GROUP_MATERIAL_INDEX] = m_Device.createBindGroupLayout(bindGroupLayoutDescriptor);

    // Transform bind group layout
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.visibility = wgpu::ShaderStage::Vertex;
    bindGroupLayoutEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    bindGroupLayoutEntry.buffer.minBindingSize = sizeof(TransformBindGroupData);

    m_BindGroupLayouts[GROUP_TRANSFORM_INDEX] = m_Device.createBindGroupLayout(bindGroupLayoutDescriptor);

    {
        std::array<WGPUBindGroupLayoutEntry, 2> bindGroupLayoutEntries {
            WGPUBindGroupLayoutEntry {
                .nextInChain = nullptr,
                .binding = 0,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer = {
                    .nextInChain = nullptr,
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(Math::Matrix3x3)
                },
                .sampler = {},
                .texture = {},
                .storageTexture = {}
            },
            WGPUBindGroupLayoutEntry {
                .nextInChain = nullptr,
                .binding = 1,
                .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
                .buffer = {
                    .nextInChain = nullptr,
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(float)
                },
                .sampler = {},
                .texture = {},
                .storageTexture = {}
            }
        };
        m_BindGroupLayouts[GROUP_CAMERA_INDEX] = m_Device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
            .nextInChain = nullptr,
            .label = "Camera Bind Group Layout",
            .entryCount = bindGroupLayoutEntries.size(),
            .entries = bindGroupLayoutEntries.data()
        });
    }

    m_CameraBuffer = Buffer(m_Device, sizeof(Math::Matrix3x3), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);
    m_TimeBuffer = Buffer(m_Device, sizeof(float), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

    std::array<WGPUBindGroupEntry, 2> cameraBindGroupEntries {
        WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = m_CameraBuffer.get(),
            .offset = 0,
            .size = m_CameraBuffer.Size(),
            .sampler = nullptr,
            .textureView = nullptr
        },
        WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = 1,
            .buffer = m_TimeBuffer.get(),
            .offset = 0,
            .size = m_TimeBuffer.Size(),
            .sampler = nullptr,
            .textureView = nullptr
        }
    };

    wgpu::BindGroupDescriptor cameraBindGroupDescriptor{};
    cameraBindGroupDescriptor.layout = m_BindGroupLayouts[GROUP_CAMERA_INDEX];
    cameraBindGroupDescriptor.entryCount = cameraBindGroupEntries.size();
    cameraBindGroupDescriptor.entries = cameraBindGroupEntries.data();

    m_CameraBindGroup = m_Device.createBindGroup(cameraBindGroupDescriptor);

    // Pipeline layout
    wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor;
    pipelineLayoutDescriptor.bindGroupLayoutCount = m_BindGroupLayouts.size();
    pipelineLayoutDescriptor.bindGroupLayouts = (WGPUBindGroupLayout*)m_BindGroupLayouts.data();
    m_PipelineLayout = m_Device.createPipelineLayout(pipelineLayoutDescriptor);

    std::optional<WGPURenderPipeline> renderPipeline = CreateRenderPipeline("quad", true, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create quad render pipeline.");
        return false;
    }
    m_QuadRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("ellipse", true, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create ellipse render pipeline.");
        return false;
    }
    m_EllipseRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("lava", true, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create lava render pipeline.");
        return false;
    }
    m_LavaRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("gravity-zone", true, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create gravity zone render pipeline.");
        return false;
    }
    m_GravityZoneRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("radial-gravity-zone", true, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create radial gravity zone render pipeline.");
        return false;
    }
    m_RadialGravityZoneRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("checkpoint", true, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create checkpoint render pipeline.");
        return false;
    }
    m_CheckpointRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("exit", true, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create exit render pipeline.");
        return false;
    }
    m_ExitRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("quad", false, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create quad render pipeline.");
        return false;
    }
    m_QuadLightRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("ellipse", false, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create ellipse render pipeline.");
        return false;
    }
    m_EllipseLightRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("lava", false, m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create lava render pipeline.");
        return false;
    }
    m_LavaLightRenderPipeline = renderPipeline.value();

    if (!m_Lighting.Init(m_Device, m_ShaderLibrary))
    {
        Log::Error("Failed to init lighting.");
        return false;
    }

    if (!m_FontAtlas.Init(*this, s_FontAtlasWidth, s_FontAtlasHeight))
    {
        Log::Error("Failed to init font atlas.");
        return false;
    }
    bool success = m_FontAtlas.LoadFont(m_Queue, "assets/fonts/Roboto/Roboto-Regular.ttf", m_DefaultCharset, 32.0f);
    if (!success)
    {
        Log::Error("Failed to load font.");
        return false;
    }

    Resize();

#if DEBUG
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForOther(window);

#if __EMSCRIPTEN__
    ImGui::GetIO().IniFilename = nullptr;
#endif

    ImGui_ImplWGPU_InitInfo wgpuInitInfo{};
    wgpuInitInfo.Device = m_Device;
    wgpuInitInfo.RenderTargetFormat = m_Format;
    wgpuInitInfo.DepthStencilFormat = s_DepthStencilFormat;

    ImGui_ImplWGPU_Init(&wgpuInitInfo);
#endif

    return true;
}

void Renderer::NewFrame(float deltaTime)
{
    m_Time += deltaTime;

#if DEBUG
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#endif
    m_FontAtlas.NewFrame();
}

bool Renderer::Render(const Scene& scene, const Camera& camera)
{
    wgpu::TextureView textureView;
    {
        wgpu::SurfaceTexture surfaceTexture;
        m_Surface.getCurrentTexture(&surfaceTexture);
        if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success)
        {
            return false;
        }

        wgpu::TextureViewDescriptor viewDescriptor;
        viewDescriptor.nextInChain = nullptr;
        viewDescriptor.label = "Surface texture view";
        viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
        viewDescriptor.dimension = WGPUTextureViewDimension_2D;
        viewDescriptor.baseMipLevel = 0;
        viewDescriptor.mipLevelCount = 1;
        viewDescriptor.baseArrayLayer = 0;
        viewDescriptor.arrayLayerCount = 1;
        viewDescriptor.aspect = WGPUTextureAspect_All;
        textureView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);
        wgpuTextureRelease(surfaceTexture.texture);
    }

    wgpu::CommandEncoder commandEncoder = m_Device.createCommandEncoder();

    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = textureView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    Math::Color clearColor = scene.properties.backgroundColor;
    renderPassColorAttachment.clearValue = wgpu::Color{ clearColor.r, clearColor.g, clearColor.b, 0.0f };
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    WGPURenderPassDepthStencilAttachment renderPassDepthStencilAttachment {
        .view = m_DepthTextureView,
        .depthLoadOp = wgpu::LoadOp::Clear,
        .depthStoreOp = wgpu::StoreOp::Store,
        .depthClearValue = 0.0f,
        .depthReadOnly = false,
        .stencilLoadOp = wgpu::LoadOp::Undefined,
        .stencilStoreOp = wgpu::StoreOp::Undefined,
        .stencilClearValue = 0,
        .stencilReadOnly = true
    };

    wgpu::RenderPassDescriptor renderPassDescriptor;
    renderPassDescriptor.nextInChain = nullptr;
    renderPassDescriptor.colorAttachmentCount = 1;
    renderPassDescriptor.colorAttachments = &renderPassColorAttachment;
    renderPassDescriptor.depthStencilAttachment = &renderPassDepthStencilAttachment;
    renderPassDescriptor.timestampWrites = nullptr;

    wgpu::RenderPassEncoder renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

    Math::Matrix3x3 viewMatrix = camera.GetMatrix();
    m_CameraBuffer.Write(m_Queue, viewMatrix);
    m_TimeBuffer.Write(m_Queue, m_Time);
    renderEncoder.setBindGroup(2, m_CameraBindGroup, 0, nullptr);

    for (const std::unique_ptr<Entity>& entity : scene.entities)
    {
        UpdateEntity(entity.get());
        if (!renderHiddenEntities && entity->flags & (uint16_t)EntityFlags::Hidden)
        {
            continue;
        }

        DrawData& drawData = m_EntityDrawData[entity->id];
        if (drawData.empty)
        {
            Log::Debug("Create entity draw data (%)", entity->id);
            CreateDrawData(drawData);
        }
        TransformBindGroupData transformData {
            .transform = entity->transform.GetMatrix(),
            .zIndex = entity->zIndex
        };
        drawData.materialBuffer.Write(m_Queue, entity->material);
        drawData.transformBuffer.Write(m_Queue, transformData);
        renderEncoder.setBindGroup(GROUP_MATERIAL_INDEX, drawData.materialBindGroup, 0, nullptr);
        renderEncoder.setBindGroup(GROUP_TRANSFORM_INDEX, drawData.transformBindGroup, 0, nullptr);

        if ((entity->flags & (uint16_t)EntityFlags::Text) != 0)
        {
            // "Ag" is an approximation of the entire alphabet
            float height = m_FontAtlas.MeasureTextHeight("Ag");
            m_FontAtlas.RenderText(m_Queue, renderEncoder, entity->name, 1.0f, 2.0f / height, 0.0f);
            continue;
        }

        if (entity->flags & (uint16_t)EntityFlags::Lava)
        {
            renderEncoder.setPipeline(m_LavaRenderPipeline);
            renderEncoder.draw(4, 1, 0, 0);
            continue;
        }
        if (entity->flags & (uint16_t)EntityFlags::Checkpoint)
        {
            renderEncoder.setPipeline(m_CheckpointRenderPipeline);
            renderEncoder.draw(4, 1, 0, 0);
            continue;
        }
        if (entity->flags & (uint16_t)EntityFlags::Exit)
        {
            renderEncoder.setPipeline(m_ExitRenderPipeline);
            renderEncoder.draw(4, 1, 0, 0);
            continue;
        }

        switch (entity->shape)
        {
            case Shape::Rectangle:
                if (entity->flags & (uint16_t)EntityFlags::GravityZone)
                {
                    renderEncoder.setPipeline(m_GravityZoneRenderPipeline);
                }
                else
                {
                    renderEncoder.setPipeline(m_QuadRenderPipeline);
                }
                break;
            case Shape::Ellipse:
                if (entity->flags & (uint16_t)EntityFlags::GravityZone)
                {
                    renderEncoder.setPipeline(m_RadialGravityZoneRenderPipeline);
                }
                else
                {
                    renderEncoder.setPipeline(m_EllipseRenderPipeline);
                }
                break;
            default:
                assert(false);
        }

        renderEncoder.draw(4, 1, 0, 0);
    }
    renderEncoder.end();
    renderEncoder.release();

    RenderLighting(commandEncoder, m_Lighting.m_RadianceTextureView, scene, camera);
    m_Lighting.Render(m_Queue, commandEncoder, textureView);

#if DEBUG
    ImGui::Image(m_Lighting.m_JumpFlood.GetSDFTextureView(), ImVec2(256, 256));
    ImGui::Image(m_Lighting.m_RadianceTextureView, ImVec2(256, 256));
    for (wgpu::TextureView view : m_Lighting.m_CascadeTextureSliceViews)
    {
        ImGui::Image(view, ImVec2(256, 256));
    }
#endif

    renderPassColorAttachment.loadOp = wgpu::LoadOp::Load;
    renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

#if DEBUG
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    // TODO: FramebufferScale was incorrect while resizing to fullscreen, so we'll stick to (1, 1) for now. Dear ImGui bug?
    drawData->FramebufferScale = ImVec2(1.0f, 1.0f);
    ImGui_ImplWGPU_RenderDrawData(drawData, renderEncoder);
#endif

    renderEncoder.end();
    renderEncoder.release();

    wgpu::CommandBuffer commandBuffer = commandEncoder.finish();
    m_Queue.submit(1, &commandBuffer);
    commandEncoder.release();
    commandBuffer.release();

#ifndef __EMSCRIPTEN__
    m_Surface.present();
#endif

    textureView.release();

    return true;
}

void Renderer::RenderLighting(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView& textureView, const Scene& scene, const Camera& camera)
{
    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = textureView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{ 0.0f, 0.0f, 0.0f, 0.0f };
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    wgpu::RenderPassDescriptor renderPassDescriptor;
    renderPassDescriptor.nextInChain = nullptr;
    renderPassDescriptor.colorAttachmentCount = 1;
    renderPassDescriptor.colorAttachments = &renderPassColorAttachment;
    renderPassDescriptor.depthStencilAttachment = nullptr;
    renderPassDescriptor.timestampWrites = nullptr;

    wgpu::RenderPassEncoder renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

    Math::Matrix3x3 viewMatrix = camera.GetMatrix();
    m_CameraBuffer.Write(m_Queue, viewMatrix);
    renderEncoder.setBindGroup(2, m_CameraBindGroup, 0, nullptr);

    for (const std::unique_ptr<Entity>& entity : scene.entities)
    {
        if ((entity->flags & (uint16_t)EntityFlags::Light) == 0)
        {
            continue;
        }

        DrawData& drawData = m_EntityDrawData[entity->id];
        if (drawData.empty)
        {
            Log::Debug("Create entity draw data (%)", entity->id);
            CreateDrawData(drawData);
        }
        TransformBindGroupData transformData {
            .transform = entity->transform.GetMatrix(),
            .zIndex = entity->zIndex
        };
        drawData.materialBuffer.Write(m_Queue, entity->material);
        drawData.transformBuffer.Write(m_Queue, transformData);
        renderEncoder.setBindGroup(GROUP_MATERIAL_INDEX, drawData.materialBindGroup, 0, nullptr);
        renderEncoder.setBindGroup(GROUP_TRANSFORM_INDEX, drawData.transformBindGroup, 0, nullptr);

        if (entity->flags & (uint16_t)EntityFlags::Lava)
        {
            renderEncoder.setPipeline(m_LavaLightRenderPipeline);
            renderEncoder.draw(4, 1, 0, 0);
            continue;
        }

        switch (entity->shape)
        {
            case Shape::Rectangle:
                renderEncoder.setPipeline(m_QuadLightRenderPipeline);
                break;
            case Shape::Ellipse:
                renderEncoder.setPipeline(m_EllipseLightRenderPipeline);
                break;
            default:
                assert(false);
        }

        renderEncoder.draw(4, 1, 0, 0);
    }
    renderEncoder.end();
    renderEncoder.release();
}

void Renderer::Resize()
{
    int newWidth, newHeight;
    SDL_GetWindowSize(m_Window, &newWidth, &newHeight);
    if (m_Width == newWidth && m_Height == newHeight)
    {
        return;
    }
    m_Width = newWidth;
    m_Height = newHeight;

    Log::Debug("Resized window: %, %", m_Width, m_Height);

    wgpu::SurfaceConfiguration surfaceConfig;
    surfaceConfig.format = m_Format;
    surfaceConfig.nextInChain = nullptr;
    surfaceConfig.width = m_Width;
    surfaceConfig.height = m_Height;
    surfaceConfig.viewFormatCount = 0;
    surfaceConfig.viewFormats = nullptr;
    // TODO: Render to offscreen texture before blitting to the framebuffer
    surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    surfaceConfig.device = m_Device;
    surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
    surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
    m_Surface.configure(surfaceConfig);

    if (m_DepthTexture != nullptr)
    {
        m_DepthTextureView.release();
        m_DepthTexture.destroy();
        m_DepthTexture.release();
    }

    wgpu::TextureDescriptor depthTextureDescriptor;
    depthTextureDescriptor.dimension = wgpu::TextureDimension::_2D;
    depthTextureDescriptor.format = s_DepthStencilFormat;
    depthTextureDescriptor.mipLevelCount = 1;
    depthTextureDescriptor.sampleCount = 1;
    depthTextureDescriptor.size.width = m_Width;
    depthTextureDescriptor.size.height = m_Height;
    depthTextureDescriptor.size.depthOrArrayLayers = 1;
    depthTextureDescriptor.usage = wgpu::TextureUsage::RenderAttachment;
    depthTextureDescriptor.viewFormatCount = 1;
    depthTextureDescriptor.viewFormats = (WGPUTextureFormat*)&s_DepthStencilFormat;
    m_DepthTexture = m_Device.createTexture(depthTextureDescriptor);

    wgpu::TextureViewDescriptor depthTextureViewDescriptor;
    depthTextureViewDescriptor.aspect = wgpu::TextureAspect::DepthOnly;
    depthTextureViewDescriptor.baseArrayLayer = 0;
    depthTextureViewDescriptor.arrayLayerCount = 1;
    depthTextureViewDescriptor.baseMipLevel = 0;
    depthTextureViewDescriptor.mipLevelCount = 1;
    depthTextureViewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
    depthTextureViewDescriptor.format = s_DepthStencilFormat;
    m_DepthTextureView = m_DepthTexture.createView(depthTextureViewDescriptor);

    m_Lighting.FitToScreen(m_Width, m_Height);
}

std::optional<WGPURenderPipeline> Renderer::CreateRenderPipeline(const std::string &shaderName, bool depthStencil, wgpu::TextureFormat format)
{
    const Shader* shader = m_ShaderLibrary.GetShader(shaderName);
    if (!shader)
    {
        Log::Error("Shader '%' does not exist.", shaderName);
        return std::nullopt;
    }
    std::string vertexEntry = shaderName + "_vert";
    std::string fragmentEntry = shaderName + "_frag";
    std::replace(vertexEntry.begin(), vertexEntry.end(), '-', '_');
    std::replace(fragmentEntry.begin(), fragmentEntry.end(), '-', '_');

    const std::string pipelineName = shaderName + " Render Pipeline";

    WGPUColorTargetState colorTarget {
        .nextInChain = nullptr,
        .format = format,
        .blend = nullptr,
        .writeMask = wgpu::ColorWriteMask::All
    };

    WGPUFragmentState fragmentState {
        .nextInChain = nullptr,
        .module = shader->shaderModule,
        .entryPoint = fragmentEntry.c_str(),
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &colorTarget
    };

    WGPUDepthStencilState depthStencilState {
        .nextInChain = nullptr,
        .format = s_DepthStencilFormat,
        .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::Greater,
        .stencilFront = {},
        .stencilBack = {},
        .stencilReadMask = 0,
        .stencilWriteMask = 0,
        .depthBias = 0,
        .depthBiasSlopeScale = 0.0f,
        .depthBiasClamp = 0.0f
    };

    WGPURenderPipelineDescriptor pipelineDescriptor {
        .nextInChain = nullptr,
        .label = pipelineName.c_str(),
        .layout = m_PipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = shader->shaderModule,
            .entryPoint = vertexEntry.c_str(),
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
        .depthStencil = &depthStencilState,
        .multisample = {
            .nextInChain = nullptr,
            .count = 1,
            .mask = ~(uint32_t)0,
            .alphaToCoverageEnabled = false
        },
        .fragment = &fragmentState
    };

    if (!depthStencil)
    {
        pipelineDescriptor.depthStencil = nullptr;
    }

    return m_Device.createRenderPipeline(pipelineDescriptor);
}

void Renderer::ImGuiDebugTextures()
{
#if DEBUG
    ImGui::Image(m_FontAtlas.GetTextureView(), ImVec2(256, 256));
#endif
}

bool Renderer::LoadAdapterSync()
{
    wgpu::RequestAdapterOptions options;
    options.setDefault();
    options.compatibleSurface = m_Surface;

    bool requestEnded = false;
    bool requestSucceeded = false;
    auto handle = m_Instance.requestAdapter(options, [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, const char* /* message */) {
        if (status == wgpu::RequestAdapterStatus::Success)
        {
            m_Adapter = adapter;
            requestSucceeded = true;
        }
        requestEnded = true;
    });

#if __EMSCRIPTEN__
    while (!requestEnded)
    {
        emscripten_sleep(100);
    }
#endif

    assert(requestEnded);
    if (!requestSucceeded)
    {
        Log::Error("Failed to get WebGPU adapter.");
        return false;
    }
    return true;
}

bool Renderer::LoadDeviceSync()
{
    wgpu::DeviceDescriptor descriptor = {};
    descriptor.nextInChain = nullptr;
    descriptor.label = "Main Device";
    descriptor.requiredFeatureCount = 0;
    descriptor.requiredLimits = nullptr;
    descriptor.defaultQueue.nextInChain = nullptr;
    descriptor.defaultQueue.label = "Default Queue";
    descriptor.deviceLostCallback = [](WGPUDeviceLostReason reason, const char* message, void* /* userData */) {
        Log::Error("Device lost: reason %", reason);
        if (message)
        {
            Log::Error(message);
        }
    };
    bool requestEnded = false;
    bool requestSucceeded = false;
    auto handle = m_Adapter.requestDevice(descriptor, [&](wgpu::RequestDeviceStatus status, wgpu::Device device, const char* /* message */) {
        if (status == wgpu::RequestDeviceStatus::Success)
        {
            m_Device = device;
            requestSucceeded = true;
        }
        requestEnded = true;
    });

#if __EMSCRIPTEN__
    while (!requestEnded)
    {
        emscripten_sleep(100);
    }
#endif

    assert(requestEnded);
    if (!requestSucceeded)
    {
        Log::Error("Failed to get WebGPU device.");
        return false;
    }

    uncapturedErrorHandle = m_Device.setUncapturedErrorCallback([](wgpu::ErrorType type, const char* message) {
        Log::Error("Uncaptured device error: type %", (int)type);
        if (message)
        {
            Log::Error(message);
        }
    });

    return true;
}

void Renderer::CreateDrawData(DrawData& drawData)
{
    assert(drawData.empty);
    drawData.empty = false;

    drawData.materialBuffer = Buffer(m_Device, sizeof(Material), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);
    drawData.transformBuffer = Buffer(m_Device, sizeof(TransformBindGroupData), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

    wgpu::BindGroupEntry binding;
    binding.binding = 0;
    binding.buffer = drawData.materialBuffer.get();
    binding.offset = 0;
    binding.size = sizeof(Material);

    wgpu::BindGroupDescriptor bindGroupDescriptor{};
    bindGroupDescriptor.layout = m_BindGroupLayouts[GROUP_MATERIAL_INDEX];
    bindGroupDescriptor.entryCount = 1;
    bindGroupDescriptor.entries = &binding;

    drawData.materialBindGroup = m_Device.createBindGroup(bindGroupDescriptor);

    bindGroupDescriptor.layout = m_BindGroupLayouts[GROUP_TRANSFORM_INDEX];
    binding.buffer = drawData.transformBuffer.get();
    binding.size = sizeof(TransformBindGroupData);

    drawData.transformBindGroup = m_Device.createBindGroup(bindGroupDescriptor);
}

void Renderer::UpdateEntity(Entity* entity)
{
    std::string_view shaderName;
    if (entity->flags & (uint16_t)EntityFlags::Text)
    {
        shaderName = "text";
    }
    else if (entity->flags & (uint16_t)EntityFlags::Checkpoint)
    {
        shaderName = "checkpoint";
    }
    else if (entity->flags & (uint16_t)EntityFlags::Exit)
    {
        shaderName = "exit";
    }
    else if (entity->shape == Shape::Rectangle)
    {
        if (entity->flags & (uint16_t)EntityFlags::GravityZone)
        {
            shaderName = "gravity-zone";
        }
        else
        {
            shaderName = "quad";
        }
    }
    else if (entity->shape == Shape::Ellipse)
    {
        if (entity->flags & (uint16_t)EntityFlags::GravityZone)
        {
            shaderName = "radial-gravity-zone";
        }
        else
        {
            shaderName = "ellipse";
        }
    }
    else
    {
        Log::Error("Could not find shader for entity");
        return;
    }
    entity->shader = m_ShaderLibrary.GetShader((std::string)shaderName);
}
