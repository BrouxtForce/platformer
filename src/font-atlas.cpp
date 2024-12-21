#include "font-atlas.hpp"

#include <SDL3/SDL.h>
#include <cmath>

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#include <stb/stb_truetype.h>

#include "log.hpp"
#include "utility.hpp"
#include "renderer.hpp"

void Charset::Insert(char c)
{
    m_Data[c / 64] |= (uint64_t)1 << (c % 64);
}

char Charset::PopChar()
{
    for (int i = 0; i < (int)m_Data.size(); i++)
    {
        int right = std::countr_zero(m_Data[i]);
        if (right < 64)
        {
            m_Data[i] &= ~((uint64_t)1 << right);
            return i * 64 + right;
        }
    }
    return 0;
}

size_t Charset::size() const
{
    size_t sum = 0;
    for (int i = 0; i < (int)m_Data.size(); i++)
    {
        sum += std::popcount(m_Data[i]);
    }
    return sum;
}

FontAtlas::~FontAtlas()
{
    if (m_RenderPipeline) m_RenderPipeline.release();
    if (m_QuadBindGroup) m_QuadBindGroup.release();
    if (m_Texture) m_Texture.destroy();
}

void FontAtlas::Init(Renderer& renderer, int width, int height)
{
    m_Width = width;
    m_Height = height;

    wgpu::TextureDescriptor textureDescriptor = wgpu::Default;
    textureDescriptor.size.width = m_Width;
    textureDescriptor.size.height = m_Height;
    textureDescriptor.label = "Font Atlas Texture";
    textureDescriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    textureDescriptor.format = wgpu::TextureFormat::R8Unorm;
    m_Texture = renderer.m_Device.createTexture(textureDescriptor);

    wgpu::TextureViewDescriptor textureViewDescriptor = wgpu::Default;
    textureViewDescriptor.aspect = wgpu::TextureAspect::All;
    textureViewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
    textureViewDescriptor.format = textureDescriptor.format;
    textureViewDescriptor.arrayLayerCount = 1;
    textureViewDescriptor.mipLevelCount = 1;
    m_TextureView = m_Texture.createView(textureViewDescriptor);

    wgpu::SamplerDescriptor samplerDescriptor = wgpu::Default;
    samplerDescriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
    samplerDescriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
    samplerDescriptor.minFilter = wgpu::FilterMode::Linear;
    samplerDescriptor.magFilter = wgpu::FilterMode::Linear;
    samplerDescriptor.maxAnisotropy = 1.0f;
    m_Sampler = renderer.m_Device.createSampler(samplerDescriptor);

    m_QuadBuffer = Buffer(renderer.m_Device, s_QuadBufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);

    std::array<WGPUBindGroupLayoutEntry, 3> bindGroupLayoutEntries {
        WGPUBindGroupLayoutEntry {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer.type = wgpu::BufferBindingType::ReadOnlyStorage,
            .buffer.minBindingSize = s_QuadBufferSize,
        },
        WGPUBindGroupLayoutEntry {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture.sampleType = wgpu::TextureSampleType::Float,
            .texture.viewDimension = wgpu::TextureViewDimension::_2D
        },
        WGPUBindGroupLayoutEntry {
            .binding = 2,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler.type = wgpu::SamplerBindingType::Filtering
        }
    };

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor = wgpu::Default;
    bindGroupLayoutDescriptor.entryCount = bindGroupLayoutEntries.size();
    bindGroupLayoutDescriptor.entries = bindGroupLayoutEntries.data();

    wgpu::BindGroupLayout bindGroupLayout = renderer.m_Device.createBindGroupLayout(bindGroupLayoutDescriptor);

    std::array<WGPUBindGroupEntry, 3> bindGroupEntries {
        WGPUBindGroupEntry {
            .binding = 0,
            .buffer = m_QuadBuffer.get(),
            .offset = 0,
            .size = s_QuadBufferSize
        },
        WGPUBindGroupEntry {
            .binding = 1,
            .textureView = m_TextureView
        },
        WGPUBindGroupEntry {
            .binding = 2,
            .sampler = m_Sampler
        }
    };

    wgpu::BindGroupDescriptor bindGroupDescriptor = wgpu::Default;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entryCount = bindGroupEntries.size();
    bindGroupDescriptor.entries = bindGroupEntries.data();

    m_QuadBindGroup = renderer.m_Device.createBindGroup(bindGroupDescriptor);

    std::vector<WGPUBindGroupLayout> packedBindGroupLayouts;
    packedBindGroupLayouts.reserve(renderer.m_BindGroupLayouts.size() + 1);
    for (WGPUBindGroupLayout layout : renderer.m_BindGroupLayouts)
    {
        packedBindGroupLayouts.push_back(layout);
    }
    packedBindGroupLayouts.push_back(bindGroupLayout);

    wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor = wgpu::Default;
    pipelineLayoutDescriptor.bindGroupLayouts = packedBindGroupLayouts.data();
    pipelineLayoutDescriptor.bindGroupLayoutCount = packedBindGroupLayouts.size();
    m_QuadBindGroupIndex = packedBindGroupLayouts.size() - 1;

    wgpu::PipelineLayout pipelineLayout = renderer.m_Device.createPipelineLayout(pipelineLayoutDescriptor);

    const std::string shaderSource = ReadFile("shaders/text.wgsl");

    wgpu::ShaderModuleWGSLDescriptor wgslDescriptor = wgpu::Default;
    wgslDescriptor.chain.next = nullptr;
    wgslDescriptor.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgslDescriptor.code = shaderSource.c_str();

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor = wgpu::Default;
    shaderModuleDescriptor.nextInChain = &wgslDescriptor.chain;

    wgpu::ShaderModule shaderModule = renderer.m_Device.createShaderModule(shaderModuleDescriptor);

    wgpu::RenderPipelineDescriptor pipelineDescriptor = wgpu::Default;
    pipelineDescriptor.label = "Font Atlas Render Pipeline";
    pipelineDescriptor.layout = pipelineLayout;

    pipelineDescriptor.vertex.module = shaderModule;
    pipelineDescriptor.vertex.entryPoint = "text_vert";

    wgpu::BlendState blendState = wgpu::Default;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.color.srcFactor = wgpu::BlendFactor::One;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.alpha.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::One;
    blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;

    wgpu::ColorTargetState colorTarget = wgpu::Default;
    colorTarget.format = renderer.m_Format;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState = wgpu::Default;
    fragmentState.targets = &colorTarget;
    fragmentState.targetCount = 1;
    fragmentState.entryPoint = "text_frag";
    fragmentState.module = shaderModule;
    pipelineDescriptor.fragment = &fragmentState;

    wgpu::DepthStencilState depthStencilState = wgpu::Default;
    depthStencilState.format = renderer.m_DepthStencilFormat;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    depthStencilState.depthCompare = wgpu::CompareFunction::Always;

    pipelineDescriptor.depthStencil = &depthStencilState;

    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    pipelineDescriptor.primitive.cullMode = wgpu::CullMode::None;

    m_RenderPipeline = renderer.m_Device.createRenderPipeline(pipelineDescriptor);
}

bool FontAtlas::LoadFont(wgpu::Queue queue, const std::string& path, const Charset& charset, float fontSize)
{
    m_FontSize = fontSize;

    std::vector<uint8_t> fontBuffer = ReadFileBuffer(path);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, fontBuffer.data(), 0))
    {
        Log::Error("Failed to initialize font data");
        return false;
    }

    float scale = stbtt_ScaleForPixelHeight(&info, fontSize);

    using stb_data_ptr = std::unique_ptr<uint8_t, std::function<void(uint8_t*)>>;

    Charset copyCharset = charset;
    copyCharset.Insert(s_DefaultChar);
    const int charsetSize = copyCharset.size();

    std::vector<stbrp_rect> rects(charsetSize);
    std::vector<stb_data_ptr> sdfs(charsetSize);
    for (int i = 0; i < charsetSize; i++)
    {
        const char c = copyCharset.PopChar();

        int padding = 1;
        uint8_t onEdgeValue = 180;
        float pixelDistScale = 36.0f;

        stbrp_rect& rect = rects[i];
        rect.id = c;

        int offsetX = 0;
        int offsetY = 0;
        uint8_t* data = stbtt_GetCodepointSDF(&info, scale, (int)c, padding, onEdgeValue, pixelDistScale, &rect.w, &rect.h, &offsetX, &offsetY);

        Glyph glyph {
            .offset = { (float)offsetX, (float)offsetY },
            .size = { (float)rect.w, (float)rect.h }
        };
        m_GlyphMap.insert({ c, glyph });

        sdfs[i] = stb_data_ptr(data, [](uint8_t* data) { STBTT_free(data, nullptr); });
    }

    stbrp_context rectPackContext;
    std::vector<stbrp_node> nodes(m_Width);
    stbrp_init_target(&rectPackContext, m_Width, m_Height, nodes.data(), nodes.size());
    stbrp_pack_rects(&rectPackContext, rects.data(), rects.size());

    Math::float2 textureSize { (float)m_Width, (float)m_Height };

    copyCharset = charset;
    copyCharset.Insert(s_DefaultChar);

    std::vector<uint8_t> bitmap(m_Width * m_Height);
    for (int i = 0; i < charsetSize; i++)
    {
        const char c = copyCharset.PopChar();

        const stbrp_rect& rect = rects[i];
        const stb_data_ptr& sdf = sdfs[i];

        for (int x = 0; x < rect.w; x++)
        {
            for (int y = 0; y < rect.h; y++)
            {
                int bitmapIndex = (rect.x + x) + m_Width * (rect.y + y);
                int sdfIndex = x + rect.w * y;
                bitmap[bitmapIndex] = sdf.get()[sdfIndex];
            }
        }

        auto it = m_GlyphMap.find(c);
        assert(it != m_GlyphMap.end());
        Glyph& glyph = it->second;

        glyph.texPosition = Math::float2{ (float)rect.x, (float)rect.y } / textureSize;
        glyph.texSize = Math::float2{ (float)rect.w, (float)rect.h } / textureSize;

        int advanceWidth = 0;
        stbtt_GetCodepointHMetrics(&info, (int)c, &advanceWidth, nullptr);
        glyph.advanceWidth = (float)advanceWidth * scale;
    }

    queue.writeTexture(
        WGPUImageCopyTexture {
            .texture = m_Texture,
            .mipLevel = 0,
            .origin = wgpu::Origin3D{},
            .aspect = wgpu::TextureAspect::All
        },
        bitmap.data(),
        bitmap.size(),
        WGPUTextureDataLayout {
            .offset = 0,
            .bytesPerRow = (uint32_t)(m_Width * sizeof(uint8_t)),
            .rowsPerImage = (uint32_t)m_Height
        },
        WGPUExtent3D {
            .width = (uint32_t)m_Width,
            .height = (uint32_t)m_Height,
            .depthOrArrayLayers = 1
        }
    );

    return true;
}

float FontAtlas::MeasureTextHeight(const std::string &text)
{
    return GetTextHeight(text) / m_FontSize;
}

void FontAtlas::RenderText(wgpu::Queue queue, wgpu::RenderPassEncoder renderEncoder, const std::string& text, float aspect, float size, Math::float2 position) const
{
    if (text.empty()) return;

    renderEncoder.setPipeline(m_RenderPipeline);

    struct GlyphQuad
    {
        Math::float2 position;
        Math::float2 scale;
        Math::float2 texPosition;
        Math::float2 texScale;
    };

    std::vector<GlyphQuad> quads;
    quads.reserve(text.size());

    float scale = size / m_FontSize;

    position.x -= GetTextWidth(text) / 2 * scale;
    position.y -= GetTextHeight(text) / 2 * scale;

    float maxPosition = -INFINITY;
    for (int i = 0; i < (int)text.size(); i++)
    {
        const Glyph& glyph = GetGlyph(text[i]);
        maxPosition = std::max(glyph.offset.y + glyph.size.y, maxPosition);
    }
    position.y += maxPosition * scale;

    float currentPoint = 0.0f;
    for (int i = 0; i < (int)text.size(); i++)
    {
        const Glyph& glyph = GetGlyph(text[i]);

        if (glyph.texSize.x != 0.0f && glyph.texSize.y != 0.0f)
        {
            Math::float2 glyphScale = scale * glyph.size / 2.0f;
            Math::float2 glyphPosition = scale * glyph.offset + Math::float2{ currentPoint, 0.0f } + glyphScale;

            glyphPosition.y *= -aspect;
            glyphScale.y *= -aspect;

            quads.push_back({
                .position = glyphPosition + position,
                .scale = glyphScale,
                .texPosition = glyph.texPosition,
                .texScale = glyph.texSize
            });
        }

        currentPoint += scale * glyph.advanceWidth;
    }
    assert(quads.size() > 0);

    queue.writeBuffer(m_QuadBuffer.get(), 0, quads.data(), quads.size() * sizeof(GlyphQuad));

    // TODO: Dynamic offset to support multiple calls per frame
    renderEncoder.setBindGroup(m_QuadBindGroupIndex, m_QuadBindGroup, 0, nullptr);
    renderEncoder.draw(4, quads.size(), 0, 0);
}

float FontAtlas::GetTextWidth(const std::string& text) const
{
    if (text.empty()) return 0.0f;

    const Glyph& leftGlyph = GetGlyph(text[0]);
    if (text.size() == 1)
    {
        return leftGlyph.size.x;
    }

    const Glyph& rightGlyph = GetGlyph(text.back());

    float left = leftGlyph.offset.x;
    float right = leftGlyph.advanceWidth + rightGlyph.offset.x + rightGlyph.size.x;

    for (int i = 1; i < (int)text.size() - 1; i++)
    {
        const Glyph& glyph = GetGlyph(text[i]);
        right += glyph.advanceWidth;
    }

    return right - left;
}

float FontAtlas::GetTextHeight(const std::string& text) const
{
    float top = 0.0f;
    float bottom = 0.0f;
    for (int i = 0; i < (int)text.size(); i++)
    {
        const Glyph& glyph = GetGlyph(text[i]);
        top    = std::min(glyph.offset.y, top);
        bottom = std::max(glyph.offset.y + glyph.size.y, bottom);
    }
    return std::abs(top - bottom);
}

const FontAtlas::Glyph& FontAtlas::GetGlyph(char c) const
{
    auto it = m_GlyphMap.find(c);
    if (it == m_GlyphMap.end())
    {
        it = m_GlyphMap.find(s_DefaultChar);
        assert(it != m_GlyphMap.end());
    }
    return it->second;
}
