#pragma once
#include "DrawableBase.h"

class SolidSphere : public DrawableBase<SolidSphere>
{
public:
	SolidSphere(Graphics& gfx, float radius = 1.0f);
	DirectX::XMMATRIX GetTransformXM() const noexcept override;
	void Update(float dt) noexcept override;
	void SetPos(DirectX::XMFLOAT3 pos) noexcept;
private:
	DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
};