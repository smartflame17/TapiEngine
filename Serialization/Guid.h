#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

// A value type in canonical UUID byte order. A default Guid is the null UUID;
// identities must be created explicitly with Generate() or restored from text.
class Guid
{
public:
	Guid() noexcept = default;
	static Guid Generate();
	static Guid FromString(std::string_view text);
	static std::optional<Guid> TryParse(std::string_view text) noexcept;
	std::string ToString() const;
	bool IsNull() const noexcept;
	const std::array<std::uint8_t, 16>& GetBytes() const noexcept { return bytes; }

	friend bool operator==(const Guid& lhs, const Guid& rhs) noexcept { return lhs.bytes == rhs.bytes; }
	friend bool operator!=(const Guid& lhs, const Guid& rhs) noexcept { return !(lhs == rhs); }

private:
	std::array<std::uint8_t, 16> bytes{};
};

template<>
struct std::hash<Guid>
{
	std::size_t operator()(const Guid& value) const noexcept
	{
		// Hash every byte; GUIDs are never truncated to a numeric identity.
		std::size_t hash = 0;
		for (const auto byte : value.GetBytes())
			hash ^= static_cast<std::size_t>(byte) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		return hash;
	}
};
