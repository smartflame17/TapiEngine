#include "Texture.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"
#include <WICTextureLoader.h> // Provided by DirectXTK

Texture::Texture(Graphics& gfx, const std::wstring& path)
{
	HRESULT hr;
	// CreateWICTextureFromFile handles creating the Texture2D and the ResourceView
	GFX_THROW_FAILED(DirectX::CreateWICTextureFromFile(
		GetDevice(gfx),
		GetContext(gfx),
		path.c_str(),
		nullptr,
		&pTextureView
	));
}

void Texture::Bind(Graphics& gfx) noexcept
{
	// Bind to slot 0 of the Pixel Shader
	GetContext(gfx)->PSSetShaderResources(0u, 1u, pTextureView.GetAddressOf());
}