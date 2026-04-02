#pragma once
#include "IBindable.h"

class ShadowMap : public IBindable
{
public:
	ShadowMap(Graphics& gfx, UINT size, UINT slot = 2u);
	void Bind(Graphics& gfx) noexcept override;
	void BeginWrite(Graphics& gfx) noexcept;
	void Unbind(Graphics& gfx) noexcept;

private:
	UINT size = 0u;
	UINT slot = 0u;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDepthStencilView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pShaderResourceView;
};
