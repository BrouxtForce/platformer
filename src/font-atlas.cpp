#include "font-atlas.hpp"

#include <SDL3/SDL.h>
#include <cmath>

#include "memory-arena.hpp"
#include "application.hpp"

// Since ImGui uses STBTT and provides no way to pass in an arena, the arena will be nullptr when ImGui initializes its font atlas
// Defining STBTT_malloc to use an arena means that I will not have to bother with freeing each malloc'd pointer individually
// TODO: ImGui is not included in Release, so this ternary expression can be stripped out of such builds

#define STBTT_malloc(bytes, arena) (((MemoryArena*)(arena ? arena : &GlobalArena))->Alloc((bytes), alignof(std::max_align_t)))
#define STBTT_free(ptr, arena) ((void)(ptr), (void)(arena))

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#include <stb/stb_truetype.h>

#include "log.hpp"
#include "utility.hpp"
#include "renderer.hpp"

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

bool FontAtlas::Init(Renderer& renderer, int width, int height)
{
    m_Width = width;
    m_Height = height;

    wgpu::TextureDescriptor textureDescriptor = wgpu::Default;
    textureDescriptor.size.width = m_Width;
    textureDescriptor.size.height = m_Height;
    textureDescriptor.label = (StringView)"Font Atlas Texture";
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

    m_QuadBuffer = renderer.m_Device.createBuffer(WGPUBufferDescriptor {
        .nextInChain = nullptr,
        .label = StringView{},
        .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst,
        .size = s_QuadBufferSize,
        .mappedAtCreation = false
    });

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
            .buffer = m_QuadBuffer,
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

    Array<WGPUBindGroupLayout> packedBindGroupLayouts;
    packedBindGroupLayouts.arena = &TransientArena;
    packedBindGroupLayouts.Reserve(renderer.m_BindGroupLayouts.size() + 1);
    for (WGPUBindGroupLayout layout : renderer.m_BindGroupLayouts)
    {
        packedBindGroupLayouts.Push(layout);
    }
    packedBindGroupLayouts.Push(bindGroupLayout);

    wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor = wgpu::Default;
    pipelineLayoutDescriptor.bindGroupLayouts = packedBindGroupLayouts.data;
    pipelineLayoutDescriptor.bindGroupLayoutCount = packedBindGroupLayouts.size;
    m_QuadBindGroupIndex = packedBindGroupLayouts.size - 1;

    wgpu::PipelineLayout pipelineLayout = renderer.m_Device.createPipelineLayout(pipelineLayoutDescriptor);

    const Shader* textShader = renderer.m_ShaderLibrary.GetShader("text");
    if (!textShader)
    {
        return false;
    }

    wgpu::RenderPipelineDescriptor pipelineDescriptor = wgpu::Default;
    pipelineDescriptor.label = (StringView)"Font Atlas Render Pipeline";
    pipelineDescriptor.layout = pipelineLayout;

    pipelineDescriptor.vertex.module = textShader->shaderModule;
    pipelineDescriptor.vertex.entryPoint = (StringView)"text_vert";

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
    fragmentState.entryPoint = (StringView)"text_frag";
    fragmentState.module = textShader->shaderModule;
    pipelineDescriptor.fragment = &fragmentState;

    wgpu::DepthStencilState depthStencilState = wgpu::Default;
    depthStencilState.format = renderer.s_DepthStencilFormat;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_False;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    depthStencilState.depthCompare = wgpu::CompareFunction::Always;

    pipelineDescriptor.depthStencil = &depthStencilState;

    pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    pipelineDescriptor.primitive.cullMode = wgpu::CullMode::None;

    m_RenderPipeline = renderer.m_Device.createRenderPipeline(pipelineDescriptor);

    return true;
}

bool FontAtlas::LoadFont(wgpu::Queue queue, StringView path, const Charset& charset, float fontSize)
{
    m_FontSize = fontSize;

    Span<uint8_t> fontBuffer = ReadFileBuffer(path, &TransientArena);

    stbtt_fontinfo info;
    info.userdata = &TransientArena;
    if (!stbtt_InitFont(&info, fontBuffer.data, 0))
    {
        Log::Error("Failed to initialize font data");
        return false;
    }

    float scale = stbtt_ScaleForPixelHeight(&info, fontSize);

    Charset copyCharset = charset;
    copyCharset.Insert(s_DefaultChar);
    const int charsetSize = copyCharset.size();

    Array<stbrp_rect> rects;
    rects.arena = &TransientArena;
    rects.Resize(charsetSize);

    Array<uint8_t*> sdfs;
    sdfs.arena = &TransientArena;
    sdfs.Resize(charsetSize);

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

        sdfs[i] = data;
    }

    stbrp_context rectPackContext;

    Array<stbrp_node> nodes;
    nodes.arena = &TransientArena;
    nodes.Resize(m_Width);

    stbrp_init_target(&rectPackContext, m_Width, m_Height, nodes.data, nodes.size);
    stbrp_pack_rects(&rectPackContext, rects.data, rects.size);

    Math::float2 textureSize { (float)m_Width, (float)m_Height };

    copyCharset = charset;
    copyCharset.Insert(s_DefaultChar);

    Array<uint8_t> bitmap;
    bitmap.arena = &TransientArena;
    bitmap.Resize(m_Width * m_Height);
    for (int i = 0; i < charsetSize; i++)
    {
        const char c = copyCharset.PopChar();

        const stbrp_rect& rect = rects[i];
        const uint8_t* const sdf = sdfs[i];

        for (int x = 0; x < rect.w; x++)
        {
            for (int y = 0; y < rect.h; y++)
            {
                int bitmapIndex = (rect.x + x) + m_Width * (rect.y + y);
                int sdfIndex = x + rect.w * y;
                bitmap[bitmapIndex] = sdf[sdfIndex];
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

    WGPUTexelCopyTextureInfo destination {
        .texture = m_Texture,
        .mipLevel = 0,
        .origin = wgpu::Origin3D{},
        .aspect = wgpu::TextureAspect::All
    };

    WGPUTexelCopyBufferLayout layout {
        .offset = 0,
        .bytesPerRow = (uint32_t)(m_Width * sizeof(uint8_t)),
        .rowsPerImage = (uint32_t)m_Height,
    };

    WGPUExtent3D writeSize {
        .width = (uint32_t)m_Width,
        .height = (uint32_t)m_Height,
        .depthOrArrayLayers = 1
    };

    wgpuQueueWriteTexture(queue, &destination, bitmap.data, bitmap.size, &layout, &writeSize);

    return true;
}

float FontAtlas::MeasureTextHeight(StringView text)
{
    return GetTextHeight(text) / m_FontSize;
}

void FontAtlas::NewFrame()
{
    m_QuadsWritten = 0;
}

void FontAtlas::RenderText(wgpu::Queue queue, wgpu::RenderPassEncoder renderEncoder, StringView text, float aspect, float size, Math::float2 position)
{
    if (text.size == 0) return;

    struct GlyphQuad
    {
        Math::float2 position;
        Math::float2 scale;
        Math::float2 texPosition;
        Math::float2 texScale;
    };

    Array<GlyphQuad> quads;
    quads.arena = &TransientArena;
    quads.Reserve(text.size);

    float scale = size / m_FontSize;

    position.x -= GetTextWidth(text) / 2 * scale;
    position.y -= GetTextHeight(s_DefaultChar) / 2 * scale;

    auto a = GetGlyph(s_DefaultChar);
    position.y += (a.offset.y + a.size.y) * scale;

    float currentPoint = 0.0f;
    for (int i = 0; i < (int)text.size; i++)
    {
        const Glyph& glyph = GetGlyph(text[i]);

        if (glyph.texSize.x != 0.0f && glyph.texSize.y != 0.0f)
        {
            Math::float2 glyphScale = scale * glyph.size / 2.0f;
            Math::float2 glyphPosition = scale * glyph.offset + Math::float2{ currentPoint, 0.0f } + glyphScale;

            glyphPosition.y *= -aspect;
            glyphScale.y *= -aspect;

            quads.Push({
                .position = glyphPosition + position,
                .scale = glyphScale,
                .texPosition = glyph.texPosition,
                .texScale = glyph.texSize
            });
        }

        currentPoint += scale * glyph.advanceWidth;
    }
    assert(quads.size > 0);

    size_t writeSize = quads.size * sizeof(GlyphQuad);
    if (m_QuadsWritten * sizeof(GlyphQuad) + writeSize > s_QuadBufferSize)
    {
        // Not enough space in m_QuadBuffer
        constexpr size_t maxCharCount = s_QuadBufferSize / sizeof(GlyphQuad);
        Log::Error("Font atlas RenderText() character limit reached (Max % characters).", maxCharCount);
        return;
    }
    size_t offset = m_QuadsWritten * sizeof(GlyphQuad);
    queue.writeBuffer(m_QuadBuffer, offset, quads.data, writeSize);

    renderEncoder.setPipeline(m_RenderPipeline);
    renderEncoder.setBindGroup(m_QuadBindGroupIndex, m_QuadBindGroup, 0, nullptr);
    renderEncoder.draw(4, quads.size, 0, m_QuadsWritten);

    m_QuadsWritten += quads.size;
}

float FontAtlas::GetTextWidth(StringView text) const
{
    if (text.size == 0) return 0.0f;

    const Glyph& leftGlyph = GetGlyph(text[0]);
    if (text.size == 1)
    {
        return leftGlyph.size.x;
    }

    const Glyph& rightGlyph = GetGlyph(text[text.size - 1]);

    float left = leftGlyph.offset.x;
    float right = leftGlyph.advanceWidth + rightGlyph.offset.x + rightGlyph.size.x;

    for (int i = 1; i < (int)text.size - 1; i++)
    {
        const Glyph& glyph = GetGlyph(text[i]);
        right += glyph.advanceWidth;
    }

    return right - left;
}

float FontAtlas::GetTextHeight(StringView text) const
{
    float top = 0.0f;
    float bottom = 0.0f;
    for (int i = 0; i < (int)text.size; i++)
    {
        const Glyph& glyph = GetGlyph(text[i]);
        top    = Math::Min(glyph.offset.y, top);
        bottom = Math::Max(glyph.offset.y + glyph.size.y, bottom);
    }
    return Math::Abs(top - bottom);
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
