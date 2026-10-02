#include "Guid.h"
#include "../SmflmWin.h"
#include <objbase.h>
#include <algorithm>
#include <stdexcept>

Guid Guid::Generate()
{
	GUID native{};
	const auto result = CoCreateGuid(&native);
	if (FAILED(result))
		throw std::runtime_error("CoCreateGuid failed (HRESULT " + std::to_string(result) + ").");

	Guid guid;
	guid.bytes[0] = static_cast<std::uint8_t>(native.Data1 >> 24);
	guid.bytes[1] = static_cast<std::uint8_t>(native.Data1 >> 16);
	guid.bytes[2] = static_cast<std::uint8_t>(native.Data1 >> 8);
	guid.bytes[3] = static_cast<std::uint8_t>(native.Data1);
	guid.bytes[4] = static_cast<std::uint8_t>(native.Data2 >> 8);
	guid.bytes[5] = static_cast<std::uint8_t>(native.Data2);
	guid.bytes[6] = static_cast<std::uint8_t>(native.Data3 >> 8);
	guid.bytes[7] = static_cast<std::uint8_t>(native.Data3);
	std::copy(std::begin(native.Data4), std::end(native.Data4), guid.bytes.begin() + 8);
	if (guid.IsNull())
		throw std::runtime_error("CoCreateGuid returned a null UUID.");
	return guid;
}

std::optional<Guid> Guid::TryParse(std::string_view text) noexcept
{
	if (text.size() != 36 || text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-')
		return std::nullopt;
	const auto hex = [](char c) noexcept -> int
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	};
	Guid guid;
	std::size_t offset = 0;
	for (auto& byte : guid.bytes)
	{
		if (offset == 8 || offset == 13 || offset == 18 || offset == 23) ++offset;
		const int high = hex(text[offset++]);
		const int low = hex(text[offset++]);
		if (high < 0 || low < 0) return std::nullopt;
		byte = static_cast<std::uint8_t>((high << 4) | low);
	}
	return guid;
}

Guid Guid::FromString(std::string_view text)
{
	const auto parsed = TryParse(text);
	if (!parsed)
		throw std::invalid_argument("Expected a UUID in xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx format.");
	return *parsed;
}

std::string Guid::ToString() const
{
	constexpr char hex[] = "0123456789abcdef";
	std::string text(36, '-');
	std::size_t offset = 0;
	for (const auto byte : bytes)
	{
		if (offset == 8 || offset == 13 || offset == 18 || offset == 23) ++offset;
		text[offset++] = hex[byte >> 4];
		text[offset++] = hex[byte & 0xf];
	}
	return text;
}

bool Guid::IsNull() const noexcept
{
	return std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t byte) { return byte == 0; });
}
