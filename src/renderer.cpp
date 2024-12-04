#include <stdio.h>

#include <sdl3webgpu.h>

#include "renderer.hpp"
#include "utility.hpp"

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
        printf("Failed to create WebGPU instance.");
        return false;
    }

    m_Surface = SDL_GetWGPUSurface(m_Instance, window);
    LoadAdapterSync();
    LoadDeviceSync();

    m_Queue = m_Device.getQueue();

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    wgpu::SurfaceConfiguration surfaceConfig;
    surfaceConfig.format = m_Surface.getPreferredFormat(m_Adapter);
    surfaceConfig.nextInChain = nullptr;
    surfaceConfig.width = windowWidth;
    surfaceConfig.height = windowHeight;
    surfaceConfig.viewFormatCount = 0;
    surfaceConfig.viewFormats = nullptr;
    surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
    surfaceConfig.device = m_Device;
    surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
    surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
    m_Surface.configure(surfaceConfig);

    std::optional<WGPURenderPipeline> renderPipeline = CreateRenderPipeline("quad", surfaceConfig.format);
    if (!renderPipeline.has_value())
    {
        return false;
    }
    quadRenderPipeline = renderPipeline.value();

    return true;
}

bool Renderer::Render()
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

#ifndef WEBGPU_BACKEND_WGPU
        wgpuTextureRelease(surfaceTexture.texture);
#endif
    }

    wgpu::CommandEncoder commandEncoder = m_Device.createCommandEncoder();

    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = textureView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{ 1.0, 0.0, 0.0, 1.0 };
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
    renderEncoder.setPipeline(quadRenderPipeline);
    renderEncoder.draw(6, 1, 0, 0);
    renderEncoder.end();

    wgpu::CommandBuffer commandBuffer = commandEncoder.finish();
    m_Queue.submit(1, &commandBuffer);

#ifndef __EMSCRIPTEN__
    m_Surface.present();
#endif

    return true;
}

std::optional<WGPURenderPipeline> Renderer::CreateRenderPipeline(const std::string &shader, wgpu::TextureFormat format)
{
    const std::string vertexEntry = shader + "_vert";
    const std::string fragmentEntry = shader + "_frag";

    // TODO: SDL_free
    static std::string basePath = SDL_GetBasePath();
#if __EMSCRIPTEN__
    // const std::string shaderPath = basePath + "shaders/" + shader + ".wgsl";
    const std::string shaderPath = "shaders/quad.wgsl";
#else
    const std::string shaderPath = basePath + "../shaders/" + shader + ".wgsl";
#endif
    const std::string shaderSource = ReadFile(shaderPath);
    if (shaderSource.empty())
    {
        printf("Failed to load shader '%s'\n", shaderPath.c_str());
        return std::nullopt;
    }

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor = {};
#ifdef WEBGPU_BACKEND_WGPU
    shaderModuleDescriptor.hintCount = 0;
    shaderModuleDescriptor.hints = nullptr;
#endif
    wgpu::ShaderModuleWGSLDescriptor wgslDescriptor;
    wgslDescriptor.chain.next = nullptr;
    wgslDescriptor.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    shaderModuleDescriptor.nextInChain = &wgslDescriptor.chain;
    wgslDescriptor.code = shaderSource.c_str();

    wgpu::ShaderModule shaderModule = m_Device.createShaderModule(shaderModuleDescriptor);

    wgpu::RenderPipelineDescriptor pipelineDescriptor;
    pipelineDescriptor.label = "Quad pipeline";

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

    pipelineDescriptor.fragment = &fragmentState;
    pipelineDescriptor.depthStencil = nullptr;

    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDescriptor.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDescriptor.primitive.frontFace = wgpu::FrontFace::CW;
    pipelineDescriptor.primitive.cullMode = wgpu::CullMode::None;

    pipelineDescriptor.multisample.count = 1;
    pipelineDescriptor.multisample.mask = ~0u;
    pipelineDescriptor.multisample.alphaToCoverageEnabled = false;

    pipelineDescriptor.layout = nullptr;

    return m_Device.createRenderPipeline(pipelineDescriptor);
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
        printf("Failed to get WebGPU adapter.");
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
        printf("Device lost: reason %i\n", (int)reason);
        if (message)
        {
            printf("%s\n", message);
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
        printf("Failed to get WebGPU device.");
        return false;
    }

    uncapturedErrorHandle = m_Device.setUncapturedErrorCallback([](wgpu::ErrorType type, const char* message) {
        printf("Uncaptured device error: type %i\n", (int)type);
        if (message)
        {
            printf("%s\n", message);
        }
    });

    return true;
}
