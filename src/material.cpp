#include "material.hpp"

#include <unordered_map>

#include "config.hpp"
#include "utility.hpp"
#include "application.hpp"
#include "webgpu/webgpu.hpp"

namespace
{
    StringView DataTypeToString(Shader::DataType dataType)
    {
        switch (dataType)
        {
            case Shader::DataType::Int32:  return "int32";
            case Shader::DataType::Uint32: return "uint32";
            case Shader::DataType::Float:  return "float";

            case Shader::DataType::Int2: return "int2";
            case Shader::DataType::Int3: return "int3";
            case Shader::DataType::Int4: return "int4";

            case Shader::DataType::Uint2: return "uint2";
            case Shader::DataType::Uint3: return "uint3";
            case Shader::DataType::Uint4: return "uint4";

            case Shader::DataType::Float2: return "float2";
            case Shader::DataType::Float3: return "float3";
            case Shader::DataType::Float4: return "float4";

            default: break;
        }
        Log::Error("Unknown data type: %", (int)dataType);
        return "unknown";
    }
}
template<typename T>
constexpr Shader::DataType GetDataType()
{
    if constexpr      (std::is_same_v<T, int32_t>)      return Shader::DataType::Int32;
    else if constexpr (std::is_same_v<T, uint32_t>)     return Shader::DataType::Uint32;
    else if constexpr (std::is_same_v<T, float>)        return Shader::DataType::Float;
    else if constexpr (std::is_same_v<T, Math::int2>)   return Shader::DataType::Int2;
    else if constexpr (std::is_same_v<T, Math::int3>)   return Shader::DataType::Int3;
    else if constexpr (std::is_same_v<T, Math::int4>)   return Shader::DataType::Int4;
    else if constexpr (std::is_same_v<T, Math::uint2>)  return Shader::DataType::Uint2;
    else if constexpr (std::is_same_v<T, Math::uint3>)  return Shader::DataType::Uint3;
    else if constexpr (std::is_same_v<T, Math::uint4>)  return Shader::DataType::Uint4;
    else if constexpr (std::is_same_v<T, Math::float2>) return Shader::DataType::Float2;
    else if constexpr (std::is_same_v<T, Math::float3>) return Shader::DataType::Float3;
    else if constexpr (std::is_same_v<T, Math::float4>) return Shader::DataType::Float4;
    else static_assert(!sizeof(T), "Invalid data type");
}

template<typename T>
void Material::SetUniform(StringView name, T value)
{
    constexpr Shader::DataType inputDataType = GetDataType<T>();

    auto it = shader->m_UniformMap.find(name);
    if (it == shader->m_UniformMap.end())
    {
        Log::Error("Unknown uniform '%'", name);
        return;
    }
    const Shader::UniformData& uniformData = it->second;

    if (uniformData.dataType != inputDataType)
    {
        Log::Error("Non-matching data types. Expected %, but got %", DataTypeToString(uniformData.dataType), DataTypeToString(inputDataType));
        return;
    }

    assert(uniformData.offset >= 0 && uniformData.offset + sizeof(T) <= data.size);
    assert(uniformData.offset % 4 == 0);
    std::memcpy(&data[uniformData.offset / 4], &value, sizeof(T));

    updated = true;
}
template void Material::SetUniform<int32_t>(StringView, int32_t);
template void Material::SetUniform<uint32_t>(StringView, uint32_t);
template void Material::SetUniform<float>(StringView, float);
template void Material::SetUniform<Math::int2>(StringView, Math::int2);
template void Material::SetUniform<Math::int3>(StringView, Math::int3);
template void Material::SetUniform<Math::int4>(StringView, Math::int4);
template void Material::SetUniform<Math::uint2>(StringView, Math::uint2);
template void Material::SetUniform<Math::uint3>(StringView, Math::uint3);
template void Material::SetUniform<Math::uint4>(StringView, Math::uint4);
template void Material::SetUniform<Math::float2>(StringView, Math::float2);
template void Material::SetUniform<Math::float3>(StringView, Math::float3);
template void Material::SetUniform<Math::float4>(StringView, Math::float4);

template<typename T>
std::optional<T> Material::GetUniform(StringView name) const
{
    constexpr Shader::DataType inputDataType = GetDataType<T>();

    auto it = shader->m_UniformMap.find(name);
    if (it == shader->m_UniformMap.end())
    {
        Log::Error("Could not find uniform '%'", name);
        return std::nullopt;
    }
    const Shader::UniformData& uniformData = it->second;

    if (uniformData.dataType != inputDataType)
    {
        Log::Error("Non-matching data types. Expected %, but got %", DataTypeToString(uniformData.dataType), DataTypeToString(inputDataType));
        return std::nullopt;
    }

    assert(uniformData.offset >= 0 && uniformData.offset + sizeof(T) <= data.size);
    assert(uniformData.offset % 4 == 0);
    static_assert(Shader::s_DataTypeSize[(int)inputDataType] == sizeof(T));
    return *(T*)&data[uniformData.offset / 4];
}
template std::optional<int32_t> Material::GetUniform<int32_t>(StringView) const;
template std::optional<uint32_t> Material::GetUniform<uint32_t>(StringView) const;
template std::optional<float> Material::GetUniform<float>(StringView) const;
template std::optional<Math::int2> Material::GetUniform<Math::int2>(StringView) const;
template std::optional<Math::int3> Material::GetUniform<Math::int3>(StringView) const;
template std::optional<Math::int4> Material::GetUniform<Math::int4>(StringView) const;
template std::optional<Math::uint2> Material::GetUniform<Math::uint2>(StringView) const;
template std::optional<Math::uint3> Material::GetUniform<Math::uint3>(StringView) const;
template std::optional<Math::uint4> Material::GetUniform<Math::uint4>(StringView) const;
template std::optional<Math::float2> Material::GetUniform<Math::float2>(StringView) const;
template std::optional<Math::float3> Material::GetUniform<Math::float3>(StringView) const;
template std::optional<Math::float4> Material::GetUniform<Math::float4>(StringView) const;

void Material::Flush(wgpu::Queue queue)
{
    queue.writeBuffer(buffer, 0, data.data, data.size);
    Log::Debug("Material '%' flushed to GPU", name);
}

// TODO: Use an arena-friendly hash map
static std::unordered_map<StringView, Material*> s_MaterialMap;
static Material* s_DefaultMaterial = nullptr;

void MaterialManager::Init(const ShaderLibrary& shaderLibrary, wgpu::Device device)
{
    Config::SetMemoryArena(&TransientArena);
    Config::Load("assets/materials.toml");

    WGPUBindGroupLayoutEntry bindGroupLayoutEntry {
        .nextInChain = nullptr,
        .binding = 0,
        // TODO: Use shader reflection to conditionally reduce visibility?
        .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
        .buffer = {
            .nextInChain = nullptr,
            .type = wgpu::BufferBindingType::Uniform,
            .hasDynamicOffset = false,
            // TODO: Does this have to be zero?
            .minBindingSize = 0
        },
        .sampler = wgpu::SamplerBindingLayout{},
        .texture = wgpu::TextureBindingLayout{},
        .storageTexture = wgpu::StorageTextureBindingLayout{}
    };
    wgpu::BindGroupLayout bindGroupLayout = device.createBindGroupLayout(WGPUBindGroupLayoutDescriptor {
        .nextInChain = nullptr,
        .label = (StringView)"Material Bind Group Layout",
        .entryCount = 1,
        .entries = &bindGroupLayoutEntry
    });

    Array<StringView> tables = Config::GetTables(&TransientArena);
    tables.Push("default");
    for (StringView table : tables)
    {
        Config::PushTable(table);

        if (table == "default")
        {
            Config::SuppressWarnings(true);
        }

        StringView shaderName = Config::Get<StringView>("shader", "quad");

        const Shader* shader = shaderLibrary.GetShader(shaderName);

        size_t materialSize = 0;
        for (const auto& [_, uniformData] : shader->m_UniformMap)
        {
            size_t offset = uniformData.offset;
            size_t size = Shader::s_DataTypeSize[(size_t)uniformData.dataType];
            materialSize = Math::Max(materialSize, offset + size);
        }
        // TODO: Verify that this is correct
        materialSize = Align(materialSize, 16);

        Material* material = GlobalArena.Alloc<Material>();
        material->shader = shader;
        material->name = String::Copy(table, &GlobalArena);

        material->data.data = GlobalArena.Alloc<uint8_t>(materialSize);
        material->data.size = materialSize;

        // Write material properties from config
        for (const auto& [name, uniformData] : shader->m_UniformMap)
        {
            // TOOD: Remove the need for bitcasts
            switch (uniformData.dataType)
            {
                case Shader::DataType::Int32:
                    material->SetUniform<int32_t>(name, Config::Get<int32_t>(name, 0));
                    break;
                case Shader::DataType::Uint32:
                    material->SetUniform<uint32_t>(name, Config::Get<int32_t>(name, 0));
                    break;
                case Shader::DataType::Float:
                    material->SetUniform<float>(name, Config::Get<float>(name, 0.0f));
                    break;
                case Shader::DataType::Int2:
                    material->SetUniform<Math::uint2>(name, std::bit_cast<Math::uint2>(Config::Get<Math::int2>(name, {})));
                    break;
                case Shader::DataType::Uint2:
                    material->SetUniform<Math::int2>(name, Config::Get<Math::int2>(name, {}));
                    break;
                case Shader::DataType::Int3:
                    material->SetUniform<Math::int3>(name, Config::Get<Math::int3>(name, {}));
                    break;
                case Shader::DataType::Uint3:
                    material->SetUniform<Math::uint3>(name, std::bit_cast<Math::uint3>(Config::Get<Math::int3>(name, {})));
                    break;
                case Shader::DataType::Int4:
                    material->SetUniform<Math::int4>(name, Config::Get<Math::int4>(name, {}));
                    break;
                case Shader::DataType::Uint4:
                    material->SetUniform<Math::uint4>(name, std::bit_cast<Math::uint4>(Config::Get<Math::int4>(name, {})));
                    break;
                case Shader::DataType::Float2:
                    material->SetUniform<Math::float2>(name, Config::Get<Math::float2>(name, 0.0f));
                    break;
                case Shader::DataType::Float3:
                    material->SetUniform<Math::float3>(name, Config::Get<Math::float3>(name, 0.0f));
                    break;
                case Shader::DataType::Float4:
                    material->SetUniform<Math::float4>(name, Config::Get<Math::float4>(name, {}));
                    break;
                default:
                    Log::Error("Invalid data type: %", (int)uniformData.dataType);
            }
        }

        String label = Log::Format(&TransientArena, "% Material Buffer", shaderName);
        label.NullTerminate();
        material->buffer = device.createBuffer(WGPUBufferDescriptor {
            .nextInChain = nullptr,
            // TODO: No null termination
            .label = (StringView)label,
            .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,// | wgpu::BufferUsage::MapWrite,
            .size = materialSize,
            .mappedAtCreation = false
        });
        // memcpy(material->buffer.getMappedRange(0, materialSize), material->data.data, materialSize);
        // material->buffer.unmap();

        WGPUBindGroupEntry bindGroupEntry {
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = material->buffer,
            .offset = 0,
            .size = materialSize,
            .sampler = nullptr,
            .textureView = nullptr
        };

        String bindGroupLabel = Log::Format(&TransientArena, "% Bind Group", shaderName);
        bindGroupLabel.NullTerminate();
        material->bindGroup = device.createBindGroup(WGPUBindGroupDescriptor {
            .nextInChain = nullptr,
            // TODO: no null termination
            .label = (StringView)bindGroupLabel,
            .layout = bindGroupLayout,
            .entryCount = 1,
            .entries = &bindGroupEntry
        });

        if (table == "default")
        {
            Config::SuppressWarnings(false);
            s_DefaultMaterial = material;
        }
        else
        {
            s_MaterialMap.insert({ material->name, material });
            Log::Debug("Created material '%'", material->name);
        }

        Config::PopTable();
    }

    assert(s_DefaultMaterial != nullptr);

    Config::PopTable();
}

Material* MaterialManager::GetMaterial(StringView name)
{
    assert(s_DefaultMaterial != nullptr && "MaterialManager::Init() must be called before MaterialManager::GetMaterial()");

    auto it = s_MaterialMap.find(name);
    if (it == s_MaterialMap.end())
    {
        Log::Error("Could not find material '%'", name);
        return s_DefaultMaterial;
    }
    return it->second;
}

Material* MaterialManager::GetDefaultMaterial()
{
    return s_DefaultMaterial;
}

Array<String> MaterialManager::GetMaterialNames(MemoryArena* arena)
{
    Array<String> names;
    names.arena = arena;
    names.Reserve(s_MaterialMap.size());

    for (const auto& [name, _] : s_MaterialMap)
    {
        names.Push(String::Copy(name, arena));
    }

    return names;
}
