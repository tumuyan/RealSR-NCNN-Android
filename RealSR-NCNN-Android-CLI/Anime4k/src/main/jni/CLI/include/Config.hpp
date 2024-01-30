#ifndef ANIME4KCPP_CLI_CONFIG_HPP
#define ANIME4KCPP_CLI_CONFIG_HPP

#include <string>
#include <string_view>

#include <cmdline.hpp>
#include <ini17.hpp>

class Config
{
public:
	cmdline::parser& getOptParser()
	{
		return optParser;
	}

	ini17::Parser& getIniParser()
	{
		return iniParser;
	}

	bool parser(int argc, char* argv[])
	{
		optParser.add<std::string>("configTemplate", '\000', "Generate config template", false);
		optParser.parse_check(argc, argv);
		generateConfigTemplateFlag = optParser.exist("configTemplate");
		if (!optParser.rest().empty() && !iniParser.parseFile(optParser.rest().front()))
			return false;
		return true;
	}

	template<typename T>
	T get(std::string_view key)
	{
		if (generateConfigTemplateFlag)
		{
			auto&& value = optParser.get<T>(key.data());
			iniSection.add(key, value);
			return value;
		}
		if (auto check = iniParser.get<T>(key))
		{
			if (optParser.exist(key.data()))
				return optParser.get<T>(key.data());
			return *check;
		}
		return optParser.get<T>(key.data());
	}

	bool exist(std::string_view key)
	{
		if (generateConfigTemplateFlag)
		{
			bool value = optParser.exist(key.data());
			iniSection.add(key, value);
			return value;
		}
		if (auto check = iniParser.get<bool>(key))
		{
			if (optParser.exist(key.data()))
				return true;
			return *check;
		}
		return optParser.exist(key.data());
	}

	bool checkGenerateConfigTemplate()
	{
		return generateConfigTemplateFlag;
	}

	std::optional<std::string_view> generateConfigTemplate()
	{
		std::string_view path = optParser.get<std::string>("configTemplate");
		ini17::Generator generator;
		generator.setHeader("Anime4KCPP CLI config file");
		generator.push(iniSection);
		return generator.generateFile(path) ? std::make_optional(path) : std::nullopt;
	}

private:
	bool generateConfigTemplateFlag = false;
	cmdline::parser optParser;
	ini17::Parser iniParser;
	ini17::Section iniSection;
};


#endif // !ANIME4KCPP_CLI_CONFIG_HPP
