#include "Texture.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"
#include <WICTextureLoader.h> // Provided by DirectXTK

Texture::Texture(Graphics& gfx, const std::wstring& path, UINT slot)
	:
	slot(slot)
{
	HRESULT hr;
	// Force WIC textures to RGBA so grayscale images don't sample as red-only in the shader.
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

void Texture::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->PSSetShaderResources(slot, 1u, pTextureView.GetAddressOf());
}
