#include "Texture.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"
#include <WICTextureLoader.h>

Texture::Texture(Graphics& gfx, const std::wstring& path, UINT slot)
	:
	slot(slot)
{
	SetPath(gfx, std::filesystem::path(path));
}

Texture::Texture(Graphics& gfx, UINT slot)
	:
	slot(slot)
{
	LoadFallback(gfx);
}

void Texture::LoadFromFile(Graphics& gfx, const std::wstring& path)
{
	HRESULT hr;
	GFX_THROW_FAILED(DirectX::CreateWICTextureFromFileEx(
		GetDevice(gfx),
		GetContext(gfx),
		path.c_str(),
		0u,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
		0u,
		0u,
		DirectX::WIC_LOADER_FORCE_RGBA32,
		nullptr,
		&pTextureView
	));
}

// Pink-and-black checkerboard pattern. Should be obvious if it's being used.
void Texture::LoadFallback(Graphics& gfx)
{
	HRESULT hr;

	constexpr unsigned int texWidth = 2u;
	constexpr unsigned int texHeight = 2u;
	constexpr unsigned int pink = 0xFFFF00FFu;
	constexpr unsigned int black = 0xFF000000u;
	const unsigned int pixelData[texWidth * texHeight] =
	{
		pink, black,
		black, pink
	};

	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = texWidth;
	textureDesc.Height = texHeight;
	textureDesc.MipLevels = 1u;
	textureDesc.ArraySize = 1u;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1u;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pSysMem = pixelData;
	subresourceData.SysMemPitch = texWidth * sizeof(unsigned int);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateTexture2D(&textureDesc, &subresourceData, &pTexture));
	GFX_THROW_FAILED(GetDevice(gfx)->CreateShaderResourceView(pTexture.Get(), nullptr, &pTextureView));
}

bool Texture::SetPath(Graphics& gfx, const std::filesystem::path& path) noexcept
{
	requestedPath = path;
	try
	{
		if (path.empty())
		{
			LoadFallback(gfx);
			usingFallback = true;
			return false;
		}

		LoadFromFile(gfx, path.wstring());
		usingFallback = false;
		return true;
	}
	catch (...)
	{
		try
		{
			LoadFallback(gfx);
		}
		catch (...)
		{
		}
		usingFallback = true;
		return false;
	}
}

void Texture::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->PSSetShaderResources(slot, 1u, pTextureView.GetAddressOf());
}

const std::filesystem::path& Texture::GetRequestedPath() const noexcept
{
	return requestedPath;
}

bool Texture::IsUsingFallback() const noexcept
{
	return usingFallback;
}
