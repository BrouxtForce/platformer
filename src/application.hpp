#pragma once

#include <memory>

#include <SDL3/SDL.h>
#include <webgpu/webgpu.hpp>

class Application
{
public:
    Application() = default;

    [[nodiscard]]
    bool CreateRenderPipeline(wgpu::RenderPipeline& renderPipeline, const std::string& shader, wgpu::TextureFormat format);

    bool Init();
    bool Loop();
    void Exit();

private:
    static constexpr int WINDOW_WIDTH = 640;
    static constexpr int WINDOW_HEIGHT = 480;

    SDL_Window* window = nullptr;
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Surface surface;

    wgpu::RenderPipeline quadRenderPipeline;

    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorHandle;
};
