#include "Sampler.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

Sampler::Sampler(Graphics& gfx)
{
	HRESULT hr;

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // Linear filtering for smooth look
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;   // Wrap texture if coord > 1.0
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	// Default comparison function (never typically used for standard textures)
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	GFX_THROW_FAILED(GetDevice(gfx)->CreateSamplerState(&samplerDesc, &pSampler));
}

void Sampler::Bind(Graphics& gfx) noexcept
{
	// Bind to slot 0 of the Pixel Shader
	GetContext(gfx)->PSSetSamplers(0u, 1u, pSampler.GetAddressOf());
}