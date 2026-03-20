#include "DepthStencilState.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

DepthStencilState::DepthStencilState(Graphics& gfx, bool depthEnable, D3D11_DEPTH_WRITE_MASK writeMask, D3D11_COMPARISON_FUNC depthFunc)
{
	HRESULT hr;
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = depthEnable ? TRUE : FALSE;
	dsDesc.DepthWriteMask = writeMask;
	dsDesc.DepthFunc = depthFunc;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateDepthStencilState(&dsDesc, &pState));
}

void DepthStencilState::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->OMSetDepthStencilState(pState.Get(), stencilRef);
}
