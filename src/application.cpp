#include "application.hpp"

#include <sdl3webgpu.h>
#include "utility.hpp"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

bool Application::CreateRenderPipeline(wgpu::RenderPipeline& renderPipeline, const std::string& shader, wgpu::TextureFormat format)
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
        return false;
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

    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderModuleDescriptor);

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

    renderPipeline = device.createRenderPipeline(pipelineDescriptor);

    return true;
}

bool Application::Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }

    window = SDL_CreateWindow("", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (window == nullptr)
    {
        return false;
    }

#if __EMSCRIPTEN__
    instance = wgpuCreateInstance(nullptr);
#else
    wgpu::InstanceDescriptor desc = {};
    instance = wgpu::createInstance(desc);
#endif

    if (!instance) {
        printf("Failed to create WebGPU instance.");
        return false;
    }

    surface = SDL_GetWGPUSurface(instance, window);

    // Get adapter
    {
        wgpu::RequestAdapterOptions options;
        options.setDefault();
        options.compatibleSurface = surface;

        bool requestEnded = false;
        bool requestSucceeded = false;
        auto handle = instance.requestAdapter(options, [&](wgpu::RequestAdapterStatus status, wgpu::Adapter newAdapter, const char* /* message */) {
            if (status == wgpu::RequestAdapterStatus::Success)
            {
                adapter = newAdapter;
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
    }

    // Get device
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
        auto handle = adapter.requestDevice(descriptor, [&](wgpu::RequestDeviceStatus status, wgpu::Device newDevice, const char* /* message */) {
            if (status == wgpu::RequestDeviceStatus::Success)
            {
                device = newDevice;
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

        uncapturedErrorHandle = device.setUncapturedErrorCallback([](wgpu::ErrorType type, const char* message) {
            printf("Uncaptured device error: type %i\n", (int)type);
            if (message)
            {
                printf("%s\n", message);
            }
        });
    }

    queue = device.getQueue();

    wgpu::SurfaceConfiguration surfaceConfig;
    surfaceConfig.format = surface.getPreferredFormat(adapter);
    surfaceConfig.nextInChain = nullptr;
    surfaceConfig.width = WINDOW_WIDTH;
    surfaceConfig.height = WINDOW_HEIGHT;
    surfaceConfig.viewFormatCount = 0;
    surfaceConfig.viewFormats = nullptr;
    surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
    surfaceConfig.device = device;
    surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
    surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
    surface.configure(surfaceConfig);

    bool success = CreateRenderPipeline(quadRenderPipeline, "quad", surfaceConfig.format);
    if (!success)
    {
        return false;
    }

    return true;
}

bool Application::Loop()
{
    wgpu::TextureView textureView;
    {
        wgpu::SurfaceTexture surfaceTexture;
        surface.getCurrentTexture(&surfaceTexture);
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

    wgpu::CommandEncoder commandEncoder = device.createCommandEncoder();

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
    queue.submit(1, &commandBuffer);

#ifndef __EMSCRIPTEN__
    surface.present();
#endif

    return true;
}

void Application::Exit()
{
    SDL_DestroyWindow(window);
}
