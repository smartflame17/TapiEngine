#pragma once

// Reuse the repository's vendored nlohmann/json 3.12.0.
#include "../Tools/json.hpp"
#include "Guid.h"

inline void to_json(nlohmann::json& out, const Guid& value)
{
	out = value.ToString();
}

inline void from_json(const nlohmann::json& in, Guid& value)
{
	value = Guid::FromString(in.get<std::string>());
}
