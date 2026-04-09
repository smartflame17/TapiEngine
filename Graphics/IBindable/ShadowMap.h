#pragma once
#include "IBindable.h"

class ShadowMap : public IBindable
{
public:
	enum class Type
	{
		Texture2D,
		TextureCube
	};

	ShadowMap(Graphics& gfx, UINT size, UINT slot = 2u, Type type = Type::Texture2D);
	void Bind(Graphics& gfx) noexcept override;
	void BeginWrite(Graphics& gfx) noexcept;
	void BeginWriteFace(Graphics& gfx, UINT face) noexcept;
	void Unbind(Graphics& gfx) noexcept;

private:
	void BeginWrite(Graphics& gfx, ID3D11DepthStencilView* pTargetView) noexcept;

	UINT size = 0u;
	UINT slot = 0u;
	Type type = Type::Texture2D;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
	std::vector<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>> pDepthStencilViews;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pShaderResourceView;
};
