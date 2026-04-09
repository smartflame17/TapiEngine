#pragma once

#include <DirectXMath.h>
#include <cstdint>

class DirectionalLight;

enum class LightType : std::uint32_t
{
	None = 0u,
	Directional = 1u,
	Point = 2u,
	Spot = 3u
};

struct RenderLight
{
	LightType type = LightType::None;
	DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
	float intensity = 1.0f;
	DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
	float attConst = 1.0f;
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	float attLinear = 0.045f;
	float attQuad = 0.0075f;
	float innerConeCos = 0.9659258f;
	float outerConeCos = 0.9063078f;
	std::uint32_t enabled = 1u;
	DirectX::XMFLOAT3 padding = { 0.0f, 0.0f, 0.0f };
	const DirectionalLight* pDirectionalLight = nullptr;
};
