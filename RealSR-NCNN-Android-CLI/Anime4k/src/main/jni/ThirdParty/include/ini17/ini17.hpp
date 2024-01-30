#include <type_traits>
#include <unordered_map>
#include <vector>
#include <list>
#include <optional>
#include <fstream>
#include <string>
#include <sstream>
#include <string_view>
#include <cctype>
#include <cstddef>

namespace ini17
{
    namespace detail
    {
        template <typename T>
        struct is_string
            : std::integral_constant<bool,
                std::is_same_v<const char *, std::decay_t<T>>      ||
                std::is_same_v<char *, std::decay_t<T>>            ||
                std::is_same_v<std::string, std::decay_t<T>>       ||
                std::is_same_v<std::string_view, std::decay_t<T>>> {};

        template <typename T>
        inline constexpr bool is_string_v = is_string<T>::value;
        template <typename T>
        inline constexpr bool not_string_v = !is_string_v<T>;

        template <typename T>
        inline T fromStringCast(const std::string &arg)
        {
            static std::istringstream iss;
            T ret{};
            iss.str(arg);
            iss >> ret;
            iss.clear();
            return ret;
        }

        template <>
        inline std::string fromStringCast<std::string>(const std::string &arg)
        {
            return arg;
        }

        template <typename T, std::enable_if_t<not_string_v<T>> * = nullptr>
        inline std::string toStringCast(const T &arg)
        {
            return std::to_string(arg);
        }

        template <typename T, std::enable_if_t<is_string_v<T>> * = nullptr>
        inline std::string toStringCast(const std::string &arg)
        {
            return arg;
        }

        enum class TokenType
        {
            Section,
            Key,
            Value,
            Equal,
            Bracket
        };

        struct Token
        {
            TokenType type;
            std::string value;
            std::size_t line;

            Token(TokenType type, std::string_view value, std::size_t line)
                : type(type), value(value), line(line) {}
        };

    } // namespace detail

    using KeyValueType = std::unordered_map<std::string, std::string>;

    class Section
    {
    private:
        class KeyValueEditer
        {
        public:
            KeyValueEditer(std::string &valueReference) : valueReference(valueReference) {}

            template <typename T>
            void operator=(const T &value)
            {
                valueReference = detail::toStringCast<T>(value);
            }

            template <typename T>
            operator T() const
            {
                return detail::fromStringCast<T>(valueReference);
            }

            operator const std::string &() const
            {
                return valueReference;
            }

        private:
            std::string &valueReference;
        };

    public:
        Section(std::string_view name, KeyValueType kv) : name(name), kv(std::move(kv)) {}
        Section(std::string_view name) : Section(name, KeyValueType{}) {}
        Section(KeyValueType kv) : Section(std::string_view{}, std::move(kv)) {}
        Section() : Section(std::string_view{}, KeyValueType{}) {}

        void setName(std::string_view name)
        {
            this->name = name;
        }

        std::string_view getName() const
        {
            return name;
        }

        const KeyValueType &getKeyValueMap() const
        {
            return kv;
        }

        template <typename T>
        std::optional<T> get(std::string_view key) const
        {
            auto vit = kv.find(key.data());
            if (vit == kv.end())
                return std::nullopt;
            return detail::fromStringCast<T>(vit->second);
        }

        template <typename T>
        bool set(std::string_view key, const T &value)
        {
            auto vit = kv.find(key.data());
            if (vit == kv.end())
                return false;
            vit->second.assign(detail::toStringCast<T>(value));
            return true;
        }

        template <typename T>
        bool add(std::string_view key, const T &value)
        {
            return kv.emplace(key, detail::toStringCast<T>(value)).second;
        }

        KeyValueEditer operator[](std::string_view key)
        {
            auto vit = kv.find(key.data());
            if (vit != kv.end())
                return vit->second;
            return kv.emplace(key, std::string{}).first->second;
        }

    private:
        std::string name;
        KeyValueType kv;
    };

    class Parser
    {
    public:
        template <typename T>
        std::optional<T> get(std::string_view name) const
        {
            if constexpr (std::is_same_v<T, Section>)
            {
                auto sit = result.find(name.data());
                if (sit == result.end())
                    return std::nullopt;
                return Section{name, sit->second};
            }
            else
            {
                return get<T>(defaultSectionName, name);
            }
        }

        template <typename T>
        std::optional<T> get(std::string_view section, std::string_view key) const
        {
            auto sit = result.find(section.data());
            if (sit == result.end())
                return std::nullopt;
            auto vit = sit->second.find(key.data());
            if (vit == sit->second.end())
                return std::nullopt;
            return detail::fromStringCast<T>(vit->second);
        }

        bool parseFile(std::string_view filePath)
        {
            std::string contents;
            std::ifstream file(filePath.data(), std::ios::in | std::ios::ate);
            if (!file.is_open())
                return false;
            contents.resize(file.tellg());
            file.seekg(0, std::ios::beg);
            file.read(&contents[0], contents.size());
            file.close();
            return parse(contents);
        }

        bool parse(std::string_view src)
        {
            if (!result.empty())
                result.clear();
            auto tokens = tokenize(src);
            if (!tokens.has_value())
            {
                pushError("Failed to parse source file.");
                return false;
            }
            analyze(*tokens);
            return true;
        }

        const std::vector<std::string> &error() const
        {
            return errors;
        }

        void setDefaultSectionName(std::string_view section)
        {
            defaultSectionName = section;
        }

        std::string_view getDefaultSectionName() const
        {
            return defaultSectionName;
        }

    private:
        template <typename Token>
        void pushError(std::size_t line, Token token)
        {
            errors.emplace_back("error: " + std::to_string(line) + ": unexcept token: " + token);
        }

        void pushError(std::string_view err)
        {
            errors.emplace_back(err);
        }

    private:
        std::optional<std::vector<detail::Token>> tokenize(std::string_view src)
        {
            std::vector<detail::Token> tokens;
            auto length = src.size();
            std::string_view::size_type count = 0;
            std::size_t line = 1;

            auto boundaryCheck = [this, length](std::size_t current) -> bool
            {
                if (current >= length)
                {
                    pushError("The source file cannot be resolved");
                    return false;
                }

                return true;
            };

            while (count < length)
            {
                char c = src[count];

                if (c == '\n')
                {
                    line++;
                    count++;
                    continue;
                }

                if (c == '#' || c == ';')
                {
                    do
                        c = src[++count];
                    while (c != '\n' && count < length);

                    count++;
                    line++;
                    continue;
                }

                if (c == '[')
                {
                    tokens.emplace_back(detail::TokenType::Bracket, "[", line);

                    std::string section;

                    while ((c = src[++count]) != ']' && !std::isspace(c) && count < length)
                        section.push_back(c);

                    if (!boundaryCheck(count))
                        return std::nullopt;

                    while (std::isspace(c))
                    {
                        if (c == '\n')
                        {
                            pushError(line, c);
                            return std::nullopt;
                        }
                        c = src[++count];
                        if (!boundaryCheck(count))
                            return std::nullopt;
                    }

                    if (c != ']')
                    {
                        pushError(line, c);
                        return std::nullopt;
                    }

                    tokens.emplace_back(detail::TokenType::Section, section, line);
                    tokens.emplace_back(detail::TokenType::Bracket, "]", line);
                    count++;
                    continue;
                }

                if (std::isgraph(c))
                {
                    std::string key{c};

                    while ((c = src[++count]) != '=' && !std::isspace(c) && count < length)
                        key.push_back(c);

                    if (!boundaryCheck(count))
                        return std::nullopt;

                    while (std::isspace(c))
                    {
                        if (c == '\n')
                        {
                            pushError(line, c);
                            return std::nullopt;
                        }
                        c = src[++count];
                        if (!boundaryCheck(count))
                            return std::nullopt;
                    }

                    if (c != '=')
                    {
                        pushError(line, c);
                        return std::nullopt;
                    }

                    tokens.emplace_back(detail::TokenType::Key, key, line);
                    tokens.emplace_back(detail::TokenType::Equal, "=", line);

                    while (std::isspace(c = src[++count]))
                    {
                        if (!boundaryCheck(count))
                            return std::nullopt;
                    }

                    if (std::isgraph(c))
                    {
                        std::string value{c};
                        for (;;)
                        {
                            while ((c = src[++count]) != '\n' && !std::isspace(c) && count < length)
                                value.push_back(c);

                            if (!boundaryCheck(count))
                                return std::nullopt;

                            if (c != '\n')
                            {
                                std::string buf{c};

                                do
                                {
                                    buf.push_back(c = src[++count]);
                                    if (!boundaryCheck(count))
                                        return std::nullopt;
                                } while (c != '\n' && std::isspace(c));

                                if (c != '\n')
                                    value.append(buf);
                                else
                                    break;
                            }
                            else
                                break;
                        };

                        tokens.emplace_back(detail::TokenType::Value, value, line);
                        line++;
                        count++;
                        continue;
                    }
                }
                count++;
            }

            return std::make_optional(tokens);
        }

        void analyze(const std::vector<detail::Token> &tokens)
        {
            auto length = tokens.size();
            std::vector<detail::Token>::size_type count = 0;

            ResultType::iterator it;

            auto boundaryCheck = [this, length](std::size_t current) -> bool
            {
                if (current >= length)
                {
                    pushError("The token list cannot be resolved");
                    return false;
                }

                return true;
            };

            if (length && tokens.front().type != detail::TokenType::Bracket)
            {
                it = result.emplace(defaultSectionName, KeyValueType{}).first;
            }

            while (count < length)
            {
                if (tokens[count].type == detail::TokenType::Bracket)
                {
                    if (!boundaryCheck(count + 2))
                        return;

                    auto &lBracketToken = tokens[count];
                    auto &sectionToken = tokens[count + 1];
                    auto &rBracketToken = tokens[count + 2];

                    if (sectionToken.type != detail::TokenType::Section || rBracketToken.type != detail::TokenType::Bracket)
                    {
                        pushError(lBracketToken.line, lBracketToken.value);
                        return;
                    }

                    it = result.emplace(sectionToken.value, KeyValueType{}).first;

                    count += 3;
                    continue;
                }

                if (tokens[count].type == detail::TokenType::Key)
                {
                    if (!boundaryCheck(count + 2))
                        return;

                    auto &keyToken = tokens[count];
                    auto &equalToken = tokens[count + 1];
                    auto &valueToken = tokens[count + 2];

                    if (equalToken.type != detail::TokenType::Equal || valueToken.type != detail::TokenType::Value)
                    {
                        pushError(keyToken.line, keyToken.value);
                        return;
                    }

                    auto &&ret = it->second.emplace(keyToken.value, valueToken.value);
                    if (!ret.second)
                        pushError("duplicate key: " + tokens[count].value + ", in section: " + it->first);

                    count += 3;
                    continue;
                }
                count++;
            }
        }

    private:
        using ResultType = std::unordered_map<std::string, KeyValueType>;

        ResultType result;
        std::vector<std::string> errors;
        std::string defaultSectionName = "global";
    };

    class Generator
    {
    public:
        template <typename... Sections>
        std::enable_if_t<std::conjunction_v<std::is_same<Section, std::remove_reference_t<Sections>>...>>
        push(Sections &&...args)
        {
            (sections.emplace_back(std::forward<Sections>(args)), ...);
        }

        template <typename... Args>
        std::enable_if_t<std::disjunction_v<std::negation<std::is_same<Section, std::remove_reference_t<Args>>>...>>
        push(Args &&...args)
        {
            sections.emplace_back(std::forward<Args>(args)...);
        }

        void clear() noexcept
        {
            sections.clear();
        }

        void setHeader(std::string_view msg)
        {
            header = msg;
        }

        void setFooter(std::string_view msg)
        {
            footer = msg;
        }

        bool generateFile(std::string_view filePath) const
        {
            std::ofstream file(filePath.data());
            if (!file.is_open())
                return false;
            file << generate();
            file.close();
            return true;
        }

        std::string generate() const
        {
            std::string ret;
            if (!header.empty())
            {
                ret.push_back(';');
                ret.append(header).append("\n\n");
            }
            for (auto &&section : sections)
            {
                auto &&kvMap = section.getKeyValueMap();
                auto name = section.getName();
                if (!name.empty())
                {
                    ret.push_back('[');
                    ret.append(name);
                    ret.push_back(']');
                    ret.push_back('\n');
                }
                for (auto &&kv : kvMap)
                {
                    ret.append(kv.first).append(" = ").append(kv.second).push_back('\n');
                }
                ret.push_back('\n');
            }
            if (!footer.empty())
            {
                ret.push_back(';');
                ret.append(footer);
                ret.push_back('\n');
            }
            return ret;
        }

    private:
        std::string header;
        std::string footer;
        std::list<Section> sections;
    };

} // namespace ini17
