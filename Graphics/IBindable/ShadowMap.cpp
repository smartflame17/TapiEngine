#include "ShadowMap.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

ShadowMap::ShadowMap(Graphics& gfx, UINT size, UINT slot)
	:
	size(size),
	slot(slot)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = size;
	textureDesc.Height = size;
	textureDesc.MipLevels = 1u;
	textureDesc.ArraySize = 1u;
	textureDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	textureDesc.SampleDesc.Count = 1u;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateTexture2D(&textureDesc, nullptr, &pTexture));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0u;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateDepthStencilView(pTexture.Get(), &dsvDesc, &pDepthStencilView));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0u;
	srvDesc.Texture2D.MipLevels = 1u;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateShaderResourceView(pTexture.Get(), &srvDesc, &pShaderResourceView));
}

void ShadowMap::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->PSSetShaderResources(slot, 1u, pShaderResourceView.GetAddressOf());
}

void ShadowMap::BeginWrite(Graphics& gfx) noexcept
{
	ID3D11ShaderResourceView* const nullSrv = nullptr;
	GetContext(gfx)->PSSetShaderResources(slot, 1u, &nullSrv);
	GetContext(gfx)->OMSetRenderTargets(0u, nullptr, pDepthStencilView.Get());
	GetContext(gfx)->ClearDepthStencilView(pDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0u);
	gfx.SetViewport(static_cast<float>(size), static_cast<float>(size));
}

void ShadowMap::Unbind(Graphics& gfx) noexcept
{
	ID3D11ShaderResourceView* const nullSrv = nullptr;
	GetContext(gfx)->PSSetShaderResources(slot, 1u, &nullSrv);
}
