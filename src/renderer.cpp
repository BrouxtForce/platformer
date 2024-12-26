#include <sdl3webgpu.h>
#include <webgpu/webgpu.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>

#include "math.hpp"
#include "renderer.hpp"
#include "utility.hpp"
#include "log.hpp"

wgpu::TextureFormat Renderer::m_DepthStencilFormat = wgpu::TextureFormat::Depth16Unorm;

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
    Resize();

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

    bindGroupLayoutEntry.buffer.minBindingSize = sizeof(Math::Matrix3x3);
    m_BindGroupLayouts[GROUP_CAMERA_INDEX] = m_Device.createBindGroupLayout(bindGroupLayoutDescriptor);

    m_CameraBuffer = Buffer(m_Device, sizeof(Math::Matrix3x3), wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

    wgpu::BindGroupEntry cameraBindGroupEntry;
    cameraBindGroupEntry.binding = 0;
    cameraBindGroupEntry.buffer = m_CameraBuffer.get();
    cameraBindGroupEntry.offset = 0;
    cameraBindGroupEntry.size = sizeof(Math::Matrix3x3);

    wgpu::BindGroupDescriptor cameraBindGroupDescriptor{};
    cameraBindGroupDescriptor.layout = m_BindGroupLayouts[GROUP_CAMERA_INDEX];
    cameraBindGroupDescriptor.entryCount = 1;
    cameraBindGroupDescriptor.entries = &cameraBindGroupEntry;

    m_CameraBindGroup = m_Device.createBindGroup(cameraBindGroupDescriptor);

    // Pipeline layout
    wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor;
    pipelineLayoutDescriptor.bindGroupLayoutCount = m_BindGroupLayouts.size();
    pipelineLayoutDescriptor.bindGroupLayouts = (WGPUBindGroupLayout*)m_BindGroupLayouts.data();
    m_PipelineLayout = m_Device.createPipelineLayout(pipelineLayoutDescriptor);

    std::optional<WGPURenderPipeline> renderPipeline = CreateRenderPipeline("quad", m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create quad render pipeline.");
        return false;
    }
    quadRenderPipeline = renderPipeline.value();

    renderPipeline = CreateRenderPipeline("ellipse", m_Format);
    if (!renderPipeline.has_value())
    {
        Log::Error("Failed to create ellipse render pipeline.");
        return false;
    }
    ellipseRenderPipeline = renderPipeline.value();

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForOther(window);

#if __EMSCRIPTEN__
    ImGui::GetIO().IniFilename = nullptr;
#endif

    ImGui_ImplWGPU_InitInfo wgpuInitInfo{};
    wgpuInitInfo.Device = m_Device;
    wgpuInitInfo.RenderTargetFormat = m_Format;
    wgpuInitInfo.DepthStencilFormat = m_DepthStencilFormat;

    ImGui_ImplWGPU_Init(&wgpuInitInfo);

    return true;
}

void Renderer::NewFrame()
{
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
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
    renderPassColorAttachment.clearValue = wgpu::Color{ clearColor.r, clearColor.g, clearColor.b, clearColor.a };
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    wgpu::RenderPassDepthStencilAttachment renderPassDepthStencilAttachment = wgpu::Default;
    renderPassDepthStencilAttachment.view = m_DepthTextureView;
    renderPassDepthStencilAttachment.depthClearValue = 0.0f;
    renderPassDepthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    renderPassDepthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    renderPassDepthStencilAttachment.depthReadOnly = false;

    wgpu::RenderPassDescriptor renderPassDescriptor;
    renderPassDescriptor.nextInChain = nullptr;
    renderPassDescriptor.colorAttachmentCount = 1;
    renderPassDescriptor.colorAttachments = &renderPassColorAttachment;
    renderPassDescriptor.depthStencilAttachment = &renderPassDepthStencilAttachment;
    renderPassDescriptor.timestampWrites = nullptr;

    wgpu::RenderPassEncoder renderEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

    Math::Matrix3x3 viewMatrix = camera.GetMatrix();
    m_CameraBuffer.Write(m_Queue, viewMatrix);
    renderEncoder.setBindGroup(2, m_CameraBindGroup, 0, nullptr);

    for (const std::unique_ptr<Entity>& entity : scene.entities)
    {
        if ((entity->flags & (uint16_t)EntityFlags::GravityZone) != 0 ||
            (entity->flags & (uint16_t)EntityFlags::Hidden) != 0)
        {
            continue;
        }

        DrawData& drawData = m_EntityDrawData[entity->id];
        if (drawData.empty)
        {
            Log::Debug("Create entity draw data (" + std::to_string(entity->id) + ")");

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

        switch (entity->shape)
        {
            case Shape::Rectangle:
                renderEncoder.setPipeline(quadRenderPipeline);
                break;
            case Shape::Ellipse:
                renderEncoder.setPipeline(ellipseRenderPipeline);
                break;
            default:
                assert(false);
        }

        renderEncoder.draw(6, 1, 0, 0);
    }

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    // TODO: FramebufferScale was incorrect while resizing to fullscreen, so we'll stick to (1, 1) for now. Dear ImGui bug?
    drawData->FramebufferScale = ImVec2(1.0f, 1.0f);
    ImGui_ImplWGPU_RenderDrawData(drawData, renderEncoder);
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

    Log::Debug("Resized window: " + std::to_string(m_Width) + ", " + std::to_string(m_Height));

    wgpu::SurfaceConfiguration surfaceConfig;
    surfaceConfig.format = m_Format;
    surfaceConfig.nextInChain = nullptr;
    surfaceConfig.width = m_Width;
    surfaceConfig.height = m_Height;
    surfaceConfig.viewFormatCount = 0;
    surfaceConfig.viewFormats = nullptr;
    surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
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
    depthTextureDescriptor.format = m_DepthStencilFormat;
    depthTextureDescriptor.mipLevelCount = 1;
    depthTextureDescriptor.sampleCount = 1;
    depthTextureDescriptor.size.width = m_Width;
    depthTextureDescriptor.size.height = m_Height;
    depthTextureDescriptor.size.depthOrArrayLayers = 1;
    depthTextureDescriptor.usage = wgpu::TextureUsage::RenderAttachment;
    depthTextureDescriptor.viewFormatCount = 1;
    depthTextureDescriptor.viewFormats = (WGPUTextureFormat*)&m_DepthStencilFormat;
    m_DepthTexture = m_Device.createTexture(depthTextureDescriptor);

    wgpu::TextureViewDescriptor depthTextureViewDescriptor;
    depthTextureViewDescriptor.aspect = wgpu::TextureAspect::DepthOnly;
    depthTextureViewDescriptor.baseArrayLayer = 0;
    depthTextureViewDescriptor.arrayLayerCount = 1;
    depthTextureViewDescriptor.baseMipLevel = 0;
    depthTextureViewDescriptor.mipLevelCount = 1;
    depthTextureViewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
    depthTextureViewDescriptor.format = m_DepthStencilFormat;
    m_DepthTextureView = m_DepthTexture.createView(depthTextureViewDescriptor);
}

std::optional<WGPURenderPipeline> Renderer::CreateRenderPipeline(const std::string &shader, wgpu::TextureFormat format)
{
    wgpu::ShaderModule shaderModule = m_ShaderLibrary.GetShaderModule(shader);
    if (!shaderModule)
    {
        Log::Error("Shader '" + shader + "' does not exist.");
        return std::nullopt;
    }
    const std::string vertexEntry = shader + "_vert";
    const std::string fragmentEntry = shader + "_frag";

    wgpu::RenderPipelineDescriptor pipelineDescriptor;
    const std::string pipelineName = shader + " Render Pipeline";
    pipelineDescriptor.label = pipelineName.c_str();

    pipelineDescriptor.vertex.bufferCount = 0;
    pipelineDescriptor.vertex.buffers = nullptr;
    pipelineDescriptor.vertex.module = shaderModule;
    pipelineDescriptor.vertex.entryPoint = vertexEntry.c_str();
    pipelineDescriptor.vertex.constantCount = 0;
    pipelineDescriptor.vertex.constants = nullptr;

    wgpu::FragmentState fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = fragmentEntry.c_str();
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = format;
    colorTarget.blend = nullptr;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    wgpu::DepthStencilState depthStencilState = wgpu::Default;
    depthStencilState.depthCompare = wgpu::CompareFunction::Greater;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = m_DepthStencilFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;

    pipelineDescriptor.fragment = &fragmentState;
    pipelineDescriptor.depthStencil = &depthStencilState;

    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDescriptor.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDescriptor.primitive.frontFace = wgpu::FrontFace::CW;
    pipelineDescriptor.primitive.cullMode = wgpu::CullMode::None;

    pipelineDescriptor.multisample.count = 1;
    pipelineDescriptor.multisample.mask = ~0u;
    pipelineDescriptor.multisample.alphaToCoverageEnabled = false;

    pipelineDescriptor.layout = m_PipelineLayout;

    return m_Device.createRenderPipeline(pipelineDescriptor);
}

void Renderer::ImGuiDebugTextures()
{
    ImGui::Image(m_FontAtlas.GetTextureView(), ImVec2(256, 256));
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
        Log::Error("Device lost: reason " + std::to_string(reason));
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
        Log::Error("Uncaptured device error: type " + std::to_string(type));
        if (message)
        {
            Log::Error(message);
        }
    });

    return true;
}
