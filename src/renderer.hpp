#pragma once

#include <optional>

#include <webgpu/webgpu.hpp>
#include <SDL3/SDL.h>

class Renderer
{
public:
    Renderer() = default;

    bool Init(SDL_Window* window);
    bool Render();

    std::optional<WGPURenderPipeline> CreateRenderPipeline(const std::string& shader, wgpu::TextureFormat format);

private:
    SDL_Window* m_Window = nullptr;

    wgpu::Instance m_Instance;
    wgpu::Adapter m_Adapter;
    wgpu::Device m_Device;
    wgpu::Queue m_Queue;
    wgpu::Surface m_Surface;

    wgpu::RenderPipeline quadRenderPipeline;

    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorHandle;

    bool LoadAdapterSync();
    bool LoadDeviceSync();
};
