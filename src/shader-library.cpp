#include "shader-library.hpp"

#include <variant>
#include "utility.hpp"
#include "log.hpp"

namespace
{
    using DataType = Shader::DataType;

    static constexpr uint8_t s_DataTypeAlignment[(int)DataType::Count] {
        [(int)DataType::Int32]  = 4,
        [(int)DataType::Uint32] = 4,
        [(int)DataType::Float]  = 4,

        [(int)DataType::Int2] = 8,
        [(int)DataType::Int3] = 16,
        [(int)DataType::Int4] = 16,

        [(int)DataType::Uint2] = 8,
        [(int)DataType::Uint3] = 16,
        [(int)DataType::Uint4] = 16,

        [(int)DataType::Float2] = 8,
        [(int)DataType::Float3] = 16,
        [(int)DataType::Float4] = 16
    };

    static constexpr uint8_t s_DataTypeSize[(int)DataType::Count] {
        [(int)DataType::Int32]  = 4,
        [(int)DataType::Uint32] = 4,
        [(int)DataType::Float]  = 4,

        [(int)DataType::Int2] = 8,
        [(int)DataType::Int3] = 12,
        [(int)DataType::Int4] = 16,

        [(int)DataType::Uint2] = 8,
        [(int)DataType::Uint3] = 12,
        [(int)DataType::Uint4] = 16,

        [(int)DataType::Float2] = 8,
        [(int)DataType::Float3] = 12,
        [(int)DataType::Float4] = 16
    };

    std::string DataTypeToString(DataType dataType)
    {
        switch (dataType)
        {
            case DataType::Int32:  return "int32";
            case DataType::Uint32: return "uint32";
            case DataType::Float:  return "float";

            case DataType::Int2: return "int2";
            case DataType::Int3: return "int3";
            case DataType::Int4: return "int4";

            case DataType::Uint2: return "uint2";
            case DataType::Uint3: return "uint3";
            case DataType::Uint4: return "uint4";

            case DataType::Float2: return "float2";
            case DataType::Float3: return "float3";
            case DataType::Float4: return "float4";

            default: break;
        }
        Log::Error("Unknown data type: %", (int)dataType);
        return "unknown";
    }
}

using Token = std::variant<std::monostate, DataType, std::string_view, char>;

// TODO: Support WGSL comments
struct Shader::TokenInputStream
{
    CharacterInputStream input;
    Token peekToken;

    TokenInputStream(std::string_view source)
        : input(source) {}

    Token PeekToken()
    {
        if (std::holds_alternative<std::monostate>(peekToken))
        {
            peekToken = ReadToken();
        }
        return peekToken;
    }

    Token ReadToken()
    {
        if (!std::holds_alternative<std::monostate>(peekToken))
        {
            return std::exchange(peekToken, std::monostate{});
        }

        while (!input.Eof())
        {
            char c = input.Peek();
            if (std::isspace(c))
            {
                input.Next();
                continue;
            }
            if (c == '/')
            {
                ReadComment();
                continue;
            }
            break;
        }

        if (input.Eof())
        {
            return std::monostate{};
        }
        if (IsWord(input.Peek()))
        {
            return ReadWord();
        }
        return input.Next();
    }

    void ReadComment()
    {
        assert(input.Peek() == '/');
        input.Next();

        // Single line comment
        if (input.Peek() == '/')
        {
            input.Next();
            while (!input.Eof())
            {
                if (input.Next() == '\n')
                {
                    break;
                }
            }
            return;
        }

        // Multi-line comment
        // TODO: We don't support nested multi-line comments
        if (input.Peek() == '*')
        {
            input.Next();

            while (!input.Eof())
            {
                char c = input.Next();
                if (c == '*' && input.Peek() == '/')
                {
                    input.Next();
                    break;
                }
            }

            return;
        }

        // NOTE: At this point, we read a division sign. This is clearly bad, but it does not matter
        // for the purposes of our reflection system.
        return;
    }

    Token ReadWord()
    {
        size_t startPosition = input.position;

        assert(IsWord(input.Peek()) && "The first character must be part of the word");
        do
        {
            input.Next();
        }
        while (IsWord(input.Peek()));

        std::string_view word = input.input.substr(startPosition, input.position - startPosition);
        if (word == "i32")   return DataType::Int32;
        if (word == "u32")   return DataType::Uint32;
        if (word == "f32")   return DataType::Float;
        if (word == "vec2i") return DataType::Int2;
        if (word == "vec3i") return DataType::Int3;
        if (word == "vec4i") return DataType::Int4;
        if (word == "vec2u") return DataType::Uint2;
        if (word == "vec3u") return DataType::Uint3;
        if (word == "vec4u") return DataType::Uint4;
        if (word == "vec2f") return DataType::Float2;
        if (word == "vec3f") return DataType::Float3;
        if (word == "vec4f") return DataType::Float4;

        return word;
    }

    static bool IsWord(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '@') || (c == '_');
    }

    bool Eof() const
    {
        return input.Eof();
    }
};

Shader::Shader(wgpu::ShaderModule shaderModule, std::string_view source)
    : shaderModule(shaderModule)
{
    GenerateReflectionInfo(source);
}

void Shader::GenerateReflectionInfo(std::string_view source)
{
    TokenInputStream stream = source;

    while (!stream.Eof())
    {
        Token token = stream.PeekToken();

        if (std::holds_alternative<std::string_view>(token))
        {
            stream.ReadToken();
            if (std::get<std::string_view>(token) != "struct")
            {
                continue;
            }

            Token structNameToken = stream.ReadToken();
            if (!std::holds_alternative<std::string_view>(structNameToken))
            {
                continue;
            }

            // We only do reflection for Material structs
            if (std::get<std::string_view>(structNameToken) == "Material")
            {
                ParseMaterialStruct(stream);
                break;
            }
            continue;
        }

        stream.ReadToken();
    }
}

template<typename T>
T GetTokenMember(Token&& token, bool* fail)
{
    if (std::holds_alternative<T>(token))
    {
        return std::get<T>(token);
    }
    *fail = true;
    return {};
}

void Shader::ParseMaterialStruct(TokenInputStream& stream)
{
    Token openBracketToken = stream.ReadToken();
    if (!std::holds_alternative<char>(openBracketToken) || std::get<char>(openBracketToken) != '{')
    {
        Log::Error("Expected open bracket");
        return;
    }

    uint8_t currentDataOffset = 0;
    while (std::holds_alternative<std::string_view>(stream.PeekToken()))
    {
        bool fail = false;
        std::string_view memberName = GetTokenMember<std::string_view>(stream.ReadToken(), &fail);
        fail = fail || GetTokenMember<char>(stream.ReadToken(), &fail) != ':';
        DataType dataType = GetTokenMember<DataType>(stream.ReadToken(), &fail);
        // TODO: Last comma is optional
        fail = fail || GetTokenMember<char>(stream.ReadToken(), &fail) != ',';

        if (fail)
        {
            Log::Error("Failure when parsing material struct");
            break;
        }

        uint8_t memberSize = s_DataTypeSize[(int)dataType];
        uint8_t memberAlignment = s_DataTypeAlignment[(int)dataType];

        // TODO: Is there a more elegant way?
        currentDataOffset += (memberAlignment - currentDataOffset % memberAlignment) % memberAlignment;
        m_UniformMap.insert({
            (std::string)memberName,
            UniformData {
                .offset = currentDataOffset,
                .dataType = dataType
            }
        });
        currentDataOffset += memberSize;
    }
}

Shader::~Shader()
{
    if (shaderModule)
    {
        shaderModule.release();
    }
}

template<typename T>
constexpr DataType GetDataType()
{
    if constexpr      (std::is_same_v<T, int32_t>)      return DataType::Int32;
    else if constexpr (std::is_same_v<T, uint32_t>)     return DataType::Uint32;
    else if constexpr (std::is_same_v<T, float>)        return DataType::Float;
    else if constexpr (std::is_same_v<T, Math::int2>)   return DataType::Int2;
    else if constexpr (std::is_same_v<T, Math::int3>)   return DataType::Int3;
    else if constexpr (std::is_same_v<T, Math::int4>)   return DataType::Int4;
    else if constexpr (std::is_same_v<T, Math::uint2>)  return DataType::Uint2;
    else if constexpr (std::is_same_v<T, Math::uint3>)  return DataType::Uint3;
    else if constexpr (std::is_same_v<T, Math::uint4>)  return DataType::Uint4;
    else if constexpr (std::is_same_v<T, Math::float2>) return DataType::Float2;
    else if constexpr (std::is_same_v<T, Math::float3>) return DataType::Float3;
    else if constexpr (std::is_same_v<T, Math::float4>) return DataType::Float4;
    else static_assert(!sizeof(T), "Invalid data type");
}

template<typename T>
void Shader::WriteUniform(Material& material, const std::string& name, T value) const
{
    constexpr DataType inputDataType = GetDataType<T>();

    auto it = m_UniformMap.find(name);
    if (it == m_UniformMap.end())
    {
        Log::Error("Unknown uniform '%'", name);
        return;
    }
    const UniformData& uniformData = it->second;

    if (uniformData.dataType != inputDataType)
    {
        Log::Error("Non-matching data types. Expected %, but got %", DataTypeToString(uniformData.dataType), DataTypeToString(inputDataType));
        return;
    }

    assert(uniformData.offset >= 0 && uniformData.offset + sizeof(T) < Material::SIZE);
    assert(uniformData.offset % 4 == 0);
    std::memcpy(&material.data[uniformData.offset / 4], &value, sizeof(T));
}
template void Shader::WriteUniform<int32_t>(Material&, const std::string&, int32_t) const;
template void Shader::WriteUniform<uint32_t>(Material&, const std::string&, uint32_t) const;
template void Shader::WriteUniform<float>(Material&, const std::string&, float) const;
template void Shader::WriteUniform<Math::int2>(Material&, const std::string&, Math::int2) const;
template void Shader::WriteUniform<Math::int3>(Material&, const std::string&, Math::int3) const;
template void Shader::WriteUniform<Math::int4>(Material&, const std::string&, Math::int4) const;
template void Shader::WriteUniform<Math::uint2>(Material&, const std::string&, Math::uint2) const;
template void Shader::WriteUniform<Math::uint3>(Material&, const std::string&, Math::uint3) const;
template void Shader::WriteUniform<Math::uint4>(Material&, const std::string&, Math::uint4) const;
template void Shader::WriteUniform<Math::float2>(Material&, const std::string&, Math::float2) const;
template void Shader::WriteUniform<Math::float3>(Material&, const std::string&, Math::float3) const;
template void Shader::WriteUniform<Math::float4>(Material&, const std::string&, Math::float4) const;

template<typename T>
std::optional<T> Shader::GetUniform(const Material& material, const std::string& name) const
{
    constexpr DataType inputDataType = GetDataType<T>();

    auto it = m_UniformMap.find(name);
    if (it == m_UniformMap.end())
    {
        Log::Error("Could not find uniform '%'", name);
        return std::nullopt;
    }
    const UniformData& uniformData = it->second;

    if (uniformData.dataType != inputDataType)
    {
        Log::Error("Non-matching data types. Expected %, but got %", DataTypeToString(uniformData.dataType), DataTypeToString(inputDataType));
        return std::nullopt;
    }

    assert(uniformData.offset >= 0 && uniformData.offset + sizeof(T) < Material::SIZE);
    assert(uniformData.offset % 4 == 0);
    static_assert(s_DataTypeSize[(int)inputDataType] == sizeof(T));
    return *(T*)&material.data[uniformData.offset / 4];
}
template std::optional<int32_t> Shader::GetUniform<int32_t>(const Material&, const std::string&) const;
template std::optional<uint32_t> Shader::GetUniform<uint32_t>(const Material&, const std::string&) const;
template std::optional<float> Shader::GetUniform<float>(const Material&, const std::string&) const;
template std::optional<Math::int2> Shader::GetUniform<Math::int2>(const Material&, const std::string&) const;
template std::optional<Math::int3> Shader::GetUniform<Math::int3>(const Material&, const std::string&) const;
template std::optional<Math::int4> Shader::GetUniform<Math::int4>(const Material&, const std::string&) const;
template std::optional<Math::uint2> Shader::GetUniform<Math::uint2>(const Material&, const std::string&) const;
template std::optional<Math::uint3> Shader::GetUniform<Math::uint3>(const Material&, const std::string&) const;
template std::optional<Math::uint4> Shader::GetUniform<Math::uint4>(const Material&, const std::string&) const;
template std::optional<Math::float2> Shader::GetUniform<Math::float2>(const Material&, const std::string&) const;
template std::optional<Math::float3> Shader::GetUniform<Math::float3>(const Material&, const std::string&) const;
template std::optional<Math::float4> Shader::GetUniform<Math::float4>(const Material&, const std::string&) const;

void ShaderLibrary::Load(wgpu::Device device)
{
    constexpr std::string_view dirname = "shaders/";
    m_Header = ReadFile((std::string)dirname + (std::string)m_HeaderFilename);

    for (const std::string& filename : GetFilesInDirectory((std::string)dirname))
    {
        constexpr std::string_view extension = ".wgsl";
        if (!filename.ends_with(extension) || filename == m_HeaderFilename)
        {
            continue;
        }
        std::string moduleName = filename.substr(0, filename.size() - extension.size());
        m_ShaderModuleMap.insert({ moduleName, LoadShader(device, (std::string)dirname + filename) });
        Log::Debug("Loaded shader module '%'", moduleName);
    }
}

const Shader* ShaderLibrary::GetShader(const std::string& name) const
{
    auto it = m_ShaderModuleMap.find(name);
    if (it == m_ShaderModuleMap.end())
    {
        return nullptr;
    }
    return it->second.get();
}

std::unique_ptr<Shader> ShaderLibrary::LoadShader(wgpu::Device device, const std::string& filepath)
{
    const std::string shaderSource = Preprocess(ReadFile(filepath));

    wgpu::ShaderModuleWGSLDescriptor wgslDescriptor = wgpu::Default;
    wgslDescriptor.chain.next = nullptr;
    wgslDescriptor.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgslDescriptor.code = shaderSource.c_str();

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor = wgpu::Default;
    shaderModuleDescriptor.label = filepath.c_str();
#if WEBGPU_BACKEND_WGPU
    shaderModuleDescriptor.hintCount = 0;
    shaderModuleDescriptor.hints = nullptr;
#endif
    shaderModuleDescriptor.nextInChain = &wgslDescriptor.chain;

    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderModuleDescriptor);
    return std::make_unique<Shader>(shaderModule, shaderSource);
}

std::string ShaderLibrary::Preprocess(const std::string& source)
{
    return source + m_Header;
}
