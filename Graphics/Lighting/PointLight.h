#pragma once
#include "../Graphics.h"
#include "../IBindable/ConstantBuffers.h"
#include "../Drawable/SolidSphere.h"
#include "../../Components/Component.h"
#include "../../imgui/imgui.h"

class PointLight : public Component
{
public:
	PointLight(Graphics& gfx, float radius = 0.5f);
	void SpawnControlWindow() noexcept;	// ImGui window for editing light properties
	void Reset() noexcept;
	void Draw(Graphics& gfx) const noexcept(!IS_DEBUG);
	void Bind(Graphics& gfx) const noexcept;
private:
	struct PointLightCbuf
	{
		DirectX::XMFLOAT3 pos;
		float diffuseIntensity = 1.0f;
		DirectX::XMFLOAT3 ambient;
		float attConst = 1.0f;
		DirectX::XMFLOAT3 diffuseColor;
		float attLinear = 0.045f;
		float attQuad = 0.0075f;
		DirectX::XMFLOAT3 padding;
	};
private:
	DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
	PointLightCbuf lightData = {
		{ 0.0f, 0.0f, 0.0f },
		1.0f,
		{ 0.6f, 0.6f, 0.6f },
		1.0f,
		{ 1.0f, 1.0f, 1.0f },
		0.045f,
		0.0075f,
		{ 0.0f, 0.0f, 0.0f }
	};
	mutable PixelConstantBuffer<PointLightCbuf> cbuf;
	mutable SolidSphere mesh;
};
