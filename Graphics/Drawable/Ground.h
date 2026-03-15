#pragma once

#include "DrawableBase.h"
#include "../IBindable/IBindableBase.h"
#include "Plane.h"

class Ground : public DrawableBase<Ground>
{
public:
	Ground(Graphics& gfx, int divisionsX = 64, int divisionsY = 64, float scale = 64.0f);
	DirectX::XMMATRIX GetTransformXM() const noexcept override;

private:
	float scale;
};
