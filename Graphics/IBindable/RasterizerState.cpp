#include "RasterizerState.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

RasterizerState::RasterizerState(Graphics& gfx, D3D11_CULL_MODE cullMode, bool frontCounterClockwise)
{
	HRESULT hr;
	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = cullMode;
	rasterDesc.FrontCounterClockwise = frontCounterClockwise ? TRUE : FALSE;
	rasterDesc.DepthClipEnable = TRUE;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateRasterizerState(&rasterDesc, &pState));
}

void RasterizerState::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->RSSetState(pState.Get());
}
