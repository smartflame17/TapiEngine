#pragma once
#include "IBindable.h"

class InputLayout : public IBindable
{
public:
	InputLayout(Graphics& gfx,
		const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout,
		ID3DBlob* pVertexShaderBytecode);
	void Bind(Graphics& gfx) noexcept override;
	void BindForShader(Graphics& gfx, ID3DBlob* pVertexShaderBytecode) noexcept(!IS_DEBUG);
protected:
	Microsoft::WRL::ComPtr<ID3D11InputLayout> pInputLayout;
	std::vector<D3D11_INPUT_ELEMENT_DESC> layoutDesc;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> pAlternateInputLayout;
	ID3DBlob* pAlternateBytecode = nullptr;
};
