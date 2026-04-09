#pragma once
#include "IBindable.h"

class RasterizerState : public IBindable
{
public:
	RasterizerState(Graphics& gfx, D3D11_CULL_MODE cullMode, bool frontCounterClockwise = false);
	explicit RasterizerState(Graphics& gfx, const D3D11_RASTERIZER_DESC& desc);
	void Bind(Graphics& gfx) noexcept override;

private:
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> pState;
};
