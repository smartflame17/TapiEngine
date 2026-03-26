#pragma once
#include <DirectXMath.h>
#include <cstdint>

struct PhongMaterial
{
	DirectX::XMFLOAT3 color = { 0.8f, 0.8f, 0.8f };
	float specularIntensity = 0.5f;
	float specularPower = 32.0f;
	DirectX::XMFLOAT3 specularColor = { 1.0f, 1.0f, 1.0f };
	std::uint32_t useNormalMap = 0u;
	std::uint32_t padding0 = 0u;
	DirectX::XMFLOAT2 padding1 = { 0.0f, 0.0f };
};
