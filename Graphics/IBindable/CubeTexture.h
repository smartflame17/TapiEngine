#pragma once
#include "IBindable.h"
#include <filesystem>

class CubeTexture : public IBindable
{
public:
	CubeTexture(Graphics& gfx, const std::filesystem::path& directory, UINT slot = 0u);
	void Bind(Graphics& gfx) noexcept override;

private:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pTextureView;
	UINT slot = 0u;
};
