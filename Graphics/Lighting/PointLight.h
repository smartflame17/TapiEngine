#pragma once
#include "../Graphics.h"
#include "../IBindable/ConstantBuffers.h"
#include "../Drawable/SolidSphere.h"

class PointLight
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
		float padding;	// align to 16 bytes
	};
private:
	DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
	mutable PixelConstantBuffer<PointLightCbuf> cbuf;
	mutable SolidSphere mesh;
};