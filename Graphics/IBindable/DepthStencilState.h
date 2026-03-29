#pragma once
#include "IBindable.h"

class DepthStencilState : public IBindable
{
public:
	DepthStencilState(Graphics& gfx, bool depthEnable, D3D11_DEPTH_WRITE_MASK writeMask, D3D11_COMPARISON_FUNC depthFunc);
	explicit DepthStencilState(Graphics& gfx, const D3D11_DEPTH_STENCIL_DESC& desc);
	void Bind(Graphics& gfx) noexcept override;

private:
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pState;
	UINT stencilRef = 1u;
};
