/*
  Copyright (c) 2009, Hideyuki Tanaka
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the <organization> nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <copyright holder> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <cstring>

namespace cmdline
{
    namespace detail
    {

        template <typename Target, typename Source>
        struct lexical_cast_t
        {
            static Target cast(const Source &arg)
            {
                return static_cast<Target>(arg);
            }
        };

        template <typename Source>
        struct lexical_cast_t<std::string, Source>
        {
            static std::string cast(const Source &arg)
            {
                return std::to_string(arg);
            }
        };

        template <typename Target>
        struct lexical_cast_t<Target, std::string>
        {
            static Target cast(const std::string &arg)
            {
                static std::istringstream iss;
                Target ret{};
                iss.str(arg);
                iss >> ret;
                iss.clear();
                return ret;
            }
        };

        template <typename Target, typename Source, std::enable_if_t<!std::is_same<Target, Source>::value> * = nullptr>
        Target lexical_cast(const Source &arg)
        {
            return lexical_cast_t<Target, Source>::cast(arg);
        }

        template <typename Target, typename Source, std::enable_if_t<std::is_same<Target, Source>::value> * = nullptr>
        Target lexical_cast(const Source &arg)
        {
            return arg;
        }

        template <typename T, std::enable_if_t<std::is_integral<T>::value> * = nullptr>
        std::string readable_typename()
        {
            return "Integer";
        }

        template <typename T, std::enable_if_t<std::is_floating_point<T>::value> * = nullptr>
        std::string readable_typename()
        {
            return "Floating-point";
        }

        template <typename T, std::enable_if_t<std::is_same<T, std::string>::value> * = nullptr>
        std::string readable_typename()
        {
            return "String";
        }

        template <typename T>
        std::string default_value(T def)
        {
            return detail::lexical_cast<std::string>(def);
        }

    } // detail

    //-----
    template <class T>
    struct default_reader
    {
        T operator()(const std::string &str)
        {
            return detail::lexical_cast<T>(str);
        }
    };

    template <class T>
    struct range_reader
    {
        range_reader(const T &low, const T &high) : low(low), high(high) {}
        T operator()(const std::string &s) const
        {
            T ret = default_reader<T>()(s);
            if (!(ret >= low && ret <= high))
                throw std::exception();
            return ret;
        }

    private:
        T low, high;
    };

    template <class T>
    range_reader<T> range(const T &low, const T &high)
    {
        return range_reader<T>(low, high);
    }

    template <class T>
    struct oneof_reader
    {
        oneof_reader(std::initializer_list<T> list) : alt(list) {}

        T operator()(const std::string &s)
        {
            T ret = default_reader<T>()(s);
            if (std::find(alt.begin(), alt.end(), ret) == alt.end())
                throw std::exception();
            return ret;
        }

    private:
        std::vector<T> alt;
    };

    template <class T, class... Args>
    oneof_reader<T> oneof(T a1, Args... args)
    {
        return oneof_reader<T>{a1, args...};
    }

    template <class Target, class Checker>
    auto custom_reader(Checker &&checker)
    {
        return [checker](const std::string &s) -> Target
        {
            Target ret = default_reader<Target>()(s);
            if (!checker(ret))
                throw std::exception();
            return ret;
        };
    }
    //-----

    class parser
    {
    public:
        void add(const std::string &name,
                 char short_name = 0,
                 const std::string &desc = std::string{})
        {
            if (options.find(name) != options.end())
            {
                std::cerr << "multiple definition: " + name << '\n';
                std::exit(1);
            }
            options.emplace(name, std::make_shared<option_without_value>(name, short_name, desc));
            ordered.emplace_back(options[name]);
        }

        template <class T, class F = default_reader<T>>
        void add(const std::string &name,
                 char short_name = 0,
                 const std::string &desc = std::string{},
                 bool need = true,
                 const T def = T(),
                 F reader = F())
        {
            if (options.find(name) != options.end())
            {
                std::cerr << "multiple definition: " + name << '\n';
                std::exit(1);
            }
            options.emplace(name, std::make_shared<option_with_value_with_reader<T, F>>(name, short_name, need, def, desc, reader));
            ordered.emplace_back(options[name]);
        }

        void footer(const std::string &f)
        {
            ftr = f;
        }

        void set_program_name(const std::string &name)
        {
            prog_name = name;
        }

        bool exist(const std::string &name) const
        {
            if (options.find(name) == options.end())
            {
                std::cerr << "there is no flag: --" + name << '\n'
                          << usage();
                std::exit(1);
            }
            return options.find(name)->second->has_set();
        }

        template <class T>
        const T &get(const std::string &name) const
        {
            if (options.find(name) == options.end())
            {
                std::cerr << "there is no flag: --" + name << '\n'
                          << usage();
                std::exit(1);
            }
            auto p = std::dynamic_pointer_cast<const option_with_value<T>>(options.find(name)->second);
            if (p == nullptr)
            {
                std::cerr << "type mismatch flag '" + name + "'" << '\n'
                          << usage();
                std::exit(1);
            }
            return p->get();
        }

        const std::vector<std::string> &rest() const
        {
            return others;
        }

        bool parse(const std::string &arg)
        {
            std::vector<std::string> args;

            std::string buf;
            bool in_quote = false;
            for (std::size_t i = 0; i < arg.length(); i++)
            {
                if (arg[i] == '\"')
                {
                    in_quote = !in_quote;
                    continue;
                }

                if (arg[i] == ' ' && !in_quote)
                {
                    args.emplace_back(buf);
                    buf = "";
                    continue;
                }

                if (arg[i] == '\\')
                {
                    i++;
                    if (i >= arg.length())
                    {
                        errors.emplace_back("unexpected occurrence of '\\' at end of string");
                        return false;
                    }
                }

                buf += arg[i];
            }

            if (in_quote)
            {
                errors.emplace_back("quote is not closed");
                return false;
            }

            if (buf.length() > 0)
                args.emplace_back(buf);

            for (std::size_t i = 0; i < args.size(); i++)
                std::cout << "\"" << args[i] << "\"" << std::endl;

            return parse(args);
        }

        bool parse(const std::vector<std::string> &args)
        {
            int argc = static_cast<int>(args.size());
            std::vector<const char *> argv(argc);

            for (int i = 0; i < argc; i++)
                argv[i] = args[i].c_str();

            return parse(argc, &argv[0]);
        }

        bool parse(int argc, const char *const argv[])
        {
            errors.clear();
            others.clear();

            if (argc < 1)
            {
                errors.emplace_back("argument number must be longer than 0");
                return false;
            }
            if (prog_name.empty())
                prog_name = argv[0];

            std::unordered_map<char, std::string> lookup;
            for (auto p = options.begin(); p != options.end(); p++)
            {
                if (p->first.length() == 0)
                    continue;
                char initial = p->second->short_name();
                if (initial)
                {
                    if (lookup.find(initial) != lookup.end())
                    {
                        lookup[initial].clear();
                        errors.emplace_back(std::string("short option '") + initial + "' is ambiguous");
                        return false;
                    }
                    else
                        lookup[initial] = p->first;
                }
            }

            for (int i = 1; i < argc; i++)
            {
                if (std::strncmp(argv[i], "--", 2) == 0)
                {
                    const char *p = std::strchr(argv[i] + 2, '=');
                    if (p)
                    {
                        set_option(std::string{argv[i] + 2, p}, std::string{p + 1});
                    }
                    else
                    {
                        std::string name(argv[i] + 2);
                        if (options.find(name) == options.end())
                        {
                            errors.emplace_back("undefined option: --" + name);
                        }
                        else if (options[name]->has_value())
                        {
                            if (i + 1 >= argc)
                            {
                                errors.emplace_back("option needs value: --" + name);
                            }
                            else
                            {
                                set_option(name, argv[++i]);
                            }
                        }
                        else
                        {
                            set_option(name);
                        }
                    }
                }
                else if (std::strncmp(argv[i], "-", 1) == 0)
                {
                    for (int j = 1; argv[i][j]; j++)
                    {
                        if (lookup.find(argv[i][j]) == lookup.end())
                        {
                            errors.emplace_back(std::string("undefined short option: -") + argv[i][j]);
                        }
                        else
                        {
                            std::string name = lookup[argv[i][j]];
                            if (!argv[i][j + 1] && i + 1 < argc && options[name]->has_value())
                            {
                                set_option(name, argv[++i]);
                                break;
                            }
                            else
                            {
                                set_option(name);
                            }
                        }
                    }
                }
                else
                {
                    others.emplace_back(argv[i]);
                }
            }

            if (errors.empty())
            {
                for (auto p = options.begin(); p != options.end(); p++)
                    if (!p->second->valid())
                        errors.emplace_back("need option: --" + p->first);
                return errors.empty();
            }

            return false;
        }

        void parse_check(const std::string &arg)
        {
            if (options.find("help") == options.end())
                add("help", '?', "print this message");
            check(parse(arg));
        }

        void parse_check(const std::vector<std::string> &args)
        {
            if (options.find("help") == options.end())
                add("help", '?', "print this message");
            check(parse(args));
        }

        void parse_check(int argc, char *argv[])
        {
            if (options.find("help") == options.end())
                add("help", '?', "print this message");
            check(parse(argc, argv));
        }

        std::string error() const
        {
            std::ostringstream os;
            auto b = std::begin(errors), e = std::end(errors);

            if (b != e)
            {
                std::copy(b, std::prev(e), std::ostream_iterator<std::string>(os, "\n"));
                b = std::prev(e);
            }
            if (b != e)
            {
                os << *b;
            }

            return os.str();
        }

        std::string usage() const
        {
            std::ostringstream oss;
            oss << "usage: " << prog_name << " ";
            for (std::size_t i = 0; i < ordered.size(); i++)
            {
                if (ordered[i]->must())
                    oss << ordered[i]->short_description() << " ";
            }

            oss << "[options] ... " << ftr << std::endl;
            oss << "options:" << std::endl;

            std::size_t max_width = 0;
            for (std::size_t i = 0; i < ordered.size(); i++)
            {
                max_width = std::max(max_width, ordered[i]->name().length());
            }
            for (std::size_t i = 0; i < ordered.size(); i++)
            {
                if (ordered[i]->short_name())
                {
                    oss << "  -" << ordered[i]->short_name() << ", ";
                }
                else
                {
                    oss << "      ";
                }

                oss << "--" << ordered[i]->name();
                for (std::size_t j = ordered[i]->name().length(); j < max_width + 4; j++)
                    oss << ' ';
                oss << ordered[i]->description() << std::endl;
            }
            return oss.str();
        }

    private:
        void check(bool ok)
        {
            if (exist("help"))
            {
                std::cout << usage() << std::endl;
                std::exit(0);
            }
            if (!ok)
            {
                std::cerr << error() << '\n'
                          << usage();
                std::exit(1);
            }
        }

        void set_option(const std::string &name)
        {
            if (options.find(name) == options.end())
            {
                errors.emplace_back("undefined option: --" + name);
                return;
            }
            if (!options[name]->set())
            {
                errors.emplace_back("option needs value: --" + name);
                return;
            }
        }

        void set_option(const std::string &name, const std::string &value)
        {
            if (options.find(name) == options.end())
            {
                errors.emplace_back("undefined option: --" + name);
                return;
            }
            if (!options[name]->set(value))
            {
                errors.emplace_back("option value is invalid: --" + name + "=" + value);
                return;
            }
        }

        class option_base
        {
        public:
            virtual ~option_base() {}

            virtual bool has_value() const = 0;
            virtual bool set() = 0;
            virtual bool set(const std::string &value) = 0;
            virtual bool has_set() const = 0;
            virtual bool valid() const = 0;
            virtual bool must() const = 0;

            virtual const std::string &name() const = 0;
            virtual char short_name() const = 0;
            virtual const std::string &description() const = 0;
            virtual std::string short_description() const = 0;
        };

        class option_without_value : public option_base
        {
        public:
            option_without_value(const std::string &name,
                                 char short_name,
                                 const std::string &desc)
                : nam(name), snam(short_name), has(false)
            {
                this->desc = format_description(desc);
            }
            ~option_without_value() {}

            bool has_value() const { return false; }

            bool set()
            {
                has = true;
                return true;
            }

            bool set(const std::string &)
            {
                return false;
            }

            bool has_set() const
            {
                return has;
            }

            bool valid() const
            {
                return true;
            }

            bool must() const
            {
                return false;
            }

            const std::string &name() const
            {
                return nam;
            }

            char short_name() const
            {
                return snam;
            }

            const std::string &description() const
            {
                return desc;
            }

            std::string short_description() const
            {
                return "--" + nam;
            }

        protected:
            std::string format_description(const std::string &desc)
            {
                std::size_t size = desc.size();
                std::size_t width = 60;
                std::string tmp = "\n        ";
                for (std::size_t i = 0; i < size; i += width)
                    tmp += (desc.substr(i, width > size - i ? size - i : width) + "\n        ");
                return tmp;
            }

        private:
            std::string nam;
            char snam;
            std::string desc;
            bool has;
        };

        template <class T>
        class option_with_value : public option_base
        {
        public:
            option_with_value(const std::string &name,
                              char short_name,
                              bool need,
                              const T &def,
                              const std::string &desc)
                : nam(name), snam(short_name), need(need), has(false), def(def), actual(def)
            {
                this->desc = format_description(desc);
            }
            ~option_with_value() {}

            const T &get() const
            {
                return actual;
            }

            bool has_value() const { return true; }

            bool set()
            {
                return false;
            }

            bool set(const std::string &value)
            {
                try
                {
                    actual = read(value);
                    has = true;
                }
                catch (const std::exception)
                {
                    return false;
                }
                return true;
            }

            bool has_set() const
            {
                return has;
            }

            bool valid() const
            {
                if (need && !has)
                    return false;
                return true;
            }

            bool must() const
            {
                return need;
            }

            const std::string &name() const
            {
                return nam;
            }

            char short_name() const
            {
                return snam;
            }

            const std::string &description() const
            {
                return desc;
            }

            std::string short_description() const
            {
                return "--" + nam + "=" + detail::readable_typename<T>();
            }

        protected:
            std::string format_description(const std::string &desc)
            {
                std::size_t size = desc.size();
                std::size_t width = 60;
                std::string tmp = "\n        ";
                for (std::size_t i = 0; i < size; i += width)
                    tmp += (desc.substr(i, width > size - i ? size - i : width) + "\n        ");
                return tmp + " (" + detail::readable_typename<T>() +
                       (need ? "" : " [=" + detail::default_value<T>(def) + "]") + ")\n";
            }

            virtual T read(const std::string &s) = 0;

            std::string nam;
            char snam;
            bool need;
            std::string desc;

            bool has;
            T def;
            T actual;
        };

        template <class T, class F>
        class option_with_value_with_reader : public option_with_value<T>
        {
        public:
            option_with_value_with_reader(const std::string &name,
                                          char short_name,
                                          bool need,
                                          const T def,
                                          const std::string &desc,
                                          F reader)
                : option_with_value<T>(name, short_name, need, def, desc), reader(reader) {}

        private:
            T read(const std::string &s)
            {
                return reader(s);
            }

            F reader;
        };

    private:
        std::unordered_map<std::string, std::shared_ptr<option_base>> options;
        std::vector<std::shared_ptr<option_base>> ordered;
        std::string ftr;

        std::string prog_name;
        std::vector<std::string> others;
        std::vector<std::string> errors;
    };

} // cmdline
