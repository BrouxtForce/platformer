#include "config.hpp"
#include "application.hpp"
#include "log.hpp"
#include "utility.hpp"

#include <type_traits>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <stack>

namespace Config
{
    static MemoryArena* s_MemoryArena = nullptr;
    static bool s_SuppressWarnings = false;

    void SetMemoryArena(MemoryArena* arena)
    {
        s_MemoryArena = arena;
    }

    namespace Parser
    {
        enum class Error
        {
            NoClosingQuotations,
            ExtraDecimalPoint,
            InvalidNumber,
            InvalidPunctuation
        };

        enum class Punctuation
        {
            OpenBracket, ClosedBracket,
            Comma,
            Equals
        };

        struct VariableName : StringView {};

        template<class> inline constexpr bool always_false_v = false;
        struct Token
        {
            using ValueType = std::variant<StringView, VariableName, float, int32_t, Punctuation, Error>;
            ValueType value{};

            static String ToString(ValueType value)
            {
                return std::visit([&](const auto& arg) {
                    String out;
                    out.arena = s_MemoryArena;

                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, StringView>)
                    {
                        out << '"' << arg << '"';
                        return out;
                    }
                    else if constexpr (std::is_same_v<T, VariableName> || std::is_same_v<T, float> || std::is_same_v<T, int32_t>)
                    {
                        out << arg;
                        return out;
                    }
                    else if constexpr (std::is_same_v<T, Punctuation>)
                    {
                        switch (arg)
                        {
                            case Punctuation::OpenBracket:   out << '['; break;
                            case Punctuation::ClosedBracket: out << ']'; break;
                            case Punctuation::Comma:         out << ','; break;
                            case Punctuation::Equals:        out << '='; break;
                        }
                        return out;
                    }
                    else if constexpr (std::is_same_v<T, Error>)
                    {
                        out << "[ERROR TOKEN]";
                        return out;
                    }
                    else
                    {
                        static_assert(always_false_v<T>);
                    }
                }, value);
            }
        };

        class TokenStream
        {
        public:
            TokenStream(StringView input)
                : m_CharStream(input) {}

            TokenStream(const TokenStream&) = delete;
            TokenStream& operator=(const TokenStream&) = delete;

            std::optional<Token> ReadToken()
            {
                if (m_PeekToken != std::nullopt)
                {
                    return std::exchange(m_PeekToken, std::nullopt);
                }
                if (m_CharStream.Eof()) return std::nullopt;

                char peek = m_CharStream.Peek();
                if (std::isdigit(peek) || peek == '-')
                {
                    return ReadNumber();
                }
                if (peek == '"')
                {
                    return ReadString();
                }
                if (peek == '#')
                {
                    ReadComment();
                    return ReadToken();
                }
                if (IsPunctuation(peek))
                {
                    return ReadPunctuation();
                }
                if (IsVariable(peek))
                {
                    return ReadVariable();
                }
                if (std::isspace(peek))
                {
                    ReadWhitespace();
                    return ReadToken();
                }
                Log::Error("Invalid character: '%'", peek);
                return std::nullopt;
            }

            std::optional<Token> PeekToken()
            {
                if (m_PeekToken == std::nullopt)
                {
                    m_PeekToken = ReadToken();
                }
                return m_PeekToken;
            }

        private:
            CharacterInputStream m_CharStream;
            std::optional<Token> m_PeekToken;

            static bool IsPunctuation(char c)
            {
                return std::string_view("[],=").find(c) != std::string::npos;
            }

            Token ReadPunctuation()
            {
                Token token {
                    .value = Error::InvalidPunctuation
                };

                assert(!m_CharStream.Eof());
                char c = m_CharStream.Next();
                switch (c)
                {
                    case '[':
                        token.value = Punctuation::OpenBracket;
                        break;
                    case ']':
                        token.value = Punctuation::ClosedBracket;
                        break;
                    case ',':
                        token.value = Punctuation::Comma;
                        break;
                    case '=':
                        token.value = Punctuation::Equals;
                        break;
                    default:
                        assert(false);
                        break;
                }
                return token;
            }

            static bool IsVariable(char c)
            {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c == '_') || (c == '.') || (c == '-');
            }

            Token ReadVariable()
            {
                Token token;

                assert(!m_CharStream.Eof());

                size_t tokenStart = m_CharStream.position;
                while (!m_CharStream.Eof())
                {
                    char c = m_CharStream.Peek();
                    if (IsVariable(c) || std::isdigit(c))
                    {
                        m_CharStream.Next();
                        continue;
                    }
                    break;
                }
                StringView name = m_CharStream.input.Substr(tokenStart, m_CharStream.position - tokenStart);

                token.value = VariableName { name };

                return token;
            }

            Token ReadNumber()
            {
                Token token;

                assert(!m_CharStream.Eof());

                size_t tokenStart = m_CharStream.position;
                if (m_CharStream.Peek() == '-')
                {
                    m_CharStream.Next();
                }

                bool readDecimalPoint = false;
                bool readExponent = false;
                while (!m_CharStream.Eof())
                {
                    char c = m_CharStream.Peek();
                    if (c >= '0' && c <= '9')
                    {
                        m_CharStream.Next();
                        continue;
                    }
                    if (c == '.')
                    {
                        m_CharStream.Next();
                        if (readDecimalPoint)
                        {
                            token.value = Error::ExtraDecimalPoint;
                            return token;
                        }
                        readDecimalPoint = true;
                        continue;
                    }
                    if (c == 'e' || c == 'E')
                    {
                        m_CharStream.Next();
                        if (readExponent)
                        {
                            token.value = Error::InvalidNumber;
                            return token;
                        }
                        readExponent = true;
                        readDecimalPoint = true;
                        if (m_CharStream.Peek() == '-')
                        {
                            m_CharStream.Next();
                        }
                        continue;
                    }
                    if (std::isspace(c) || IsPunctuation(c))
                    {
                        break;
                    }
                    token.value = Error::InvalidNumber;
                    return token;
                }
                StringView str = m_CharStream.input.Substr(tokenStart, m_CharStream.position - tokenStart);
                if (readDecimalPoint)
                {
                    token.value = str.ToFloat();
                }
                else
                {
                    token.value = str.ToInt();
                }
                return token;
            }

            Token ReadString()
            {
                Token token;

                assert(!m_CharStream.Eof());
                if (m_CharStream.Next() != '"') assert(false);

                size_t tokenStart = m_CharStream.position;
                while (!m_CharStream.Eof())
                {
                    char c = m_CharStream.Next();
                    if (c == '"')
                    {
                        StringView str = m_CharStream.input.Substr(tokenStart, m_CharStream.position - tokenStart - 1);
                        token.value = str;
                        return token;
                    }
                    if (c == '\n')
                    {
                        break;
                    }
                }
                token.value = Error::NoClosingQuotations;
                return token;
            }

            void ReadComment()
            {
                assert(!m_CharStream.Eof());
                if (m_CharStream.Next() != '#') assert(false);

                while (!m_CharStream.Eof())
                {
                    char c = m_CharStream.Next();
                    if (c == '\n')
                    {
                        break;
                    }
                }
            }

            void ReadWhitespace()
            {
                assert(!m_CharStream.Eof());
                if (!std::isspace(m_CharStream.Next())) assert(false);

                while (!m_CharStream.Eof())
                {
                    char c = m_CharStream.Peek();
                    if (!std::isspace(c))
                    {
                        break;
                    }
                    m_CharStream.Next();
                }
            }
        };

        class Table
        {
        public:
            Table() = default;
            Table(StringView filepath, StringView source)
                : filepath(filepath), source(source) {}

            Table(const Table&) = delete;
            Table& operator=(const Table&) = delete;

            StringView filepath;
            StringView source;

        private:
            template<typename T>
            T* Get(StringView key)
            {
                auto it = m_ValueMap.find(key);
                if (it == m_ValueMap.end())
                {
                    return nullptr;
                }
                if (std::holds_alternative<T>(it->second))
                {
                    return &std::get<T>(it->second);
                }
                return nullptr;
            }

            template<typename T, size_t Count>
            bool Get(StringView key, std::array<T, Count>& array)
            {
                Array<T>* elems = Get<Array<T>>(key);
                if (elems == nullptr)
                {
                    return false;
                }
                if (elems->size != array.size())
                {
                    return false;
                }
                std::copy(elems->begin(), elems->end(), array.begin());
                return true;
            }

            template<typename T>
            T& Set(StringView key, T value)
            {
                return std::get<T>(m_ValueMap[key] = value);
            }

            [[nodiscard]]
            bool Has(StringView key)
            {
                return m_ValueMap.find(key) != m_ValueMap.end();
            }

            using ValueType = std::variant<
                bool,
                float, Array<float>,
                int, Array<int>,
                StringView, Array<StringView>
            >;

            std::unordered_map<StringView, ValueType> m_ValueMap;

            friend class TableView;
        };

        class TableView
        {
        public:
            TableView()
                : m_Table(std::make_shared<Table>()) {}
            TableView(std::shared_ptr<Table> table)
                : m_Table(table) {}

            [[nodiscard]]
            Array<StringView> GetSubTableNames(MemoryArena* arena)
            {
                std::unordered_set<StringView> tableNamesSet;

                Array<StringView> out;
                out.arena = arena;

                for (const auto& [name, _] : m_Table->m_ValueMap)
                {
                    if (!name.StartsWith(m_Prefix))
                    {
                        continue;
                    }

                    size_t startIndex = m_Prefix.size;
                    size_t endIndex = name.Find('.', startIndex);
                    if (endIndex == String::NPOS)
                    {
                        continue;
                    }

                    StringView key = name.Substr(startIndex, endIndex - startIndex);
                    if (tableNamesSet.contains(key))
                    {
                        continue;
                    }
                    tableNamesSet.insert(key);
                    out.Push(name.Substr(startIndex, endIndex - startIndex));
                }

                return out;
            }

            [[nodiscard]]
            Array<StringView> GetKeys(MemoryArena* arena)
            {
                Array<StringView> out;
                out.arena = arena;

                for (const auto& [name, _] : m_Table->m_ValueMap)
                {
                    if (!name.StartsWith(m_Prefix))
                    {
                        continue;
                    }

                    size_t startIndex = m_Prefix.size;
                    size_t endIndex = name.Find('.', startIndex);
                    if (endIndex == String::NPOS)
                    {
                        out.Push(name.Substr(startIndex));
                        continue;
                    }
                    out.Push(name.Substr(startIndex, endIndex - startIndex));
                }

                return out;
            }

            [[nodiscard]]
            TableView GetSubTable(StringView name)
            {
                String newPrefix;
                newPrefix.arena = s_MemoryArena;
                newPrefix << m_Prefix << name << '.';

                return TableView(m_Table, newPrefix);
            }

            [[nodiscard]]
            bool Has(StringView key)
            {
                return m_Table->Has(GetKey(key));
            }

            template<typename T>
            [[nodiscard]]
            T* Get(StringView key)
            {
                return m_Table->Get<T>(GetKey(key));
            }

            template<typename T, size_t Count>
            [[nodiscard]]
            bool Get(StringView key, std::array<T, Count>& array)
            {
                return m_Table->Get<T, Count>(GetKey(key), array);
            }

            template<typename T>
            T& Set(StringView key, T value)
            {
                return m_Table->Set<T>(GetKey(key), value);
            }

            [[nodiscard]]
            Table* get()
            {
                return m_Table.get();
            }

            Table* operator->()
            {
                return get();
            }

        private:
            // TODO: Remove the need for a shared_ptr (Table is not trivially destructible)
            std::shared_ptr<Table> m_Table;
            StringView m_Prefix;

            TableView(std::shared_ptr<Table> table, StringView prefix)
                : m_Table(table), m_Prefix(prefix) {}

            StringView GetKey(StringView key)
            {
                String out;
                out.arena = s_MemoryArena;
                out << m_Prefix << key;
                return out;
            }
        };

        void LogNotHomogenous(StringView arrayName)
        {
            Log::Error("Array '%' is not homogenous", arrayName);
        }

        void AssertMatchingTokens(std::optional<Token> a, Token::ValueType b)
        {
            assert(a != std::nullopt && a->value == b);
        }

        [[nodiscard]]
        bool ExpectMatchingTokens(std::optional<Token> a, Token::ValueType b)
        {
            bool result = a != std::nullopt && a->value == b;
            if (!result)
            {
                Log::Error("Expected '%' but got '%'", Token::ToString(b), a.has_value() ? (StringView)Token::ToString(a->value) : "[null token]");
            }
            return result;
        }

        [[nodiscard]]
        bool ExpectMatchingTokens(std::optional<Token> a, std::initializer_list<Token::ValueType> values)
        {
            if (a != std::nullopt)
            {
                for (Token::ValueType value : values)
                {
                    if (a->value == value)
                    {
                        return true;
                    }
                }
            }
            String error;
            error.arena = &TransientArena;
            error << "Unexpected token '";
            error << ((a == std::nullopt) ? (StringView)"<EOF>" : Token::ToString(a->value));
            error << "'. Expected any of ";
            bool first = true;
            for (const Token::ValueType& value : values)
            {
                if (!first) error << ", ";
                first = false;
                error << '\'' << Token::ToString(value) << '\'';
            }
            error << '.';
            Log::Error(error);
            return false;
        }

        [[nodiscard]]
        bool ParseArray(TableView& table, StringView key, TokenStream& tokenStream)
        {
            AssertMatchingTokens(tokenStream.ReadToken(), Punctuation::OpenBracket);

            Array<double> numArray;
            numArray.arena = &TransientArena;

            Array<StringView> stringArray;
            stringArray.arena = s_MemoryArena;

            bool noFloats = true;

            while (true)
            {
                std::optional<Token> token = tokenStream.ReadToken();
                if (token == std::nullopt)
                {
                    Log::Error("Unexpected EOF in array '%'", key);
                    return false;
                }

                if (std::holds_alternative<float>(token->value) || std::holds_alternative<int32_t>(token->value))
                {
                    if (std::holds_alternative<float>(token->value))
                    {
                        noFloats = false;
                        numArray.Push(std::get<float>(token->value));
                    }
                    else
                    {
                        numArray.Push(std::get<int32_t>(token->value));
                    }
                    if (stringArray.size != 0)
                    {
                        Log::Error("Array '%' is not homogenous", key);
                        return false;
                    }
                }
                else if (std::holds_alternative<StringView>(token->value))
                {
                    if (numArray.size != 0)
                    {
                        Log::Error("Array '%' is not homogenous", key);
                        return false;
                    }
                    stringArray.Push(std::get<StringView>(token->value));
                }
                else
                {
                    Log::Error("Invalid array element '%'", Token::ToString(token.value().value));
                    return false;
                }

                std::optional<Token> punctuationToken = tokenStream.ReadToken();
                if (!ExpectMatchingTokens(punctuationToken, { Punctuation::ClosedBracket, Punctuation::Comma }))
                {
                    return false;
                }
                if (std::get<Punctuation>(punctuationToken->value) == Punctuation::ClosedBracket)
                {
                    break;
                }
            }

            if (stringArray.size != 0)
            {
                table.Set(key, stringArray);
                return true;
            }
            if (numArray.size != 0)
            {
                if (noFloats)
                {
                    Array<int32_t> array;
                    array.arena = s_MemoryArena;
                    array.Reserve(numArray.size);
                    for (double value : numArray)
                    {
                        array.Push(value);
                    }
                    table.Set(key, array);
                }
                else
                {
                    Array<float> array;
                    array.arena = s_MemoryArena;
                    array.Reserve(numArray.size);
                    for (double value : numArray)
                    {
                        array.Push(value);
                    }
                    table.Set(key, array);
                }
                return true;
            }

            Log::Error("Array '%' is empty", key);
            return false;
        }

        [[nodiscard]]
        bool ParseDeclaration(TableView& table, TokenStream& tokenStream)
        {
            std::optional<Token> variableToken = tokenStream.ReadToken();
            assert(std::holds_alternative<VariableName>(variableToken->value));
            StringView variableName = std::get<VariableName>(variableToken->value);

            if (!ExpectMatchingTokens(tokenStream.ReadToken(), Punctuation::Equals))
            {
                return false;
            }

            std::optional<Token> valueToken = tokenStream.PeekToken();
            if (valueToken == std::nullopt)
            {
                Log::Error("Unexpected EOF in declaration '%'", variableName);
                return false;
            }
            if (std::holds_alternative<int32_t>(valueToken->value))
            {
                table.Set(variableName, std::get<int32_t>(valueToken->value));
                tokenStream.ReadToken();
                return true;
            }
            if (std::holds_alternative<float>(valueToken->value))
            {
                table.Set(variableName, std::get<float>(valueToken->value));
                tokenStream.ReadToken();
                return true;
            }
            if (std::holds_alternative<StringView>(valueToken->value))
            {
                table.Set(variableName, std::get<StringView>(valueToken->value));
                tokenStream.ReadToken();
                return true;
            }
            if (std::holds_alternative<Punctuation>(valueToken->value))
            {
                if (!ExpectMatchingTokens(valueToken, Punctuation::OpenBracket))
                {
                    return false;
                }
                return ParseArray(table, variableName, tokenStream);
            }
            if (std::holds_alternative<VariableName>(valueToken->value))
            {
                if (!ExpectMatchingTokens(valueToken, { VariableName{"true"}, VariableName{"false"} }))
                {
                    return false;
                }
                table.Set(variableName, std::get<VariableName>(valueToken->value) == "true");
                tokenStream.ReadToken();
                return true;
            }

            Log::Error("Invalid token '%'", Token::ToString(valueToken.value().value));
            return false;
        }

        [[nodiscard]]
        std::optional<StringView> ParseTable(TokenStream& tokenStream)
        {
            AssertMatchingTokens(tokenStream.ReadToken(), Punctuation::OpenBracket);

            std::optional<Token> tableNameToken = tokenStream.ReadToken();
            if (tableNameToken == std::nullopt)
            {
                return std::nullopt;
            }
            if (!std::holds_alternative<VariableName>(tableNameToken->value) &&
                !std::holds_alternative<StringView> (tableNameToken->value))
            {
                return std::nullopt;
            }
            if (!ExpectMatchingTokens(tokenStream.ReadToken(), Punctuation::ClosedBracket))
            {
                return std::nullopt;
            }
            if (std::holds_alternative<VariableName>(tableNameToken->value))
            {
                return (StringView)std::get<VariableName>(tableNameToken->value);
            }
            return std::get<StringView>(tableNameToken->value);
        }

        [[nodiscard]]
        bool ParseString(TableView& table)
        {
            bool isValid = true;

            TokenStream tokenStream(table->source);
            TableView view = table;

            while (true)
            {
                std::optional<Token> token = tokenStream.PeekToken();
                if (token == std::nullopt) break;
                if (std::holds_alternative<VariableName>(token->value))
                {
                    isValid &= ParseDeclaration(view, tokenStream);
                    continue;
                }
                if (std::holds_alternative<Error>(token->value))
                {
                    isValid = false;
                    tokenStream.ReadToken();
                    continue;
                }
                if (ExpectMatchingTokens(token, Punctuation::OpenBracket))
                {
                    std::optional<StringView> tableName = ParseTable(tokenStream);
                    if (tableName == std::nullopt)
                    {
                        isValid = false;
                        continue;
                    }
                    view = table.GetSubTable(tableName.value());
                    continue;
                }
                Log::Error("Parsing broke early");
                break;
            }

            return isValid;
        }

        [[nodiscard]]
        bool ParseFile(TableView& table, StringView filepath)
        {
            table->filepath = String::Copy(filepath, s_MemoryArena);
            table->source = ReadFile(filepath, s_MemoryArena);
            bool success = ParseString(table);
            if (!success)
            {
                Log::Error("Failed to parse file '%'", filepath);
            }
            return success;
        }
    }

    // TODO: Get rid of std::stack
    static std::stack<Parser::TableView> tableStack;

    void Load(StringView filepath)
    {
        tableStack.push({});
        if (!Parser::ParseFile(tableStack.top(), filepath))
        {
            tableStack.top() = {};
        }
    }

    void LoadRaw(StringView text)
    {
        tableStack.push({});
        Parser::TableView& table = tableStack.top();
        table->filepath = "";
        table->source = text;
        if (!Parser::ParseString(tableStack.top()))
        {
            tableStack.top() = {};
        }
    }

    bool HasKey(StringView key)
    {
        assert(tableStack.size() > 0);
        return tableStack.top().Has(key);
    }

    void LogMissingKey(StringView key)
    {
        if (s_SuppressWarnings) return;
        Log::Warn("Missing key '%'", key);
    }

    template<typename T>
    std::optional<T> Get(StringView key)
    {
        assert(tableStack.size() > 0);
        Parser::TableView& table = tableStack.top();

        if constexpr (
            std::is_same_v<T, Math::float2> || std::is_same_v<T, Math::float3> || std::is_same_v<T, Math::float4> ||
            std::is_same_v<T, Math::int2>   || std::is_same_v<T, Math::int3>   || std::is_same_v<T, Math::int4>)
        {
            using ComponentType = Math::Traits<T>::ScalarType;
            constexpr int NumComponents = Math::Traits<T>::Count;

            std::array<ComponentType, NumComponents> array;
            if (!table.Get(key, array))
            {
                if constexpr (std::is_floating_point_v<ComponentType>)
                {
                    std::array<int32_t, NumComponents> intArray;
                    if (table.Get(key, intArray))
                    {
                        T out;
                        for (int i = 0; i < NumComponents; i++)
                        {
                            out[i] = intArray[i];
                        }
                        return out;
                    }
                }

                LogMissingKey(key);
                return std::nullopt;
            }

            T out;
            for (size_t i = 0; i < NumComponents; i++)
            {
                out[i] = array[i];
            }
            return out;
        }
        else
        {
            T* result = table.Get<T>(key);
            if (result == nullptr)
            {
                if constexpr (std::is_floating_point_v<T>)
                {
                    return (std::optional<float>)Get<int32_t>(key);
                }

                LogMissingKey(key);
                return std::nullopt;
            }
            return *result;
        }
    }

    // TODO: Use Span<T> rather than Array<T>

    template std::optional<float>             Get(StringView key);
    template std::optional<Math::float2>      Get(StringView key);
    template std::optional<Math::float3>      Get(StringView key);
    template std::optional<Math::float4>      Get(StringView key);
    template std::optional<Array<float>>      Get(StringView key);

    template std::optional<int32_t>           Get(StringView key);
    template std::optional<Math::int2>        Get(StringView key);
    template std::optional<Math::int3>        Get(StringView key);
    template std::optional<Math::int4>        Get(StringView key);
    template std::optional<Array<int32_t>>    Get(StringView key);

    template std::optional<bool>              Get(StringView key);

    template std::optional<StringView>        Get(StringView key);
    template std::optional<Array<StringView>> Get(StringView key);

    Array<StringView> GetTables(MemoryArena* arena)
    {
        assert(tableStack.size() > 0);
        return tableStack.top().GetSubTableNames(arena);
    }

    Array<StringView> GetKeys(MemoryArena* arena)
    {
        assert(tableStack.size() > 0);
        return tableStack.top().GetKeys(arena);
    }

    void PushTable(StringView name)
    {
        assert(tableStack.size() > 0);
        tableStack.push(tableStack.top().GetSubTable(name));
    }

    void PopTable()
    {
        assert(tableStack.size() > 0);
        tableStack.pop();
    }

    void SuppressWarnings(bool value)
    {
        s_SuppressWarnings = value;
    }
}
