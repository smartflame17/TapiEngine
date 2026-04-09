#include "BlendState.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

BlendState::BlendState(Graphics& gfx, const D3D11_BLEND_DESC& desc)
{
	HRESULT hr;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateBlendState(&desc, &pState));
}

void BlendState::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->OMSetBlendState(pState.Get(), blendFactor, sampleMask);
}
