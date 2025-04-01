#include "shader-library.hpp"

#include <variant>
#include "utility.hpp"
#include "log.hpp"
#include "application.hpp"

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
}

using Token = std::variant<std::monostate, DataType, StringView, char>;

// TODO: Support WGSL comments
struct Shader::TokenInputStream
{
    CharacterInputStream input;
    Token peekToken;

    TokenInputStream(StringView source)
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

        StringView word = input.input.Substr(startPosition, input.position - startPosition);
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

Shader::Shader(wgpu::ShaderModule shaderModule, StringView name, StringView source)
    : name(String::Copy(name, &GlobalArena)), shaderModule(shaderModule)
{
    GenerateReflectionInfo(source);
}

void Shader::GenerateReflectionInfo(StringView source)
{
    TokenInputStream stream = source;

    while (!stream.Eof())
    {
        Token token = stream.PeekToken();

        if (std::holds_alternative<StringView>(token))
        {
            stream.ReadToken();
            if (std::get<StringView>(token) != "struct")
            {
                continue;
            }

            Token structNameToken = stream.ReadToken();
            if (!std::holds_alternative<StringView>(structNameToken))
            {
                continue;
            }

            // We only do reflection for Material structs
            if (std::get<StringView>(structNameToken) == "Material")
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
    while (std::holds_alternative<StringView>(stream.PeekToken()))
    {
        bool fail = false;
        StringView memberName = GetTokenMember<StringView>(stream.ReadToken(), &fail);
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
            String::Copy(memberName, &GlobalArena),
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

void ShaderLibrary::Load(wgpu::Device device)
{
    constexpr StringView dirname = "shaders/";

    String headerPath;
    headerPath.arena = &TransientArena;
    headerPath << dirname << m_HeaderFilename << '\0';

    m_Header = ReadFile(headerPath.data, &GlobalArena);

    Array<String> files = GetFilesInDirectory(dirname, &TransientArena);
    for (StringView filename : files)
    {
        constexpr StringView extension = ".wgsl";
        if (!filename.EndsWith(extension) || filename == m_HeaderFilename)
        {
            continue;
        }
        StringView moduleName = filename.Substr(0, filename.size - extension.size);

        String filepath;
        filepath.arena = &TransientArena;
        filepath << dirname << filename;

        m_ShaderModuleMap.insert({ String::Copy(moduleName, &GlobalArena), LoadShader(device, filepath, moduleName) });
        Log::Debug("Loaded shader module '%'", moduleName);
    }
}

const Shader* ShaderLibrary::GetShader(StringView name) const
{
    auto it = m_ShaderModuleMap.find(name);
    if (it == m_ShaderModuleMap.end())
    {
        Log::Error("Could not find shader '%'", name);
        return nullptr;
    }
    return it->second.get();
}

std::unique_ptr<Shader> ShaderLibrary::LoadShader(wgpu::Device device, StringView filepath, StringView shaderName)
{
    String shaderSource = Preprocess(ReadFile(filepath, &TransientArena), &TransientArena);
    shaderSource.NullTerminate();

    WGPUShaderSourceWGSL wgslDescriptor {
        .chain = {
            .next = nullptr,
            .sType = WGPUSType_ShaderSourceWGSL
        },
        // TODO: No null termination
        .code = (StringView)shaderSource
    };

    /*
    wgpu::ShaderModuleWGSLDescriptor wgslDescriptor = wgpu::Default;
    wgslDescriptor.chain.next = nullptr;
    wgslDescriptor.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgslDescriptor.code = shaderSource.data;
    */

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor = wgpu::Default;
    // TODO: No null termination
    shaderModuleDescriptor.label = filepath;
    shaderModuleDescriptor.nextInChain = &wgslDescriptor.chain;

    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderModuleDescriptor);
    return std::make_unique<Shader>(shaderModule, shaderName, shaderSource);
}

String ShaderLibrary::Preprocess(StringView source, MemoryArena* arena)
{
    String out;
    out.arena = arena;
    out << source << m_Header;
    return out;
}
