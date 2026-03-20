#include "CubeTexture.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"
#include <WICTextureLoader.h>
#include <array>
#include <string>
#include <stdexcept>

namespace
{
	std::filesystem::path ResolveFacePath(const std::filesystem::path& directory, int faceIndex)
	{
		const auto stem = std::to_wstring(faceIndex);
		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			if (entry.is_regular_file() && entry.path().stem().wstring() == stem)
			{
				return entry.path();
			}
		}
		throw std::runtime_error("Missing cubemap face: " + std::to_string(faceIndex) + " in " + directory.string());
	}
}

CubeTexture::CubeTexture(Graphics& gfx, const std::filesystem::path& directory, UINT slot)
	:
	slot(slot)
{
	HRESULT hr;
	std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>, 6> faceTextures;
	D3D11_TEXTURE2D_DESC sourceDesc = {};

	for (int faceIndex = 0; faceIndex < 6; faceIndex++)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> pFaceResource;
		const auto facePath = ResolveFacePath(directory, faceIndex);
		GFX_THROW_FAILED(DirectX::CreateWICTextureFromFileEx(
			GetDevice(gfx),
			nullptr,
			facePath.c_str(),
			0u,
			D3D11_USAGE_DEFAULT,
			0u,
			0u,
			0u,
			DirectX::WIC_LOADER_FORCE_RGBA32,
			&pFaceResource,
			nullptr
		));
		GFX_THROW_FAILED(pFaceResource.As(&faceTextures[faceIndex]));

		D3D11_TEXTURE2D_DESC currentDesc = {};
		faceTextures[faceIndex]->GetDesc(&currentDesc);
		if (faceIndex == 0)
		{
			sourceDesc = currentDesc;
		}
		else
		{
			assert(currentDesc.Width == sourceDesc.Width);
			assert(currentDesc.Height == sourceDesc.Height);
			assert(currentDesc.MipLevels == sourceDesc.MipLevels);
			assert(currentDesc.Format == sourceDesc.Format);
		}
	}

	D3D11_TEXTURE2D_DESC cubeDesc = sourceDesc;
	cubeDesc.ArraySize = 6u;
	cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> pCubeTexture;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateTexture2D(&cubeDesc, nullptr, &pCubeTexture));

	for (UINT faceIndex = 0; faceIndex < 6u; faceIndex++)
	{
		for (UINT mipIndex = 0; mipIndex < cubeDesc.MipLevels; mipIndex++)
		{
			GetContext(gfx)->CopySubresourceRegion(
				pCubeTexture.Get(),
				D3D11CalcSubresource(mipIndex, faceIndex, cubeDesc.MipLevels),
				0u, 0u, 0u,
				faceTextures[faceIndex].Get(),
				mipIndex,
				nullptr
			);
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0u;
	srvDesc.TextureCube.MipLevels = cubeDesc.MipLevels;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateShaderResourceView(pCubeTexture.Get(), &srvDesc, &pTextureView));
}

void CubeTexture::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->PSSetShaderResources(slot, 1u, pTextureView.GetAddressOf());
}
