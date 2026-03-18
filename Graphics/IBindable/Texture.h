#pragma once
#include "IBindable.h"

class Texture : public IBindable
{
public:
	// Loads image from file path using DirectXTK
	Texture(Graphics& gfx, const std::wstring& path, UINT slot = 0u);
	void Bind(Graphics& gfx) noexcept override;
protected:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pTextureView;
	UINT slot = 0u;
};
