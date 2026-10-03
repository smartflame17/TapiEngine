#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace AssetPath
{
	inline std::filesystem::path FromUtf8(std::string_view text)
	{
		return std::filesystem::path(std::u8string(text.begin(), text.end()));
	}
	inline std::string ToUtf8(const std::filesystem::path& path)
	{
		const auto text = path.generic_u8string();
		return std::string(text.begin(), text.end());
	}
	std::filesystem::path Resolve(const std::string& reference, const std::filesystem::path& projectRoot);
	std::string Reference(const std::filesystem::path& source, const std::filesystem::path& projectRoot);
}
