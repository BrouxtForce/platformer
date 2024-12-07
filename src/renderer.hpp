#pragma once

#include <optional>
#include <unordered_map>

#include <webgpu/webgpu.hpp>
#include <SDL3/SDL.h>

#include "scene.hpp"
#include "buffer.hpp"
#include "camera.hpp"

class Renderer
{
public:
    Renderer() = default;

    bool Init(SDL_Window* window);
    bool Render(const Scene& scene, const Camera& camera);

    inline int GetWidth()  { return m_Width; }
    inline int GetHeight() { return m_Height; }

    std::optional<WGPURenderPipeline> CreateRenderPipeline(const std::string& shader, wgpu::TextureFormat format);

private:
    SDL_Window* m_Window = nullptr;

    int m_Width = 0;
    int m_Height = 0;

    wgpu::Instance m_Instance;
    wgpu::Adapter m_Adapter;
    wgpu::Device m_Device;
    wgpu::Queue m_Queue;
    wgpu::Surface m_Surface;

    wgpu::RenderPipeline quadRenderPipeline;
    wgpu::RenderPipeline ellipseRenderPipeline;

    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorHandle;

    bool LoadAdapterSync();
    bool LoadDeviceSync();

    constexpr static int GROUP_MATERIAL_INDEX = 0;
    constexpr static int GROUP_TRANSFORM_INDEX = 1;
    constexpr static int GROUP_CAMERA_INDEX = 2;
    std::array<wgpu::BindGroupLayout, 3> m_BindGroupLayouts;
    wgpu::PipelineLayout m_PipelineLayout;

    struct DrawData
    {
        bool empty = true;

        Buffer materialBuffer;
        wgpu::BindGroup materialBindGroup;

        Buffer transformBuffer;
        wgpu::BindGroup transformBindGroup;
    };
    std::unordered_map<uint32_t, DrawData> m_EntityDrawData;

    Buffer m_CameraBuffer;
    wgpu::BindGroup m_CameraBindGroup;
};
