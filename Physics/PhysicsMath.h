#pragma once

#include <DirectXMath.h>
#include <box3d/math_functions.h>
#include <cstdint>
#include <cstring>

namespace PhysicsMath
{
// The engine builds with /fp:fast; inspect the exponent so NaN/Inf checks survive optimization.
inline bool IsFinite(float value) noexcept
{
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	return (bits & 0x7f800000u) != 0x7f800000u;
}
inline bool IsFinite(const DirectX::XMFLOAT3& v) noexcept
{
	return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z);
}
inline bool IsPositive(const DirectX::XMFLOAT3& v) noexcept
{
	return IsFinite(v) && v.x > 0 && v.y > 0 && v.z > 0;
}
inline b3Vec3 Vector(const DirectX::XMFLOAT3& v) noexcept { return { v.x, v.y, v.z }; }
inline b3Quat Quaternion(const DirectX::XMFLOAT4& q) noexcept { return { { q.x, q.y, q.z }, q.w }; }
inline constexpr const char* SuspendedWarning =
	"Physics suspended: use finite transforms, positive object scale, and positive uniform scale on all ancestors.";
}
