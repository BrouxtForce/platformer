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
#include "application.hpp"
#include "material.hpp"

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
    MaterialManager::Init(m_ShaderLibrary, m_Device);

    // TODO: Use the replacement for getPreferredFormat()
    m_Format = WGPUTextureFormat_BGRA8Unorm;

    // Material bind group layout
    wgpu::BindGroupLayoutEntry bindGroupLayoutEntry = wgpu::Default;
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindGroupLayoutEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    // TODO: Does this have to be zero?
    bindGroupLayoutEntry.buffer.minBindingSize = 0;

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
            .label = (StringView)"Camera Bind Group Layout",
            .entryCount = bindGroupLayoutEntries.size(),
            .entries = bindGroupLayoutEntries.data()
        });
    }

    m_CameraBuffer = m_Device.createBuffer(WGPUBufferDescriptor {
        // TODO: Correct?
        .nextInChain = nullptr,
        .label = WEBGPU_STRING_NULL,
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = sizeof(Math::Matrix3x3),
        .mappedAtCreation = false
    });
    m_TimeBuffer = m_Device.createBuffer(WGPUBufferDescriptor {
        .nextInChain = nullptr,
        // TODO: Correct?
        .label = WEBGPU_STRING_NULL,
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = sizeof(float),
        .mappedAtCreation = false
    });

    std::array<WGPUBindGroupEntry, 2> cameraBindGroupEntries {
        WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = m_CameraBuffer,
            .offset = 0,
            .size = m_CameraBuffer.getSize(),
            .sampler = nullptr,
            .textureView = nullptr
        },
        WGPUBindGroupEntry {
            .nextInChain = nullptr,
            .binding = 1,
            .buffer = m_TimeBuffer,
            .offset = 0,
            .size = m_TimeBuffer.getSize(),
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
        if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal)
        {
            return false;
        }

        wgpu::TextureViewDescriptor viewDescriptor;
        viewDescriptor.nextInChain = nullptr;
        viewDescriptor.label = (StringView)"Surface texture view";
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
    m_Queue.writeBuffer(m_CameraBuffer, 0, &viewMatrix, sizeof(viewMatrix));
    m_Queue.writeBuffer(m_TimeBuffer, 0, &m_Time, sizeof(m_Time));
    renderEncoder.setBindGroup(2, m_CameraBindGroup, 0, nullptr);

    for (Entity& entity : scene.entities)
    {
        if (!renderHiddenEntities && entity.flags & (uint16_t)EntityFlags::Hidden)
        {
            continue;
        }

        DrawData& drawData = m_EntityDrawData[entity.id];
        if (drawData.empty)
        {
            Log::Debug("Create entity draw data (%)", entity.id);
            CreateDrawData(drawData);
        }
        TransformBindGroupData transformData {
            .transform = entity.transform.GetMatrix(),
            .zIndex = entity.zIndex
        };

        assert(entity.material != nullptr);
        if (entity.material->updated)
        {
            entity.material->Flush(m_Queue);
            entity.material->updated = false;
        }
        m_Queue.writeBuffer(drawData.transformBuffer, 0, &transformData, sizeof(transformData));

        renderEncoder.setBindGroup(GROUP_MATERIAL_INDEX, entity.material->bindGroup, 0, nullptr);
        renderEncoder.setBindGroup(GROUP_TRANSFORM_INDEX, drawData.transformBindGroup, 0, nullptr);

        if ((entity.flags & (uint16_t)EntityFlags::Text) != 0)
        {
            // "Ag" is an approximation of the entire alphabet
            float height = m_FontAtlas.MeasureTextHeight("Ag");
            m_FontAtlas.RenderText(m_Queue, renderEncoder, entity.name, 1.0f, 2.0f / height, 0.0f);
            continue;
        }

        renderEncoder.setPipeline(GetRenderPipeline(entity.material, m_Format, true));
        renderEncoder.draw(4, 1, 0, 0);
    }
    renderEncoder.end();
    renderEncoder.release();

    RenderLighting(commandEncoder, m_Lighting.m_RadianceTextureView, scene, camera);
    m_Lighting.Render(m_Queue, commandEncoder, textureView);

#if DEBUG
    wgpu::TextureView jumpFloodTextureView = m_Lighting.m_JumpFlood.GetSDFTextureView();
    ImGui::Image(jumpFloodTextureView, ImVec2(256, 256));
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
    m_Queue.writeBuffer(m_CameraBuffer, 0, &viewMatrix, sizeof(viewMatrix));
    renderEncoder.setBindGroup(2, m_CameraBindGroup, 0, nullptr);

    for (const Entity& entity : scene.entities)
    {
        if ((entity.flags & (uint16_t)EntityFlags::Light) == 0)
        {
            continue;
        }

        DrawData& drawData = m_EntityDrawData[entity.id];
        if (drawData.empty)
        {
            Log::Debug("Create entity draw data (%)", entity.id);
            CreateDrawData(drawData);
        }
        TransformBindGroupData transformData {
            .transform = entity.transform.GetMatrix(),
            .zIndex = entity.zIndex
        };
        if (entity.material->updated)
        {
            entity.material->Flush(m_Queue);
            entity.material->updated = false;
        }
        m_Queue.writeBuffer(drawData.transformBuffer, 0, &transformData, sizeof(transformData));

        renderEncoder.setBindGroup(GROUP_MATERIAL_INDEX, entity.material->bindGroup, 0, nullptr);
        renderEncoder.setBindGroup(GROUP_TRANSFORM_INDEX, drawData.transformBindGroup, 0, nullptr);

        renderEncoder.setPipeline(GetRenderPipeline(entity.material, m_Format, false));

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

std::optional<WGPURenderPipeline> Renderer::CreateRenderPipeline(StringView shaderName, bool depthStencil, wgpu::TextureFormat format)
{
    const Shader* shader = m_ShaderLibrary.GetShader(shaderName);
    if (!shader)
    {
        Log::Error("Shader '%' does not exist.", shaderName);
        return std::nullopt;
    }
    String vertexEntry;
    vertexEntry.arena = &TransientArena;
    vertexEntry << shaderName << "_vert" << '\0';
    vertexEntry.ReplaceAll('-', '_');

    String fragmentEntry;
    fragmentEntry.arena = &TransientArena;
    fragmentEntry << shaderName << "_frag" << '\0';
    fragmentEntry.ReplaceAll('-', '_');

    String pipelineName;
    pipelineName.arena = &TransientArena;
    pipelineName << shaderName << " Render Pipeline";

    WGPUColorTargetState colorTarget {
        .nextInChain = nullptr,
        .format = format,
        .blend = nullptr,
        .writeMask = wgpu::ColorWriteMask::All
    };

    WGPUFragmentState fragmentState {
        .nextInChain = nullptr,
        .module = shader->shaderModule,
        // TODO: fragmentEntry does not have to be null-terminated
        .entryPoint = (StringView)fragmentEntry,
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &colorTarget
    };

    WGPUDepthStencilState depthStencilState {
        .nextInChain = nullptr,
        .format = s_DepthStencilFormat,
        .depthWriteEnabled = WGPUOptionalBool_True,
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
        // TODO: no null termination
        .label = (StringView)pipelineName,
        .layout = m_PipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = shader->shaderModule,
            // TODO: no null termination
            .entryPoint = (StringView)vertexEntry,
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

wgpu::RenderPipeline Renderer::GetRenderPipeline(Material* material, wgpu::TextureFormat textureFormat, bool depthStencil)
{
    const Shader* shader = material->shader;
    auto& renderPipelineMap = depthStencil ? m_RenderPipelineMap : m_LightRenderPipelineMap;

    auto it = renderPipelineMap.find(shader);
    if (it != renderPipelineMap.end())
    {
        return it->second;
    }

    String vertexEntry = String::Copy(material->shader->name, &TransientArena) << "_vert";
    vertexEntry.NullTerminate();
    vertexEntry.ReplaceAll('-', '_');

    String fragmentEntry = String::Copy(material->shader->name, &TransientArena) << "_frag";
    fragmentEntry.NullTerminate();
    fragmentEntry.ReplaceAll('-', '_');

    WGPUColorTargetState colorTarget {
        .nextInChain = nullptr,
        .format = textureFormat,
        .blend = nullptr,
        .writeMask = wgpu::ColorWriteMask::All
    };

    WGPUFragmentState fragmentState {
        .nextInChain = nullptr,
        .module = shader->shaderModule,
        // TODO: No null termination
        .entryPoint = (StringView)fragmentEntry,
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &colorTarget
    };

    WGPUDepthStencilState depthStencilState {
        .nextInChain = nullptr,
        .format = s_DepthStencilFormat,
        .depthWriteEnabled = WGPUOptionalBool_True,
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
        // TODO: Label?
        // TODO: Correct?
        .label = WEBGPU_STRING_NULL,
        .layout = m_PipelineLayout,
        .vertex = {
            .nextInChain = nullptr,
            .module = shader->shaderModule,
            // TODO: No null termination
            .entryPoint = (StringView)vertexEntry.data,
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

    wgpu::RenderPipeline renderPipeline = m_Device.createRenderPipeline(pipelineDescriptor);
    renderPipelineMap.insert({ shader, renderPipeline });
    return renderPipeline;
}

bool Renderer::LoadAdapterSync()
{
    wgpu::RequestAdapterOptions options;
    options.setDefault();
    options.compatibleSurface = m_Surface;

    struct RequestInfo
    {
        WGPUAdapter adapter = nullptr;
        bool ended = false;
    };
    RequestInfo requestInfo;

#if __EMSCRIPTEN__
    auto callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata)
#else
    auto callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata, void* /* userdata2 */)
#endif
    {
        RequestInfo& requestInfo = *(RequestInfo*)userdata;

        if (status == WGPURequestAdapterStatus_Success)
        {
            requestInfo.adapter = adapter;
        }
        else
        {
            Log::Error(message);
        }
        requestInfo.ended = true;
    };

#if __EMSCRIPTEN__
    wgpuInstanceRequestAdapter(m_Instance, &options, callback, &requestInfo);

    while (!requestInfo.ended)
    {
        emscripten_sleep(100);
    }
#else
    WGPURequestAdapterCallbackInfo callbackInfo {
        .nextInChain = nullptr,
        // TODO: What should the mode be?
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = callback,
        .userdata1 = &requestInfo
    };
    wgpuInstanceRequestAdapter(m_Instance, &options, callbackInfo);
#endif

    assert(requestInfo.ended);
    m_Adapter = requestInfo.adapter;

    if (!m_Adapter)
    {
        Log::Error("Failed to get WebGPU adapter.");
        return false;
    }
    return true;
}

#if !__EMSCRIPTEN__
void UncapturedErrorCallback(const WGPUDevice* /* device */, WGPUErrorType type, WGPUStringView message, void* /* userdata1 */, void* /* userdata2 */)
{
    StringView typeString;
    switch (type)
    {
        case WGPUErrorType_Validation:  typeString = "Validation"; break;
        case WGPUErrorType_OutOfMemory: typeString = "Out of memory"; break;
        case WGPUErrorType_Internal:    typeString = "Internal"; break;
        case WGPUErrorType_Unknown:     typeString = "Unknown"; break;

        default:
            Log::Error("Invalid error type: %", type);
    }

    Log::Error("WebGPU % error:\n%", typeString, message);
}
#endif

bool Renderer::LoadDeviceSync()
{
    WGPUDeviceDescriptor descriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Main Device",
        .requiredFeatureCount = 0,
        .requiredFeatures = nullptr,
        .requiredLimits = nullptr,
        .defaultQueue = {
            .nextInChain = nullptr,
            .label = (StringView)"Default Queue"
        },
        // TODO: Implement for emscripten
#if !__EMSCRIPTEN__
        // TODO: Set device lost callback
        .deviceLostCallbackInfo = {},
        .uncapturedErrorCallbackInfo = {
            .nextInChain = nullptr,
            .callback = UncapturedErrorCallback
        }
#endif
    };

    struct RequestInfo
    {
        WGPUDevice device = nullptr;
        bool ended = false;
    };
    RequestInfo requestInfo;

#if __EMSCRIPTEN__
    auto callback = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userdata)
#else
    auto callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata, void* /* userdata2 */)
#endif
    {
        RequestInfo& requestInfo = *(RequestInfo*)userdata;

        if (status == WGPURequestDeviceStatus_Success)
        {
            requestInfo.device = device;
        }
        else
        {
            Log::Error(message);
        }
        requestInfo.ended = true;
    };

#if __EMSCRIPTEN__
    wgpuAdapterRequestDevice(m_Adapter, &descriptor, callback, &requestInfo);

    while (!requestInfo.ended)
    {
        emscripten_sleep(100);
    }
#else
    WGPURequestDeviceCallbackInfo callbackInfo {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = callback,
        .userdata1 = &requestInfo,
    };
    wgpuAdapterRequestDevice(m_Adapter, &descriptor, callbackInfo);
#endif

    assert(requestInfo.ended);
    m_Device = requestInfo.device;

    if (!m_Device)
    {
        Log::Error("Failed to get WebGPU device.");
        return false;
    }
    return true;
}

void Renderer::CreateDrawData(DrawData& drawData)
{
    assert(drawData.empty);
    drawData.empty = false;

    drawData.transformBuffer = m_Device.createBuffer(WGPUBufferDescriptor {
        .nextInChain = nullptr,
        // TODO: Correct?
        .label = WEBGPU_STRING_NULL,
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = sizeof(TransformBindGroupData),
        .mappedAtCreation = false
    });

    wgpu::BindGroupEntry binding;
    binding.binding = 0;
    binding.buffer = drawData.transformBuffer;
    binding.offset = 0;
    binding.size = sizeof(TransformBindGroupData);

    wgpu::BindGroupDescriptor bindGroupDescriptor{};
    bindGroupDescriptor.layout = m_BindGroupLayouts[GROUP_TRANSFORM_INDEX];
    bindGroupDescriptor.entryCount = 1;
    bindGroupDescriptor.entries = &binding;

    drawData.transformBindGroup = m_Device.createBindGroup(bindGroupDescriptor);
}
