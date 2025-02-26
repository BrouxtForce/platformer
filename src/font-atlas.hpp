#pragma once

#include <array>

#include <webgpu/webgpu.hpp>

#include "math.hpp"
#include "buffer.hpp"
#include "data-structures.hpp"

class Renderer;

class Charset
{
public:
    inline constexpr Charset(StringView str)
    {
        for (const char& c : str)
        {
            Insert(c);
        }
    }

    constexpr void Insert(char c)
    {
        m_Data[c / 64] |= (uint64_t)1 << (c % 64);
    }

    char PopChar();
    size_t size() const;

private:
    std::array<uint64_t, 4> m_Data{};
};

class FontAtlas
{
public:
    FontAtlas() = default;
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    bool Init(Renderer& renderer, int width, int height);

    // Returns true if successful
    bool LoadFont(wgpu::Queue queue, StringView path, const Charset& charset, float fontSize);

    float MeasureTextHeight(StringView text);

    void NewFrame();
    // Renders the text horizontally centered
    void RenderText(wgpu::Queue queue, wgpu::RenderPassEncoder renderEncoder, StringView text, float aspect, float size, Math::float2 position);

    inline wgpu::TextureView GetTextureView()
    {
        return m_TextureView;
    }

private:
    int m_Width = 0;
    int m_Height = 0;

    float m_FontSize = 0.0f;

    wgpu::RenderPipeline m_RenderPipeline;

    // TODO: Dynamic buffer resizing?
    static constexpr size_t s_QuadBufferSize = 16384;
    size_t m_QuadsWritten = 0;
    Buffer m_QuadBuffer;
    wgpu::BindGroup m_QuadBindGroup;
    int m_QuadBindGroupIndex = 0;

    wgpu::Sampler m_Sampler;
    wgpu::Texture m_Texture;
    wgpu::TextureView m_TextureView;

    struct Glyph
    {
        Math::float2 offset{};
        Math::float2 size{};

        Math::float2 texPosition{};
        Math::float2 texSize{};

        float advanceWidth = 0;
    };

    static constexpr char s_DefaultChar = '?';
    std::unordered_map<char, Glyph> m_GlyphMap;

    float GetTextWidth(StringView text) const;
    float GetTextHeight(StringView text) const;

    const Glyph& GetGlyph(char c) const;
};
