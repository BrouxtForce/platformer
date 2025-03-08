#pragma once

#include <optional>
#include <unordered_map>

#include <webgpu/webgpu.hpp>
#include <SDL3/SDL.h>

#include "scene.hpp"
#include "camera.hpp"
#include "font-atlas.hpp"
#include "shader-library.hpp"
#include "lighting.hpp"

class Renderer
{
public:
    bool renderHiddenEntities = false;

    Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Init(SDL_Window* window);

    void NewFrame(float deltaTime);
    bool Render(const Scene& scene, const Camera& camera);

    void Resize();

    inline int GetWidth()  { return m_Width; }
    inline int GetHeight() { return m_Height; }

    inline float GetTime() { return m_Time; }

    std::optional<WGPURenderPipeline> CreateRenderPipeline(StringView shader, bool depthStencil, wgpu::TextureFormat format);

    void ImGuiDebugTextures();

private:
    SDL_Window* m_Window = nullptr;

    int m_Width = 0;
    int m_Height = 0;

    wgpu::Instance m_Instance;
    wgpu::Adapter m_Adapter;
    wgpu::Device m_Device;
    wgpu::Queue m_Queue;
    wgpu::Surface m_Surface;

    ShaderLibrary m_ShaderLibrary;

    wgpu::TextureFormat m_Format = wgpu::TextureFormat::Undefined;

    std::unordered_map<const Shader*, wgpu::RenderPipeline> m_RenderPipelineMap;
    std::unordered_map<const Shader*, wgpu::RenderPipeline> m_LightRenderPipelineMap;

    wgpu::RenderPipeline GetRenderPipeline(Material* material, wgpu::TextureFormat textureFormat, bool depthStencil);

    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorHandle;

    void RenderLighting(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView& textureView, const Scene& scene, const Camera& camera);

    bool LoadAdapterSync();
    bool LoadDeviceSync();

    struct TransformBindGroupData
    {
        Math::Matrix3x3 transform;
        uint32_t zIndex;
        uint32_t padding[3] {};
    };

    constexpr static int GROUP_MATERIAL_INDEX = 0;
    constexpr static int GROUP_TRANSFORM_INDEX = 1;
    constexpr static int GROUP_CAMERA_INDEX = 2;
    std::array<wgpu::BindGroupLayout, 3> m_BindGroupLayouts;
    wgpu::PipelineLayout m_PipelineLayout;

    constexpr static WGPUTextureFormat s_DepthStencilFormat = WGPUTextureFormat_Depth16Unorm;
    wgpu::Texture m_DepthTexture;
    wgpu::TextureView m_DepthTextureView;

    struct DrawData
    {
        bool empty = true;

        wgpu::Buffer transformBuffer;
        wgpu::BindGroup transformBindGroup;
    };
    std::unordered_map<uint32_t, DrawData> m_EntityDrawData;

    void CreateDrawData(DrawData& drawData);

    wgpu::Buffer m_CameraBuffer;
    wgpu::Buffer m_TimeBuffer;
    float m_Time = 0.0f;
    wgpu::BindGroup m_CameraBindGroup;

    Lighting m_Lighting;

    constexpr static int s_FontAtlasWidth = 256;
    constexpr static int s_FontAtlasHeight = 256;
    constexpr static Charset m_DefaultCharset = Charset("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789. []");
    FontAtlas m_FontAtlas;

    friend class FontAtlas;
};
