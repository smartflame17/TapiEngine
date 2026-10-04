#pragma once

#include <cstdint>
#include <string_view>

namespace SceneFormat
{
	inline constexpr std::string_view Name = "TapiScene";
	inline constexpr std::string_view Extension = ".scene";
	inline constexpr std::uint32_t CurrentVersion = 1;
}
