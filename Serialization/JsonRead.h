#pragma once
#include "JsonMath.h"
#include "AssetPath.h"
#include <stdexcept>

// Relative field paths are expanded to full object/component paths by the loader.
class MetadataError : public std::invalid_argument
{
public:
	MetadataError(std::string field, std::string message) : std::invalid_argument(std::move(message)), field(std::move(field)) {}
	std::string field;
};

namespace JsonRead
{
	inline void Object(const nlohmann::json& data)
	{
		if (!data.is_object()) throw MetadataError("", "Expected a JSON object.");
	}
	template<typename T>
	T Value(const nlohmann::json& data, const char* key, const T& fallback)
	{
		if (!data.contains(key)) return fallback;
		try { return data.at(key).get<T>(); }
		catch (const std::exception& error) { throw MetadataError("." + std::string(key), error.what()); }
	}
	inline float Float(const nlohmann::json& data, const char* key, float fallback)
	{
		if (!data.contains(key)) return fallback;
		try { return JsonDetail::ReadFloatArray<1>(nlohmann::json::array({ data.at(key) }))[0]; }
		catch (const std::exception& error) { throw MetadataError("." + std::string(key), error.what()); }
	}
	inline float Nonnegative(const nlohmann::json& data, const char* key, float fallback, bool positive = false)
	{
		const float value = Float(data, key, fallback);
		if (value < 0 || (positive && value == 0))
			throw MetadataError("." + std::string(key), positive ? "Expected a positive value." : "Expected a nonnegative value.");
		return value;
	}
	inline std::string Path(const nlohmann::json& data, const char* key, const std::filesystem::path& root, bool required = false)
	{
		const auto reference = Value<std::string>(data, key, "");
		try { if (required || !reference.empty()) AssetPath::Resolve(reference, root); }
		catch (const std::exception& error) { throw MetadataError("." + std::string(key), error.what()); }
		return reference;
	}
	inline std::size_t Index(const nlohmann::json& data, const char* key, std::size_t fallback)
	{
		if (!data.contains(key)) return fallback;
		const auto& value = data.at(key);
		if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>() < 0))
			throw MetadataError("." + std::string(key), "Expected a nonnegative integer index.");
		return value.get<std::size_t>();
	}
}
