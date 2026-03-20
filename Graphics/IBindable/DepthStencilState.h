#pragma once
#include "IBindable.h"

class DepthStencilState : public IBindable
{
public:
	DepthStencilState(Graphics& gfx, bool depthEnable, D3D11_DEPTH_WRITE_MASK writeMask, D3D11_COMPARISON_FUNC depthFunc);
	void Bind(Graphics& gfx) noexcept override;

private:
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pState;
	UINT stencilRef = 1u;
};
