#include "ShadowMap.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

ShadowMap::ShadowMap(Graphics& gfx, UINT size, UINT slot, Type type)
	:
	size(size),
	slot(slot),
	type(type)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = size;
	textureDesc.Height = size;
	textureDesc.MipLevels = 1u;
	textureDesc.ArraySize = type == Type::TextureCube ? 6u : 1u;
	textureDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	textureDesc.SampleDesc.Count = 1u;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.MiscFlags = type == Type::TextureCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0u;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateTexture2D(&textureDesc, nullptr, &pTexture));

	// For point light mapping, 6 faces as cube map depth view
	if (type == Type::TextureCube)
	{
		pDepthStencilViews.reserve(6u);
		for (UINT face = 0u; face < 6u; face++)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.MipSlice = 0u;
			dsvDesc.Texture2DArray.FirstArraySlice = face;
			dsvDesc.Texture2DArray.ArraySize = 1u;

			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDepthStencilView;
			GFX_THROW_FAILED(GetDevice(gfx)->CreateDepthStencilView(pTexture.Get(), &dsvDesc, &pDepthStencilView));
			pDepthStencilViews.push_back(std::move(pDepthStencilView));
		}
	}
	else // For directional light mapping, single 2D depth view
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0u;

		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDepthStencilView;
		GFX_THROW_FAILED(GetDevice(gfx)->CreateDepthStencilView(pTexture.Get(), &dsvDesc, &pDepthStencilView));
		pDepthStencilViews.push_back(std::move(pDepthStencilView));
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	if (type == Type::TextureCube)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0u;
		srvDesc.TextureCube.MipLevels = 1u;
	}
	else
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0u;
		srvDesc.Texture2D.MipLevels = 1u;
	}
	GFX_THROW_FAILED(GetDevice(gfx)->CreateShaderResourceView(pTexture.Get(), &srvDesc, &pShaderResourceView));
}

void ShadowMap::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->PSSetShaderResources(slot, 1u, pShaderResourceView.GetAddressOf());
}

void ShadowMap::BeginWrite(Graphics& gfx) noexcept
{
	if (!pDepthStencilViews.empty())
	{
		BeginWrite(gfx, pDepthStencilViews.front().Get());
	}
}

void ShadowMap::BeginWriteFace(Graphics& gfx, UINT face) noexcept
{
	if (face < pDepthStencilViews.size())
	{
		BeginWrite(gfx, pDepthStencilViews[face].Get());
	}
}

void ShadowMap::Unbind(Graphics& gfx) noexcept
{
	ID3D11ShaderResourceView* const nullSrv = nullptr;
	GetContext(gfx)->PSSetShaderResources(slot, 1u, &nullSrv);
}

void ShadowMap::BeginWrite(Graphics& gfx, ID3D11DepthStencilView* pTargetView) noexcept
{
	ID3D11ShaderResourceView* const nullSrv = nullptr;
	GetContext(gfx)->PSSetShaderResources(slot, 1u, &nullSrv);
	GetContext(gfx)->OMSetRenderTargets(0u, nullptr, pTargetView);
	GetContext(gfx)->ClearDepthStencilView(pTargetView, D3D11_CLEAR_DEPTH, 1.0f, 0u);
	gfx.SetViewport(static_cast<float>(size), static_cast<float>(size));
}
