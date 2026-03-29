#pragma once
#include "IBindable.h"

class BlendState : public IBindable
{
public:
	explicit BlendState(Graphics& gfx, const D3D11_BLEND_DESC& desc);
	void Bind(Graphics& gfx) noexcept override;

private:
	Microsoft::WRL::ComPtr<ID3D11BlendState> pState;
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	UINT sampleMask = 0xffffffffu;
};
