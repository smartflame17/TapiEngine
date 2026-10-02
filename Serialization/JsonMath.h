#pragma once

#include "Json.h"
#include "../Scene/Transform.h"
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace JsonDetail
{
	inline void RequireFinite(float value)
	{
		if (!std::isfinite(value))
			throw std::invalid_argument("JSON math values must be finite.");
	}

	template<std::size_t Size>
	std::array<float, Size> ReadFloatArray(const nlohmann::json& in)
	{
		if (!in.is_array() || in.size() != Size)
			throw std::invalid_argument("Expected a numeric array with " + std::to_string(Size) + " elements.");
		std::array<float, Size> result{};
		for (std::size_t i = 0; i < Size; ++i)
		{
			if (!in[i].is_number())
				throw std::invalid_argument("JSON math array elements must be numbers.");
			const auto value = in[i].get<double>();
			if (!std::isfinite(value) || std::abs(value) > (std::numeric_limits<float>::max)())
				throw std::invalid_argument("JSON math values must be finite and fit in a float.");
			result[i] = static_cast<float>(value);
		}
		return result;
	}
}

// DirectX types need their converters in the DirectX namespace for ADL.
namespace DirectX
{
	inline void to_json(nlohmann::json& out, const XMFLOAT2& value)
	{
		JsonDetail::RequireFinite(value.x); JsonDetail::RequireFinite(value.y);
		out = nlohmann::json::array({ value.x, value.y });
	}
	inline void from_json(const nlohmann::json& in, XMFLOAT2& value)
	{
		const auto elements = JsonDetail::ReadFloatArray<2>(in);
		value = { elements[0], elements[1] };
	}
	inline void to_json(nlohmann::json& out, const XMFLOAT3& value)
	{
		JsonDetail::RequireFinite(value.x); JsonDetail::RequireFinite(value.y); JsonDetail::RequireFinite(value.z);
		out = nlohmann::json::array({ value.x, value.y, value.z });
	}
	inline void from_json(const nlohmann::json& in, XMFLOAT3& value)
	{
		const auto elements = JsonDetail::ReadFloatArray<3>(in);
		value = { elements[0], elements[1], elements[2] };
	}
	inline void to_json(nlohmann::json& out, const XMFLOAT4& value)
	{
		JsonDetail::RequireFinite(value.x); JsonDetail::RequireFinite(value.y);
		JsonDetail::RequireFinite(value.z); JsonDetail::RequireFinite(value.w);
		out = nlohmann::json::array({ value.x, value.y, value.z, value.w });
	}
	inline void from_json(const nlohmann::json& in, XMFLOAT4& value)
	{
		const auto elements = JsonDetail::ReadFloatArray<4>(in);
		value = { elements[0], elements[1], elements[2], elements[3] };
	}
}

inline void to_json(nlohmann::json& out, const Transform& value)
{
	out = { { "position", value.position }, { "rotation", value.rotation }, { "scale", value.scale } };
}

inline void from_json(const nlohmann::json& in, Transform& value)
{
	if (!in.is_object())
		throw std::invalid_argument("Expected a transform object.");
	// Stage into a temporary so malformed fields cannot partially change a transform.
	Transform restored;
	in.at("position").get_to(restored.position);
	in.at("rotation").get_to(restored.rotation);
	in.at("scale").get_to(restored.scale);
	value = restored;
}
